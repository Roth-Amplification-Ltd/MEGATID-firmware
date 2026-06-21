#include "engine.h"
#include "io.h"
#include "clock.h"
#include "waveforms.h"
#include "megatide_config.h"

#include <util/atomic.h>
#include <stdint.h>
#include <string.h>

/*
 * Real-time ownership model
 * -------------------------
 *
 * Timer2 overflow is the only code path that mutates phase, envelope, random,
 * and gate state. The main loop fills an inactive render buffer, then atomically swaps an
 * 8-bit index. This makes multi-byte values deterministic on an 8-bit CPU.
 */

static mt_render_config_t g_render_config[2];
static volatile uint8_t g_active_config_index;
static mt_engine_state_t g_engine_state[MT_ENGINE_COUNT];
static uint32_t g_gate_phase[MT_OUTPUT_COUNT];
static uint32_t g_prng_state;

static volatile uint8_t g_trigger_pending;
static volatile uint8_t g_external_sync_pending;
static volatile uint8_t g_control_update_due;
static volatile uint8_t g_control_divider;
static volatile uint8_t g_status;

static uint16_t pseudo_random_u16(void)
{
    /* Xorshift32: compact, deterministic, and sufficient for modulation noise. */
    uint32_t x = g_prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_prng_state = x;
    return (uint16_t)(x >> 16);
}

static void initialize_engine_state(mt_engine_state_t *state)
{
    state->phase = 0u;
    state->random_start = pseudo_random_u16();
    state->random_target = pseudo_random_u16();
    state->envelope_level_q16 = 0u;
    state->envelope_state = MT_ENV_IDLE;
    state->last_sample = 32768u;
}

static bool advance_phase(uint8_t engine_index, uint32_t increment)
{
    mt_engine_state_t *state = &g_engine_state[engine_index];
    uint32_t old_phase = state->phase;
    state->phase += increment;

    if (state->phase < old_phase)
    {
        state->random_start = state->random_target;
        state->random_target = pseudo_random_u16();
        return true;
    }

    return false;
}

static uint32_t apply_cross_modulation(
    uint32_t base_increment,
    uint16_t amount,
    uint16_t modulator_sample)
{
    /*
     * The rate modulation calculation intentionally works in 20-bit-like units
     * by discarding the lowest twelve phase-increment bits. This keeps the
     * signed multiplication inside 32 bits while retaining more than enough
     * resolution for LFO work.
     */
    int16_t bipolar = (int16_t)((int16_t)(modulator_sample >> 8) - 128);
    int32_t factor = (int32_t)bipolar * (int32_t)(amount >> 8);
    int32_t base_units = (int32_t)(base_increment >> 12);
    int32_t delta_units = (base_units * factor) >> 15;
    int32_t result_units = base_units + delta_units;

    if (result_units < 1)
    {
        result_units = 1;
    }

    return (uint32_t)result_units << 12;
}

static void service_envelope(
    uint8_t engine_index,
    uint32_t attack_step,
    uint32_t decay_step)
{
    mt_engine_state_t *state = &g_engine_state[engine_index];

    if (state->envelope_state == MT_ENV_ATTACK)
    {
        uint32_t previous = state->envelope_level_q16;
        state->envelope_level_q16 += attack_step;

        if ((state->envelope_level_q16 < previous)
            || (state->envelope_level_q16 >= 0xFFFF0000UL))
        {
            state->envelope_level_q16 = 0xFFFF0000UL;
            state->envelope_state = MT_ENV_DECAY;
        }
    }
    else if (state->envelope_state == MT_ENV_DECAY)
    {
        if (state->envelope_level_q16 <= decay_step)
        {
            state->envelope_level_q16 = 0u;
            state->envelope_state = MT_ENV_IDLE;
        }
        else
        {
            state->envelope_level_q16 -= decay_step;
        }
    }
}

static uint16_t clamp_u16_from_i32(int32_t value)
{
    if (value < 0)
    {
        return 0u;
    }
    if (value > 65535)
    {
        return 65535u;
    }
    return (uint16_t)value;
}

static uint16_t route_output(
    uint8_t route,
    uint16_t sample_a,
    uint16_t sample_b,
    const mt_engine_config_t *config_a,
    const mt_engine_config_t *config_b,
    uint32_t phase_a,
    uint32_t phase_b)
{
    switch ((uint8_t)(route % MT_ROUTE_COUNT))
    {
        case MT_ROUTE_ENGINE_A:
            return sample_a;

        case MT_ROUTE_ENGINE_B:
            return sample_b;

        case MT_ROUTE_MIX:
            return (uint16_t)(((uint32_t)sample_a + sample_b) >> 1);

        case MT_ROUTE_DIFFERENCE:
            return clamp_u16_from_i32((int32_t)sample_a - sample_b + 32768);

        case MT_ROUTE_ENGINE_A_QUADRATURE:
        {
            uint16_t raw = mt_waveform_render(
                &g_engine_state[0], config_a, phase_a + 0x40000000UL);
            return mt_waveform_apply_level(raw, config_a->depth, config_a->offset);
        }

        case MT_ROUTE_ENGINE_B_QUADRATURE:
        {
            uint16_t raw = mt_waveform_render(
                &g_engine_state[1], config_b, phase_b + 0x40000000UL);
            return mt_waveform_apply_level(raw, config_b->depth, config_b->offset);
        }

        case MT_ROUTE_ENGINE_A_GATE:
            return (sample_a >= 32768u) ? 65535u : 0u;

        case MT_ROUTE_ENGINE_B_GATE:
            return (sample_b >= 32768u) ? 65535u : 0u;

        case MT_ROUTE_ZERO:
            return 0u;

        case MT_ROUTE_FULL:
            return 65535u;

        default:
            return 32768u;
    }
}

static void render_dual_engine_mode(
    const mt_render_config_t *config,
    uint16_t output16[MT_OUTPUT_COUNT])
{
    mt_engine_config_t config_a = config->engine[0];
    mt_engine_config_t config_b = config->engine[1];

    uint32_t increment_a = config_a.phase_increment;
    uint32_t increment_b = config_b.phase_increment;

    if ((config->mode == MT_MODE_CROSS_MOD) || (config->mode == MT_MODE_REGISTER))
    {
        increment_a = apply_cross_modulation(
            increment_a,
            config->cross_b_to_a,
            g_engine_state[1].last_sample);
        increment_b = apply_cross_modulation(
            increment_b,
            config->cross_a_to_b,
            g_engine_state[0].last_sample);
    }

    bool wrapped_a = advance_phase(0u, increment_a);
    bool wrapped_b = advance_phase(1u, increment_b);

    if (config->mode == MT_MODE_RANDOM)
    {
        /* Correlate B's newly selected target toward A's target when requested. */
        if (wrapped_b)
        {
            uint32_t independent = g_engine_state[1].random_target;
            uint32_t shared = g_engine_state[0].random_target;
            uint32_t correlation = config->random_correlation;
            g_engine_state[1].random_target = (uint16_t)(
                ((independent * (65535u - correlation))
                + (shared * correlation)) >> 16);
        }

        /* If A did not wrap this tick, wrapped_a is intentionally unused. */
        (void)wrapped_a;
    }

    uint32_t phase_a = g_engine_state[0].phase;
    uint32_t phase_b = g_engine_state[1].phase + config->phase_offset_b;

    uint16_t raw_a = mt_waveform_render(&g_engine_state[0], &config_a, phase_a);
    uint16_t raw_b = mt_waveform_render(&g_engine_state[1], &config_b, phase_b);

    g_engine_state[0].last_sample = raw_a;
    g_engine_state[1].last_sample = raw_b;

    uint16_t sample_a = mt_waveform_apply_level(raw_a, config_a.depth, config_a.offset);
    uint16_t sample_b = mt_waveform_apply_level(raw_b, config_b.depth, config_b.offset);

    for (uint8_t i = 0u; i < MT_OUTPUT_COUNT; ++i)
    {
        output16[i] = route_output(
            config->output_route[i],
            sample_a,
            sample_b,
            &config_a,
            &config_b,
            phase_a,
            phase_b);
    }
}

static void render_polyphase_mode(
    const mt_render_config_t *config,
    uint16_t output16[MT_OUTPUT_COUNT])
{
    mt_engine_config_t engine = config->engine[0];
    advance_phase(0u, engine.phase_increment);

    uint32_t phase = g_engine_state[0].phase;
    uint32_t spacing = (uint32_t)config->polyphase_spacing << 16;

    for (uint8_t i = 0u; i < MT_OUTPUT_COUNT; ++i)
    {
        uint16_t raw = mt_waveform_render(
            &g_engine_state[0],
            &engine,
            phase + spacing * i);
        output16[i] = mt_waveform_apply_level(raw, engine.depth, engine.offset);
    }

    g_engine_state[0].last_sample = output16[0];
}

static void render_envelope_mode(
    const mt_render_config_t *config,
    uint16_t output16[MT_OUTPUT_COUNT])
{
    service_envelope(0u, config->envelope_attack_step[0], config->envelope_decay_step[0]);
    service_envelope(1u, config->envelope_attack_step[1], config->envelope_decay_step[1]);

    uint16_t linear_a = (uint16_t)(g_engine_state[0].envelope_level_q16 >> 16);
    uint16_t linear_b = (uint16_t)(g_engine_state[1].envelope_level_q16 >> 16);

    uint16_t sample_a = mt_waveform_shape_envelope(linear_a, config->envelope_curve[0]);
    uint16_t sample_b = mt_waveform_shape_envelope(linear_b, config->envelope_curve[1]);

    output16[0] = sample_a;
    output16[1] = sample_b;
    output16[2] = (uint16_t)(((uint32_t)sample_a + sample_b) >> 1);
    output16[3] = ((g_engine_state[0].envelope_state != MT_ENV_IDLE)
        || (g_engine_state[1].envelope_state != MT_ENV_IDLE)) ? 65535u : 0u;
}

static void render_clock_gate_mode(
    const mt_render_config_t *config,
    uint16_t output16[MT_OUTPUT_COUNT])
{
    for (uint8_t i = 0u; i < MT_OUTPUT_COUNT; ++i)
    {
        uint32_t increment = ((g_gate_phase[i] & 0x80000000UL) == 0u)
            ? config->gate_increment_first[i]
            : config->gate_increment_second[i];

        g_gate_phase[i] += increment;
        output16[i] = ((uint16_t)(g_gate_phase[i] >> 16) < config->gate_width)
            ? 65535u
            : 0u;
    }
}

void mt_engine_init(void)
{
    g_prng_state = 0x7A5B3C1DUL;
    g_active_config_index = 0u;
    memset(&g_render_config[0], 0, sizeof(g_render_config));
    g_trigger_pending = 0u;
    g_external_sync_pending = 0u;
    g_control_update_due = 0u;
    g_control_divider = 0u;
    g_status = 0u;

    for (uint8_t i = 0u; i < MT_ENGINE_COUNT; ++i)
    {
        initialize_engine_state(&g_engine_state[i]);
    }
    for (uint8_t i = 0u; i < MT_OUTPUT_COUNT; ++i)
    {
        g_gate_phase[i] = 0u;
    }

    mt_render_config_t initial;
    memset(&initial, 0, sizeof(initial));
    initial.mode = MT_MODE_DUAL_LFO;
    initial.outputs_enabled = 1u;
    initial.trigger_resets_phase = 1u;
    initial.engine[0].waveform = MT_WAVE_SINE;
    initial.engine[1].waveform = MT_WAVE_TRIANGLE;
    initial.engine[0].depth = 65535u;
    initial.engine[1].depth = 65535u;
    initial.output_route[0] = MT_ROUTE_ENGINE_A;
    initial.output_route[1] = MT_ROUTE_ENGINE_B;
    initial.output_route[2] = MT_ROUTE_MIX;
    initial.output_route[3] = MT_ROUTE_ENGINE_A_GATE;
    mt_engine_commit_config(&initial);
}

void mt_engine_commit_config(const mt_render_config_t *config)
{
    /*
     * Double buffering keeps the 7.8125 kHz ISR from copying a large structure
     * every tick. The foreground writes only the inactive slot, then changes a
     * single 8-bit index with interrupts briefly disabled. The ISR therefore
     * sees either the complete old configuration or the complete new one.
     */
    uint8_t inactive = (uint8_t)(g_active_config_index ^ 1u);
    memcpy(&g_render_config[inactive], config, sizeof(*config));

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        g_active_config_index = inactive;
    }
}

void mt_engine_trigger_isr(uint8_t trigger_mask)
{
    g_trigger_pending |= (uint8_t)(trigger_mask & 0x03u);
}

void mt_engine_external_clock_edge_isr(void)
{
    g_external_sync_pending = 1u;
}

void mt_engine_service_tick_isr(void)
{
    mt_clock_service_tick_isr();

    g_control_divider++;
    if (g_control_divider >= MT_CONTROL_DIVIDER)
    {
        g_control_divider = 0u;
        g_control_update_due = 1u;
    }

    /* The active index cannot change while this ISR is executing. */
    const mt_render_config_t *config = &g_render_config[g_active_config_index];

    uint8_t triggers = g_trigger_pending;
    g_trigger_pending = 0u;

    if (triggers != 0u)
    {
        if (config->mode == MT_MODE_ENVELOPE)
        {
            if ((triggers & 0x01u) != 0u)
            {
                g_engine_state[0].envelope_level_q16 = 0u;
                g_engine_state[0].envelope_state = MT_ENV_ATTACK;
            }
            if ((triggers & 0x02u) != 0u)
            {
                g_engine_state[1].envelope_level_q16 = 0u;
                g_engine_state[1].envelope_state = MT_ENV_ATTACK;
            }
        }
        else if (config->trigger_resets_phase != 0u)
        {
            if ((triggers & 0x01u) != 0u)
            {
                g_engine_state[0].phase = 0u;
            }
            if ((triggers & 0x02u) != 0u)
            {
                g_engine_state[1].phase = 0u;
            }
        }
    }

    if (g_external_sync_pending != 0u)
    {
        g_external_sync_pending = 0u;
        if (config->sync_on_external_clock != 0u)
        {
            g_engine_state[0].phase = 0u;
            g_engine_state[1].phase = 0u;
            for (uint8_t i = 0u; i < MT_OUTPUT_COUNT; ++i)
            {
                g_gate_phase[i] = 0u;
            }
        }
    }

    uint16_t output16[MT_OUTPUT_COUNT];

    switch (config->mode)
    {
        case MT_MODE_POLYPHASE:
            render_polyphase_mode(config, output16);
            break;

        case MT_MODE_ENVELOPE:
            render_envelope_mode(config, output16);
            break;

        case MT_MODE_CLOCK_GATE:
            render_clock_gate_mode(config, output16);
            break;

        case MT_MODE_DUAL_LFO:
        case MT_MODE_RATIO_LFO:
        case MT_MODE_RANDOM:
        case MT_MODE_CROSS_MOD:
        case MT_MODE_REGISTER:
        default:
            render_dual_engine_mode(config, output16);
            break;
    }

    uint8_t output8[MT_OUTPUT_COUNT];
    for (uint8_t i = 0u; i < MT_OUTPUT_COUNT; ++i)
    {
        output8[i] = (config->outputs_enabled != 0u)
            ? (uint8_t)(output16[i] >> 8)
            : 0u;
    }
    mt_io_write_pwm_isr(output8);

    uint8_t status = 0u;
    if (g_engine_state[0].envelope_state != MT_ENV_IDLE)
    {
        status |= MT_STATUS_ENVELOPE_A;
    }
    if (g_engine_state[1].envelope_state != MT_ENV_IDLE)
    {
        status |= MT_STATUS_ENVELOPE_B;
    }
    g_status = status;
}

bool mt_engine_take_control_update_due(void)
{
    bool due;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        due = (g_control_update_due != 0u);
        g_control_update_due = 0u;
    }
    return due;
}

uint8_t mt_engine_get_status(void)
{
    return g_status;
}
