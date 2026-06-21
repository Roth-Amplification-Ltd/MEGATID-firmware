#include "adc.h"
#include "megatide_config.h"

#include <avr/io.h>
#include <util/atomic.h>

/*
 * ADC scanning strategy
 * ---------------------
 *
 * ADC0 through ADC5 are sampled continuously in round-robin order. After every
 * multiplexer change, one conversion is deliberately discarded so charge left
 * in the sample-and-hold capacitor cannot contaminate the new channel.
 *
 * Each accepted sample passes through a one-pole IIR filter:
 *
 *     filtered += (sample - filtered) / 8
 *
 * This removes much of the pot/CV noise without introducing a large control
 * latency. The filter state remains 16-bit even though the ADC is 10-bit so it
 * can be expanded directly into the engine's normalized 0..65535 parameter
 * space.
 */

static volatile uint16_t g_filtered[MT_PARAMETER_COUNT];
static volatile uint8_t g_channel;
static volatile uint8_t g_discard_next;
static volatile uint8_t g_initialized_mask;

void mt_adc_init(void)
{
    g_channel = 0u;
    g_discard_next = 1u;
    g_initialized_mask = 0u;

    for (uint8_t i = 0u; i < MT_PARAMETER_COUNT; ++i)
    {
        g_filtered[i] = 0u;
    }

    /* AVCC reference, right-adjusted result, start on ADC0. */
    ADMUX = _BV(REFS0);
    ADCSRB = 0u;

    /* Enable ADC and interrupt; divide 16 MHz by 128 for a 125 kHz ADC clock. */
    ADCSRA = _BV(ADEN) | _BV(ADIE)
        | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);

    ADCSRA |= _BV(ADSC);
}

void mt_adc_conversion_complete_isr(void)
{
    uint16_t raw = ADC;

    if (g_discard_next != 0u)
    {
        g_discard_next = 0u;
    }
    else
    {
        uint8_t bit = (uint8_t)(1u << g_channel);
        uint16_t expanded = (uint16_t)((raw << 6) | (raw >> 4));

        if ((g_initialized_mask & bit) == 0u)
        {
            g_filtered[g_channel] = expanded;
            g_initialized_mask |= bit;
        }
        else
        {
            int32_t error = (int32_t)expanded - g_filtered[g_channel];
            g_filtered[g_channel] = (uint16_t)(g_filtered[g_channel] + (error >> 3));
        }

        g_channel++;
        if (g_channel >= MT_PARAMETER_COUNT)
        {
            g_channel = 0u;
        }

        ADMUX = (uint8_t)(_BV(REFS0) | g_channel);
        g_discard_next = 1u;
    }

    /* Manual restart keeps channel switching and discard behavior explicit. */
    ADCSRA |= _BV(ADSC);
}

void mt_adc_get_snapshot(mt_parameter_snapshot_t *snapshot)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        for (uint8_t i = 0u; i < MT_PARAMETER_COUNT; ++i)
        {
            snapshot->value[i] = g_filtered[i];
        }
    }
}
