/*
 * Central interrupt vector file
 * -----------------------------
 *
 * Keeping vector definitions in one translation unit prevents accidental
 * duplicate ISR ownership as the firmware grows. Each vector immediately
 * delegates to the module that owns the corresponding peripheral behavior.
 */

#include "adc.h"
#include "clock.h"
#include "engine.h"
#include "io.h"
#include "config_bus.h"

#include <avr/interrupt.h>

ISR(TIMER2_OVF_vect)
{
    mt_engine_service_tick_isr();
}

ISR(ADC_vect)
{
    mt_adc_conversion_complete_isr();
}

ISR(INT0_vect)
{
    /* Reject contact bounce before it can repeatedly hard-sync the engines. */
    if (mt_clock_external_edge_isr())
    {
        mt_engine_external_clock_edge_isr();
    }
}

ISR(PCINT0_vect)
{
    mt_io_pcint0_isr();
}

ISR(PCINT2_vect)
{
    mt_io_pcint2_isr();
}

ISR(SPI_STC_vect)
{
    mt_config_bus_spi_transfer_isr();
}
