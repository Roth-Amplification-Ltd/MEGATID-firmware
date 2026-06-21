#ifndef MEGATIDE_CLOCK_H
#define MEGATIDE_CLOCK_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize INT0 for an active-low clock/tap input. */
void mt_clock_init(void);

/* Increment the free-running service tick counter from the Timer2 ISR. */
void mt_clock_service_tick_isr(void);

/* Capture an external clock edge from INT0. */
bool mt_clock_external_edge_isr(void);

/* Return true when a recent external period is available. */
bool mt_clock_get_external_increment(uint32_t *phase_increment);

/* Return true once for each newly measured external clock edge. */
bool mt_clock_take_new_period_flag(void);

#endif /* MEGATIDE_CLOCK_H */
