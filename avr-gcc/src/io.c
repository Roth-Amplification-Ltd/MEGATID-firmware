#include "io.h"
#include "megatide_config.h"
#include "engine.h"
#include "config_bus.h"

#include <avr/io.h>

/* Previous samples are retained so pin-change interrupts can identify edges. */
static uint8_t g_previous_pinb;
static uint8_t g_previous_pind;

void mt_io_init(void)
{
    /* --------------------------- Mode inputs ---------------------------- */
    MT_MODE0_DDR &= (uint8_t)~_BV(MT_MODE0_BIT);
    MT_MODE1_DDR &= (uint8_t)~_BV(MT_MODE1_BIT);
    MT_MODE2_DDR &= (uint8_t)~_BV(MT_MODE2_BIT);

    MT_MODE0_PORT |= _BV(MT_MODE0_BIT);
    MT_MODE1_PORT |= _BV(MT_MODE1_BIT);
    MT_MODE2_PORT |= _BV(MT_MODE2_BIT);

    /* ------------------------- Trigger inputs --------------------------- */
    MT_TRIG_A_DDR &= (uint8_t)~_BV(MT_TRIG_A_BIT);
    MT_TRIG_B_DDR &= (uint8_t)~_BV(MT_TRIG_B_BIT);
    MT_TRIG_A_PORT |= _BV(MT_TRIG_A_BIT);
    MT_TRIG_B_PORT |= _BV(MT_TRIG_B_BIT);

    /* --------------------------- PWM outputs ---------------------------- */
    MT_OUT_A_DDR |= _BV(MT_OUT_A_BIT);
    MT_OUT_B_DDR |= _BV(MT_OUT_B_BIT);
    MT_OUT_C_DDR |= _BV(MT_OUT_C_BIT);
    MT_OUT_D_DDR |= _BV(MT_OUT_D_BIT);

    /*
     * Timer0: fast PWM, TOP=0xFF, non-inverting OC0A/OC0B, prescaler /8.
     * This produces OUT_C and OUT_D at 7,812.5 Hz.
     */
    TCCR0A = _BV(COM0A1) | _BV(COM0B1) | _BV(WGM01) | _BV(WGM00);
    TCCR0B = _BV(CS01);
    OCR0A = 127u;
    OCR0B = 127u;

    /*
     * Timer1: 8-bit fast PWM, TOP=0x00FF, non-inverting OC1A, prescaler /8.
     * OC1B is unavailable because PB2 is the configuration chip-select pin.
     */
    TCCR1A = _BV(COM1A1) | _BV(WGM10);
    TCCR1B = _BV(WGM12) | _BV(CS11);
    OCR1A = 127u;

    /*
     * Timer2: fast PWM, TOP=0xFF, non-inverting OC2B, prescaler /8.
     * Its overflow interrupt is also the master MEGATIDE service tick.
     */
    TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
    TCCR2B = _BV(CS21);
    OCR2B = 127u;
    TIMSK2 = _BV(TOIE2);

    /* ---------------------- Configuration interface --------------------- */
    MT_CFG_DDR &= (uint8_t)~(
        _BV(MT_CFG_CS_BIT)
        | _BV(MT_CFG_MOSI_BIT)
        | _BV(MT_CFG_MISO_BIT)
        | _BV(MT_CFG_SCK_BIT));

    /* Chip select must never float active during power-up. */
    MT_CFG_PORT |= _BV(MT_CFG_CS_BIT);

    /* ----------------------- Pin-change interrupts ---------------------- */
    g_previous_pinb = PINB;
    g_previous_pind = PIND;

    /* Port B watches TRIG_B and /CFG_CS. */
    PCMSK0 = _BV(MT_TRIG_B_BIT) | _BV(MT_CFG_CS_BIT);
    PCICR |= _BV(PCIE0);

    /* Port D watches TRIG_A. */
    PCMSK2 = _BV(MT_TRIG_A_BIT);
    PCICR |= _BV(PCIE2);

    PCIFR = _BV(PCIF0) | _BV(PCIF2);
}

uint8_t mt_io_read_mode_bits(void)
{
    uint8_t mode = 0u;

    if ((MT_MODE0_PINREG & _BV(MT_MODE0_BIT)) != 0u)
    {
        mode |= 0x01u;
    }
    if ((MT_MODE1_PINREG & _BV(MT_MODE1_BIT)) != 0u)
    {
        mode |= 0x02u;
    }
    if ((MT_MODE2_PINREG & _BV(MT_MODE2_BIT)) != 0u)
    {
        mode |= 0x04u;
    }

    return mode;
}

void mt_io_write_pwm_isr(const uint8_t output[4])
{
    /* Physical output order follows the public A/B/C/D naming. */
    OCR1A = output[0];
    OCR2B = output[1];
    OCR0B = output[2];
    OCR0A = output[3];
}

void mt_io_pcint0_isr(void)
{
    uint8_t current = PINB;
    uint8_t changed = (uint8_t)(current ^ g_previous_pinb);

    if ((changed & _BV(MT_TRIG_B_BIT)) != 0u)
    {
        if ((current & _BV(MT_TRIG_B_BIT)) == 0u)
        {
            mt_engine_trigger_isr(0x02u);
        }
    }

    if ((changed & _BV(MT_CFG_CS_BIT)) != 0u)
    {
        bool selected = (current & _BV(MT_CFG_CS_BIT)) == 0u;
        mt_config_bus_chip_select_changed_isr(selected);
    }

    g_previous_pinb = current;
}

void mt_io_pcint2_isr(void)
{
    uint8_t current = PIND;
    uint8_t changed = (uint8_t)(current ^ g_previous_pind);

    if ((changed & _BV(MT_TRIG_A_BIT)) != 0u)
    {
        if ((current & _BV(MT_TRIG_A_BIT)) == 0u)
        {
            mt_engine_trigger_isr(0x01u);
        }
    }

    g_previous_pind = current;
}
