#ifndef MEGATIDE_IO_H
#define MEGATIDE_IO_H

#include <stdint.h>

/* Initialize fixed pin directions, pull-ups, PWM timers, and pin-change masks. */
void mt_io_init(void);

/* Read the three physical mode bits as a binary value from 0 through 7. */
uint8_t mt_io_read_mode_bits(void);

/* Write all four PWM compare registers. Safe to call from the service ISR. */
void mt_io_write_pwm_isr(const uint8_t output[4]);

/* Pin-change handlers called by the central interrupt module. */
void mt_io_pcint0_isr(void);
void mt_io_pcint2_isr(void);

#endif /* MEGATIDE_IO_H */
