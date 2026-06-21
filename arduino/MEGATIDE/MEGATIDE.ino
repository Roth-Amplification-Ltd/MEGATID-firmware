/*
 * MEGATIDE Arduino build entry point
 * ----------------------------------
 *
 * This sketch intentionally does not use Arduino's high-level timing, ADC,
 * PWM, interrupt, or SPI APIs. The framework is used only as an accessible
 * compiler/upload environment. All real hardware behavior is implemented by
 * the multi-file register-level modules beside this sketch.
 */

#include <Arduino.h>
#include <avr/interrupt.h>

#include "io.h"
#include "adc.h"
#include "clock.h"
#include "engine.h"
#include "config_bus.h"
#include "eeprom_store.h"
#include "modes.h"

void setup()
{
    /* Arduino enables global interrupts before setup(); stop them while the
       custom-part runtime and every interrupt-owned object are initialized. */
    cli();

    mt_io_init();
    mt_clock_init();
    mt_config_bus_init();

    /* A blank/corrupt EEPROM image leaves the documented register defaults. */
    (void)mt_eeprom_load();

    mt_adc_init();
    mt_engine_init();
    mt_modes_init();
    mt_modes_control_update();

    sei();
}

void loop()
{
    /* Apply SPI writes in foreground context rather than inside SPI_STC_vect. */
    mt_config_bus_service();

    if (mt_config_bus_take_restore_request())
    {
        mt_config_bus_restore_defaults();
        mt_modes_control_update();
    }

    if (mt_config_bus_take_save_request())
    {
        mt_eeprom_save();
    }

    /* Timer2 schedules this flag about 244 times per second. */
    if (mt_engine_take_control_update_due())
    {
        mt_modes_control_update();
    }
}
