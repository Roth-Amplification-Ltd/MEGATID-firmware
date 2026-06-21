#ifndef MEGATIDE_ADC_H
#define MEGATIDE_ADC_H

#include "megatide_types.h"

/* Configure the ADC for interrupt-driven round-robin scanning of ADC0..ADC5. */
void mt_adc_init(void);

/* Called only by ISR(ADC_vect). */
void mt_adc_conversion_complete_isr(void);

/* Atomically copy all six filtered channels for control-rate processing. */
void mt_adc_get_snapshot(mt_parameter_snapshot_t *snapshot);

#endif /* MEGATIDE_ADC_H */
