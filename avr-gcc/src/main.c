#include "io.h"
#include "adc.h"
#include "clock.h"
#include "engine.h"
#include "config_bus.h"
#include "eeprom_store.h"
#include "modes.h"

#include <avr/interrupt.h>

int main(void)
{
    /*
     * Initialize with global interrupts disabled. Peripheral modules may set
     * interrupt-enable bits, but no vector can run until the entire shared
     * state graph is ready.
     */
    cli();

    mt_io_init();
    mt_clock_init();
    mt_config_bus_init();

    /* Blank or corrupt EEPROM simply leaves the documented defaults active. */
    (void)mt_eeprom_load();

    mt_adc_init();
    mt_engine_init();
    mt_modes_init();

    /* Commit the first control interpretation before outputs begin evolving. */
    mt_modes_control_update();

    sei();

    for (;;)
    {
        /* Apply queued SPI writes outside interrupt context. */
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

        /* Approximately 244 control updates per second at a 16 MHz clock. */
        if (mt_engine_take_control_update_due())
        {
            mt_modes_control_update();
        }
    }
}
