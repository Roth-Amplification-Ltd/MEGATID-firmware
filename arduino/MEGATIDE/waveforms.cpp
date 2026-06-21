#include "waveforms.h"
#include "tables.h"

#include <stdint.h>

/*
 * Render a symmetric trapezoid.
 *
 * The shape parameter controls the duration of each ramp. Small values create
 * a waveform close to a square wave; large values approach a triangle. The
 * implementation uses only integer arithmetic so it is safe in the service ISR.
 */
static uint16_t render_trapezoid(uint16_t phase, uint16_t shape)
{
    uint16_t ramp = (uint16_t)(1024u + ((uint32_t)shape * 15360u >> 16));
    uint16_t half = (uint16_t)(phase & 0x7FFFu);
    bool second_half = (phase & 0x8000u) != 0u;

    if (!second_half)
    {
        if (half < ramp)
        {
            return (uint16_t)(((uint32_t)half * 65535u) / ramp);
        }
        return 65535u;
    }

    if (half < ramp)
    {
        return (uint16_t)(65535u - ((uint32_t)half * 65535u) / ramp);
    }
    return 0u;
}

uint16_t mt_waveform_render(
    const mt_engine_state_t *state,
    const mt_engine_config_t *config,
    uint32_t phase)
{
    uint16_t phase16 = (uint16_t)(phase >> 16);
    uint8_t waveform = (uint8_t)(config->waveform % MT_WAVE_COUNT);

    switch (waveform)
    {
        case MT_WAVE_SINE:
            /* Expand the 8-bit sine table to 16 bits without a divide. */
            return (uint16_t)mt_table_sine_u8((uint8_t)(phase >> 24)) * 257u;

        case MT_WAVE_TRIANGLE:
        {
            uint16_t half_phase = (uint16_t)(phase16 & 0x7FFFu);
            if ((phase16 & 0x8000u) == 0u)
            {
                return (uint16_t)(half_phase << 1);
            }
            return (uint16_t)(65535u - (half_phase << 1));
        }

        case MT_WAVE_SAW_UP:
            return phase16;

        case MT_WAVE_SAW_DOWN:
            return (uint16_t)(65535u - phase16);

        case MT_WAVE_SQUARE:
            return (phase16 < 32768u) ? 0u : 65535u;

        case MT_WAVE_PULSE:
        {
            /* Keep pulse width away from the mathematically degenerate endpoints. */
            uint16_t threshold = config->shape;
            if (threshold < 655u)
            {
                threshold = 655u;
            }
            else if (threshold > 64880u)
            {
                threshold = 64880u;
            }
            return (phase16 < threshold) ? 65535u : 0u;
        }

        case MT_WAVE_TRAPEZOID:
            return render_trapezoid(phase16, config->shape);

        case MT_WAVE_EXP_RISE:
            /* y = x^2 produces a useful exponential-like control curve. */
            return (uint16_t)(((uint32_t)phase16 * phase16) >> 16);

        case MT_WAVE_SAMPLE_HOLD:
            return state->random_target;

        case MT_WAVE_SLEWED_RANDOM:
        {
            /*
             * Shape controls how much of the cycle is spent slewing. At zero,
             * the output remains held until almost the end of the cycle. At
             * full scale, interpolation spans essentially the entire cycle.
             */
            uint16_t slew_length = config->shape;
            if (slew_length < 256u)
            {
                slew_length = 256u;
            }

            uint16_t hold_end = (uint16_t)(65535u - slew_length);
            if (phase16 <= hold_end)
            {
                return state->random_start;
            }

            uint16_t local_phase = (uint16_t)(phase16 - hold_end);
            int32_t delta = (int32_t)state->random_target - state->random_start;
            int32_t interpolated = (int32_t)state->random_start
                + (delta * local_phase) / slew_length;

            if (interpolated < 0)
            {
                return 0u;
            }
            if (interpolated > 65535)
            {
                return 65535u;
            }
            return (uint16_t)interpolated;
        }

        default:
            return 32768u;
    }
}

uint16_t mt_waveform_apply_level(
    uint16_t sample,
    uint16_t depth,
    int16_t offset)
{
    /*
     * Re-center around zero, apply unsigned depth, then move back to unsigned
     * space and add the signed offset. The final clamp prevents wraparound at
     * both rails.
     */
    int32_t centered = (int32_t)sample - 32768;
    int32_t scaled = (centered * depth) >> 16;
    int32_t result = scaled + 32768 + offset;

    if (result < 0)
    {
        return 0u;
    }
    if (result > 65535)
    {
        return 65535u;
    }
    return (uint16_t)result;
}

uint16_t mt_waveform_shape_envelope(uint16_t linear, uint8_t curve)
{
    switch (curve & 0x03u)
    {
        case 0u:
            return linear;

        case 1u:
            /* Convex response: slow start, faster finish. */
            return (uint16_t)(((uint32_t)linear * linear) >> 16);

        case 2u:
        {
            /* Concave response: fast start, slower finish. */
            uint16_t inverse = (uint16_t)(65535u - linear);
            return (uint16_t)(65535u - (((uint32_t)inverse * inverse) >> 16));
        }

        case 3u:
        default:
            /* A deliberately stronger S-curve useful for percussive envelopes. */
            if (linear < 32768u)
            {
                uint32_t doubled = (uint32_t)linear << 1;
                return (uint16_t)((doubled * doubled) >> 17);
            }
            else
            {
                uint16_t inverse = (uint16_t)(65535u - linear);
                uint32_t doubled = (uint32_t)inverse << 1;
                return (uint16_t)(65535u - ((doubled * doubled) >> 17));
            }
    }
}
