#ifndef MEGATIDE_WAVEFORMS_H
#define MEGATIDE_WAVEFORMS_H

#include "megatide_types.h"
#include <stdint.h>

/* Render one unsigned 16-bit waveform sample at an arbitrary phase. */
uint16_t mt_waveform_render(
    const mt_engine_state_t *state,
    const mt_engine_config_t *config,
    uint32_t phase);

/* Apply depth and signed DC offset, saturating to the 0..65535 output range. */
uint16_t mt_waveform_apply_level(
    uint16_t sample,
    uint16_t depth,
    int16_t offset);

/* Shape a linear envelope level according to one of four response curves. */
uint16_t mt_waveform_shape_envelope(uint16_t linear, uint8_t curve);

#endif /* MEGATIDE_WAVEFORMS_H */
