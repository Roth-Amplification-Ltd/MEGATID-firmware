#ifndef MEGATIDE_TABLES_H
#define MEGATIDE_TABLES_H

/* Flash-resident lookup tables used by both control-rate and render-rate code. */

#include <stdint.h>

uint8_t mt_table_sine_u8(uint8_t index);
uint32_t mt_table_rate_increment(uint8_t index);
uint32_t mt_table_envelope_step(uint8_t index);

#endif /* MEGATIDE_TABLES_H */
