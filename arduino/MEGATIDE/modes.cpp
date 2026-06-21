#include "modes.h"
#include "megatide_types.h"
#include "megatide_config.h"
#include "io.h"
#include "adc.h"
#include "clock.h"
#include "engine.h"
#include "tables.h"
#include "config_bus.h"

#include <string.h>
#include <stdint.h>

#define MT_RATIO_COUNT 11u

/* B:A musical ratios used by ratio-linked, clock, and register modes. */
static const uint8_t g_ratio_numerator[MT_RATIO_COUNT] =
    {1u, 1u, 1u, 1u, 2u, 1u, 3u, 2u, 3u, 4u, 8u};
static const uint8_t g_ratio_denominator[MT_RATIO_COUNT] =
    {8u, 4u, 3u, 2u, 3u, 1u, 2u, 1u, 1u, 1u, 1u};

static uint8_t g_active_mode;
static uint8_t g_last_raw_mode;
static uint8_t g_mode_stable_count;

static uint16_t scale_u8_to_u16(uint8_t value)
{
    return (uint16_t)value * 257u;
}

static int16_t offset_u8_to_i16(uint8_t value)
{
    return (int16_t)(((int16_t)value - 128) << 8);
}

static uint8_t parameter_to_waveform(uint16_t value)
{
    uint8_t waveform = (uint8_t)(((uint32_t)value * MT_WAVE_COUNT) >> 16);
    if (waveform >= MT_WAVE_COUNT)
    {
        waveform = MT_WAVE_COUNT - 1u;
    }
    return waveform;
}

static uint8_t parameter_to_ratio_index(uint16_t value)
{
    uint8_t index = (uint8_t)(((uint32_t)value * MT_RATIO_COUNT) >> 16);
    if (index >= MT_RATIO_COUNT)
    {
        index = MT_RATIO_COUNT - 1u;
    }
    return index;
}

static uint32_t apply_ratio(uint32_t increment, uint8_t ratio_index)
{
    ratio_index %= MT_RATIO_COUNT;
    uint8_t numerator = g_ratio_numerator[ratio_index];
    uint8_t denominator = g_ratio_denominator[ratio_index];

    /* Quotient/remainder form avoids overflowing 32 bits. */
    uint32_t quotient = increment / denominator;
    uint32_t remainder = increment % denominator;
    return (quotient * numerator) + ((remainder * numerator) / denominator);
}

static void configure_default_routes(mt_render_config_t *config)
{
    config->output_route[0] = MT_ROUTE_ENGINE_A;
    config->output_route[1] = MT_ROUTE_ENGINE_B;
    config->output_route[2] = MT_ROUTE_MIX;
    config->output_route[3] = MT_ROUTE_ENGINE_A_GATE;
}

static void configure_engine_defaults(mt_render_config_t *config)
{
    memset(config, 0, sizeof(*config));

    config->outputs_enabled = 1u;
    config->trigger_resets_phase = 1u;
    config->engine[0].depth = 65535u;
    config->engine[1].depth = 65535u;
    config->engine[0].waveform = MT_WAVE_SINE;
    config->engine[1].waveform = MT_WAVE_TRIANGLE;
    config->gate_width = 32768u;
    configure_default_routes(config);
}

static uint8_t update_mode_debounce(void)
{
    uint8_t raw = mt_io_read_mode_bits();

    if (raw == g_last_raw_mode)
    {
        if (g_mode_stable_count < 255u)
        {
            g_mode_stable_count++;
        }
    }
    else
    {
        g_last_raw_mode = raw;
        g_mode_stable_count = 0u;
    }

    /* Five 244 Hz control frames provide about 20 ms of switch debounce. */
    if (g_mode_stable_count >= 5u)
    {
        g_active_mode = raw;
    }

    return g_active_mode;
}

static int16_t route_modulation_delta(uint16_t parameter, uint8_t route_byte)
{
    int16_t centered = (int16_t)((int16_t)(parameter >> 8) - 128);
    uint8_t amount_code = (uint8_t)(route_byte >> 5);
    int16_t amount = (int16_t)(amount_code + 1u);

    if ((route_byte & 0x10u) != 0u)
    {
        centered = (int16_t)-centered;
    }

    return (int16_t)((centered * amount) / 8);
}

static uint8_t modulated_register_value(
    const uint8_t registers[MT_PERSISTENT_REGISTER_COUNT],
    const mt_parameter_snapshot_t *parameters,
    uint8_t base_address,
    uint8_t destination)
{
    int16_t value = registers[base_address];

    for (uint8_t i = 0u; i < MT_PARAMETER_COUNT; ++i)
    {
        uint8_t route = registers[MT_REG_PARAM0_ROUTE + i];
        if ((route & 0x0Fu) == destination)
        {
            value += route_modulation_delta(parameters->value[i], route);
        }
    }

    if (value < 0)
    {
        return 0u;
    }
    if (value > 255)
    {
        return 255u;
    }
    return (uint8_t)value;
}

static void configure_mode_dual_lfo(
    mt_render_config_t *config,
    const mt_parameter_snapshot_t *p)
{
    config->mode = MT_MODE_DUAL_LFO;
    config->engine[0].phase_increment = mt_table_rate_increment((uint8_t)(p->value[0] >> 8));
    config->engine[0].waveform = parameter_to_waveform(p->value[1]);
    config->engine[0].shape = p->value[2];

    config->engine[1].phase_increment = mt_table_rate_increment((uint8_t)(p->value[3] >> 8));
    config->engine[1].waveform = parameter_to_waveform(p->value[4]);
    config->engine[1].shape = p->value[5];
}

static void configure_mode_ratio_lfo(
    mt_render_config_t *config,
    const mt_parameter_snapshot_t *p,
    bool external_clock,
    uint32_t external_increment)
{
    config->mode = MT_MODE_RATIO_LFO;
    config->sync_on_external_clock = external_clock ? 1u : 0u;

    uint32_t master = external_clock
        ? external_increment
        : mt_table_rate_increment((uint8_t)(p->value[0] >> 8));

    config->engine[0].phase_increment = master;
    config->engine[1].phase_increment = apply_ratio(
        master,
        parameter_to_ratio_index(p->value[1]));

    config->engine[0].waveform = parameter_to_waveform(p->value[2]);
    config->engine[1].waveform = parameter_to_waveform(p->value[3]);
    config->phase_offset_b = (uint32_t)p->value[4] << 16;
    config->engine[0].shape = p->value[5];
    config->engine[1].shape = p->value[5];
    config->output_route[3] = MT_ROUTE_ENGINE_B_GATE;
}

static void configure_mode_polyphase(
    mt_render_config_t *config,
    const mt_parameter_snapshot_t *p,
    bool external_clock,
    uint32_t external_increment)
{
    config->mode = MT_MODE_POLYPHASE;
    config->sync_on_external_clock = external_clock ? 1u : 0u;
    config->engine[0].phase_increment = external_clock
        ? external_increment
        : mt_table_rate_increment((uint8_t)(p->value[0] >> 8));
    config->engine[0].waveform = parameter_to_waveform(p->value[1]);
    config->engine[0].shape = p->value[2];

    /* 0..180 degrees; midpoint gives the conventional 90-degree spacing. */
    config->polyphase_spacing = (uint16_t)(p->value[3] >> 1);
    config->engine[0].depth = p->value[4];
    config->engine[0].offset = (int16_t)(p->value[5] - 32768u);
}

static void configure_mode_random(
    mt_render_config_t *config,
    const mt_parameter_snapshot_t *p)
{
    config->mode = MT_MODE_RANDOM;

    config->engine[0].phase_increment = mt_table_rate_increment((uint8_t)(p->value[0] >> 8));
    config->engine[0].waveform = (p->value[1] < 2048u)
        ? MT_WAVE_SAMPLE_HOLD
        : MT_WAVE_SLEWED_RANDOM;
    config->engine[0].shape = p->value[1];

    config->engine[1].phase_increment = mt_table_rate_increment((uint8_t)(p->value[2] >> 8));
    config->engine[1].waveform = (p->value[3] < 2048u)
        ? MT_WAVE_SAMPLE_HOLD
        : MT_WAVE_SLEWED_RANDOM;
    config->engine[1].shape = p->value[3];

    config->random_correlation = p->value[4];
    config->engine[0].depth = p->value[5];
    config->engine[1].depth = p->value[5];
}

static void configure_mode_envelope(
    mt_render_config_t *config,
    const mt_parameter_snapshot_t *p)
{
    config->mode = MT_MODE_ENVELOPE;
    config->trigger_resets_phase = 0u;

    config->envelope_attack_step[0] = mt_table_envelope_step((uint8_t)(p->value[0] >> 8));
    config->envelope_decay_step[0] = mt_table_envelope_step((uint8_t)(p->value[1] >> 8));
    config->envelope_curve[0] = (uint8_t)(p->value[2] >> 14);

    config->envelope_attack_step[1] = mt_table_envelope_step((uint8_t)(p->value[3] >> 8));
    config->envelope_decay_step[1] = mt_table_envelope_step((uint8_t)(p->value[4] >> 8));
    config->envelope_curve[1] = (uint8_t)(p->value[5] >> 14);
}

static void configure_gate_increment_pair(
    uint32_t base_increment,
    uint16_t swing_delta,
    uint32_t *first,
    uint32_t *second)
{
    uint32_t denominator_first = 32768u + swing_delta;
    uint32_t denominator_second = 32768u - swing_delta;

    *first = (uint32_t)(((uint64_t)base_increment * 32768u) / denominator_first);
    *second = (uint32_t)(((uint64_t)base_increment * 32768u) / denominator_second);

    if (*first == 0u)
    {
        *first = 1u;
    }
    if (*second == 0u)
    {
        *second = 1u;
    }
}

static void configure_mode_clock_gate(
    mt_render_config_t *config,
    const mt_parameter_snapshot_t *p,
    bool external_clock,
    uint32_t external_increment)
{
    config->mode = MT_MODE_CLOCK_GATE;
    config->sync_on_external_clock = external_clock ? 1u : 0u;

    uint32_t master = external_clock
        ? external_increment
        : mt_table_rate_increment((uint8_t)(p->value[0] >> 8));

    uint8_t ratio_index[MT_OUTPUT_COUNT];
    ratio_index[0] = 5u; /* 1:1 */
    ratio_index[1] = parameter_to_ratio_index(p->value[1]);
    ratio_index[2] = parameter_to_ratio_index(p->value[2]);
    ratio_index[3] = parameter_to_ratio_index(p->value[3]);

    config->gate_width = p->value[4];

    /* Limit swing to 75/25 timing so neither half-cycle denominator reaches zero. */
    uint16_t swing_delta = (uint16_t)(p->value[5] >> 2);

    for (uint8_t i = 0u; i < MT_OUTPUT_COUNT; ++i)
    {
        uint32_t increment = apply_ratio(master, ratio_index[i]);
        configure_gate_increment_pair(
            increment,
            swing_delta,
            &config->gate_increment_first[i],
            &config->gate_increment_second[i]);
    }
}

static void configure_mode_cross_mod(
    mt_render_config_t *config,
    const mt_parameter_snapshot_t *p)
{
    config->mode = MT_MODE_CROSS_MOD;
    config->engine[0].phase_increment = mt_table_rate_increment((uint8_t)(p->value[0] >> 8));
    config->engine[1].phase_increment = mt_table_rate_increment((uint8_t)(p->value[1] >> 8));
    config->cross_a_to_b = p->value[2];
    config->cross_b_to_a = p->value[3];

    uint8_t pair = parameter_to_waveform(p->value[4]);
    config->engine[0].waveform = pair;
    config->engine[1].waveform = (uint8_t)((pair + 3u) % MT_WAVE_COUNT);
    config->engine[0].shape = p->value[5];
    config->engine[1].shape = p->value[5];
    config->output_route[3] = MT_ROUTE_DIFFERENCE;
}

static void configure_mode_register(
    mt_render_config_t *config,
    const mt_parameter_snapshot_t *parameters,
    bool external_clock,
    uint32_t external_increment)
{
    uint8_t r[MT_PERSISTENT_REGISTER_COUNT];
    mt_config_bus_copy_persistent_registers(r);

    config->mode = MT_MODE_REGISTER;

    uint8_t rate_a = modulated_register_value(r, parameters, MT_REG_RATE_A, MT_PARAM_DEST_RATE_A);
    uint8_t rate_b = modulated_register_value(r, parameters, MT_REG_RATE_B, MT_PARAM_DEST_RATE_B);
    uint8_t wave_a = modulated_register_value(r, parameters, MT_REG_WAVE_A, MT_PARAM_DEST_WAVE_A);
    uint8_t wave_b = modulated_register_value(r, parameters, MT_REG_WAVE_B, MT_PARAM_DEST_WAVE_B);
    uint8_t shape_a = modulated_register_value(r, parameters, MT_REG_SHAPE_A, MT_PARAM_DEST_SHAPE_A);
    uint8_t shape_b = modulated_register_value(r, parameters, MT_REG_SHAPE_B, MT_PARAM_DEST_SHAPE_B);
    uint8_t depth_a = modulated_register_value(r, parameters, MT_REG_DEPTH_A, MT_PARAM_DEST_DEPTH_A);
    uint8_t depth_b = modulated_register_value(r, parameters, MT_REG_DEPTH_B, MT_PARAM_DEST_DEPTH_B);
    uint8_t phase_b = modulated_register_value(r, parameters, MT_REG_PHASE_B, MT_PARAM_DEST_PHASE_B);
    uint8_t cross_ab = modulated_register_value(r, parameters, MT_REG_CROSS_A_TO_B, MT_PARAM_DEST_CROSS_A_TO_B);
    uint8_t cross_ba = modulated_register_value(r, parameters, MT_REG_CROSS_B_TO_A, MT_PARAM_DEST_CROSS_B_TO_A);
    uint8_t gate_width = modulated_register_value(r, parameters, MT_REG_GATE_WIDTH, MT_PARAM_DEST_GATE_WIDTH);

    config->outputs_enabled = (r[MT_REG_GLOBAL_CONTROL] & MT_GLOBAL_OUTPUT_ENABLE) != 0u;
    config->trigger_resets_phase = (r[MT_REG_GLOBAL_CONTROL] & MT_GLOBAL_TRIGGER_RESET) != 0u;

    bool use_external = external_clock
        && ((r[MT_REG_GLOBAL_CONTROL] & MT_GLOBAL_EXTERNAL_SYNC) != 0u);
    config->sync_on_external_clock = use_external ? 1u : 0u;

    if (use_external)
    {
        config->engine[0].phase_increment = external_increment;
        config->engine[1].phase_increment = apply_ratio(external_increment, r[MT_REG_RATIO]);
    }
    else
    {
        config->engine[0].phase_increment = mt_table_rate_increment(rate_a);
        config->engine[1].phase_increment = mt_table_rate_increment(rate_b);
    }

    config->engine[0].waveform = (uint8_t)(wave_a % MT_WAVE_COUNT);
    config->engine[1].waveform = (uint8_t)(wave_b % MT_WAVE_COUNT);
    config->engine[0].shape = scale_u8_to_u16(shape_a);
    config->engine[1].shape = scale_u8_to_u16(shape_b);
    config->engine[0].depth = scale_u8_to_u16(depth_a);
    config->engine[1].depth = scale_u8_to_u16(depth_b);
    config->engine[0].offset = offset_u8_to_i16(r[MT_REG_OFFSET_A]);
    config->engine[1].offset = offset_u8_to_i16(r[MT_REG_OFFSET_B]);
    config->phase_offset_b = (uint32_t)phase_b << 24;
    config->cross_a_to_b = scale_u8_to_u16(cross_ab);
    config->cross_b_to_a = scale_u8_to_u16(cross_ba);
    config->gate_width = scale_u8_to_u16(gate_width);

    config->output_route[0] = (uint8_t)(r[MT_REG_OUT_A_ROUTE] % MT_ROUTE_COUNT);
    config->output_route[1] = (uint8_t)(r[MT_REG_OUT_B_ROUTE] % MT_ROUTE_COUNT);
    config->output_route[2] = (uint8_t)(r[MT_REG_OUT_C_ROUTE] % MT_ROUTE_COUNT);
    config->output_route[3] = (uint8_t)(r[MT_REG_OUT_D_ROUTE] % MT_ROUTE_COUNT);
}

void mt_modes_init(void)
{
    g_active_mode = mt_io_read_mode_bits();
    g_last_raw_mode = g_active_mode;
    g_mode_stable_count = 0u;
}

void mt_modes_control_update(void)
{
    mt_parameter_snapshot_t parameters;
    mt_adc_get_snapshot(&parameters);

    uint8_t mode = update_mode_debounce();

    uint32_t external_increment = 0u;
    bool external_clock = mt_clock_get_external_increment(&external_increment);

    mt_render_config_t config;
    configure_engine_defaults(&config);

    switch (mode)
    {
        case MT_MODE_DUAL_LFO:
            configure_mode_dual_lfo(&config, &parameters);
            break;

        case MT_MODE_RATIO_LFO:
            configure_mode_ratio_lfo(&config, &parameters, external_clock, external_increment);
            break;

        case MT_MODE_POLYPHASE:
            configure_mode_polyphase(&config, &parameters, external_clock, external_increment);
            break;

        case MT_MODE_RANDOM:
            configure_mode_random(&config, &parameters);
            break;

        case MT_MODE_ENVELOPE:
            configure_mode_envelope(&config, &parameters);
            break;

        case MT_MODE_CLOCK_GATE:
            configure_mode_clock_gate(&config, &parameters, external_clock, external_increment);
            break;

        case MT_MODE_CROSS_MOD:
            configure_mode_cross_mod(&config, &parameters);
            break;

        case MT_MODE_REGISTER:
        default:
            configure_mode_register(&config, &parameters, external_clock, external_increment);
            break;
    }

    mt_engine_commit_config(&config);

    uint8_t status = mt_engine_get_status();
    if (external_clock)
    {
        status |= MT_STATUS_EXTERNAL_CLOCK;
    }
    mt_config_bus_set_status(status);

    /* Reading and clearing this flag documents that the latest period was seen. */
    (void)mt_clock_take_new_period_flag();
}
