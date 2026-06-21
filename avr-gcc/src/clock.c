#include "clock.h"
#include "megatide_config.h"

#include <avr/io.h>
#include <util/atomic.h>
#include <limits.h>

/* Free-running time base expressed in MEGATIDE service ticks. */
static volatile uint32_t g_tick_count;
static volatile uint32_t g_last_edge_tick;
static volatile uint32_t g_period_ticks;
static volatile uint8_t g_period_valid;
static volatile uint8_t g_new_period;
static volatile uint8_t g_have_previous_edge;

void mt_clock_init(void)
{
    g_tick_count = 0u;
    g_last_edge_tick = 0u;
    g_period_ticks = 0u;
    g_period_valid = 0u;
    g_new_period = 0u;
    g_have_previous_edge = 0u;

    /* Active-low input with internal pull-up. */
    MT_CLOCK_DDR &= (uint8_t)~_BV(MT_CLOCK_BIT);
    MT_CLOCK_PORT |= _BV(MT_CLOCK_BIT);

    /* Falling edge on INT0. */
    EICRA = (uint8_t)((EICRA & (uint8_t)~(_BV(ISC00) | _BV(ISC01))) | _BV(ISC01));
    EIFR = _BV(INTF0);
    EIMSK |= _BV(INT0);
}

void mt_clock_service_tick_isr(void)
{
    g_tick_count++;
}

bool mt_clock_external_edge_isr(void)
{
    uint32_t now = g_tick_count;

    /* The first edge establishes an origin; it is not itself a period. */
    if (g_have_previous_edge == 0u)
    {
        g_last_edge_tick = now;
        g_have_previous_edge = 1u;
        return true;
    }

    uint32_t period = now - g_last_edge_tick;
    g_last_edge_tick = now;

    /*
     * Reject impossibly short intervals caused by contact bounce or noise.
     * Sixteen service ticks are approximately 2.05 ms, or about 488 Hz.
     */
    if (period >= 16u)
    {
        g_period_ticks = period;
        g_period_valid = 1u;
        g_new_period = 1u;
        return true;
    }

    return false;
}

bool mt_clock_get_external_increment(uint32_t *phase_increment)
{
    uint32_t now;
    uint32_t last;
    uint32_t period;
    uint8_t valid;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        now = g_tick_count;
        last = g_last_edge_tick;
        period = g_period_ticks;
        valid = g_period_valid;
    }

    if ((valid == 0u) || (period == 0u))
    {
        return false;
    }

    /* Declare clock loss after four missing periods. */
    if ((now - last) > (period * 4u))
    {
        return false;
    }

    /*
     * One full 32-bit phase cycle per measured external period. Using
     * UINT32_MAX rather than 2^32 avoids a 64-bit numerator while preserving
     * practical accuracy.
     */
    *phase_increment = (UINT32_MAX / period) + 1u;
    return true;
}

bool mt_clock_take_new_period_flag(void)
{
    bool result;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        result = (g_new_period != 0u);
        g_new_period = 0u;
    }
    return result;
}
