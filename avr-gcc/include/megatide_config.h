#ifndef MEGATIDE_CONFIG_H
#define MEGATIDE_CONFIG_H

/*
 * MEGATIDE compile-time configuration and physical pin contract
 * -------------------------------------------------------------
 *
 * These definitions intentionally use AVR port and bit names rather than
 * Arduino pin numbers. The firmware is a fixed-function part, so its physical
 * DIP pinout is the primary interface.
 */

#include <avr/io.h>
#include <stdint.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#define MT_FIRMWARE_MAJOR  0u
#define MT_FIRMWARE_MINOR  1u

/* Timer2 runs fast PWM with a divide-by-8 prescaler and overflows every 256 ticks. */
#define MT_TIMER_PRESCALER 8UL
#define MT_SERVICE_HZ      (F_CPU / MT_TIMER_PRESCALER / 256UL)
#define MT_CONTROL_DIVIDER 32u

/* MODE0: DIP pin 2, PD0. */
#define MT_MODE0_DDR       DDRD
#define MT_MODE0_PORT      PORTD
#define MT_MODE0_PINREG    PIND
#define MT_MODE0_BIT       PD0

/* MODE1: DIP pin 3, PD1. */
#define MT_MODE1_DDR       DDRD
#define MT_MODE1_PORT      PORTD
#define MT_MODE1_PINREG    PIND
#define MT_MODE1_BIT       PD1

/* CLOCK_IN: DIP pin 4, PD2 / INT0. */
#define MT_CLOCK_DDR       DDRD
#define MT_CLOCK_PORT      PORTD
#define MT_CLOCK_PINREG    PIND
#define MT_CLOCK_BIT       PD2

/* OUT_B: DIP pin 5, PD3 / OC2B. */
#define MT_OUT_B_DDR       DDRD
#define MT_OUT_B_BIT       PD3

/* TRIG_A: DIP pin 6, PD4 / PCINT20. */
#define MT_TRIG_A_DDR      DDRD
#define MT_TRIG_A_PORT     PORTD
#define MT_TRIG_A_PINREG   PIND
#define MT_TRIG_A_BIT      PD4

/* OUT_C: DIP pin 11, PD5 / OC0B. */
#define MT_OUT_C_DDR       DDRD
#define MT_OUT_C_BIT       PD5

/* OUT_D: DIP pin 12, PD6 / OC0A. */
#define MT_OUT_D_DDR       DDRD
#define MT_OUT_D_BIT       PD6

/* MODE2: DIP pin 13, PD7. */
#define MT_MODE2_DDR       DDRD
#define MT_MODE2_PORT      PORTD
#define MT_MODE2_PINREG    PIND
#define MT_MODE2_BIT       PD7

/* TRIG_B: DIP pin 14, PB0 / PCINT0. */
#define MT_TRIG_B_DDR      DDRB
#define MT_TRIG_B_PORT     PORTB
#define MT_TRIG_B_PINREG   PINB
#define MT_TRIG_B_BIT      PB0

/* OUT_A: DIP pin 15, PB1 / OC1A. */
#define MT_OUT_A_DDR       DDRB
#define MT_OUT_A_BIT       PB1

/* Configuration SPI: DIP pins 16-19, PB2-PB5. */
#define MT_CFG_DDR         DDRB
#define MT_CFG_PORT        PORTB
#define MT_CFG_PINREG      PINB
#define MT_CFG_CS_BIT      PB2
#define MT_CFG_MOSI_BIT    PB3
#define MT_CFG_MISO_BIT    PB4
#define MT_CFG_SCK_BIT     PB5

/*
 * Register addresses. Keeping them in one public header prevents the SPI
 * protocol documentation and implementation from drifting apart.
 */
#define MT_REG_GLOBAL_CONTROL      0x00u
#define MT_REG_ENGINE_CONTROL      0x01u
#define MT_REG_WAVE_A              0x02u
#define MT_REG_WAVE_B              0x03u
#define MT_REG_SHAPE_A             0x04u
#define MT_REG_SHAPE_B             0x05u
#define MT_REG_RATE_A              0x06u
#define MT_REG_RATE_B              0x07u
#define MT_REG_DEPTH_A             0x08u
#define MT_REG_DEPTH_B             0x09u
#define MT_REG_OFFSET_A            0x0Au
#define MT_REG_OFFSET_B            0x0Bu
#define MT_REG_PHASE_B             0x0Cu
#define MT_REG_RATIO               0x0Du
#define MT_REG_OUT_A_ROUTE         0x0Eu
#define MT_REG_OUT_B_ROUTE         0x0Fu
#define MT_REG_OUT_C_ROUTE         0x10u
#define MT_REG_OUT_D_ROUTE         0x11u
#define MT_REG_PARAM0_ROUTE        0x12u
#define MT_REG_PARAM1_ROUTE        0x13u
#define MT_REG_PARAM2_ROUTE        0x14u
#define MT_REG_PARAM3_ROUTE        0x15u
#define MT_REG_PARAM4_ROUTE        0x16u
#define MT_REG_PARAM5_ROUTE        0x17u
#define MT_REG_CROSS_A_TO_B        0x18u
#define MT_REG_CROSS_B_TO_A        0x19u
#define MT_REG_GATE_WIDTH          0x1Au
#define MT_REG_SYNC_CONTROL        0x1Bu
#define MT_REG_STATUS              0x1Cu
#define MT_REG_FW_MAJOR            0x1Du
#define MT_REG_FW_MINOR            0x1Eu
#define MT_REG_COMMAND             0x1Fu

#define MT_COMMAND_SAVE_EEPROM     0xA5u
#define MT_COMMAND_RESTORE_DEFAULT 0x5Au

/* GLOBAL_CONTROL bits. */
#define MT_GLOBAL_OUTPUT_ENABLE    0x01u
#define MT_GLOBAL_EXTERNAL_SYNC    0x02u
#define MT_GLOBAL_TRIGGER_RESET    0x04u

/* Status bits exposed through MT_REG_STATUS. */
#define MT_STATUS_EXTERNAL_CLOCK   0x01u
#define MT_STATUS_ENVELOPE_A       0x02u
#define MT_STATUS_ENVELOPE_B       0x04u
#define MT_STATUS_FIFO_OVERFLOW    0x08u

#endif /* MEGATIDE_CONFIG_H */
