#ifndef MEGATIDE_ENGINE_H
#define MEGATIDE_ENGINE_H

#include "megatide_types.h"
#include <stdint.h>
#include <stdbool.h>

/* Initialize all engine state and install a benign default render configuration. */
void mt_engine_init(void);

/* Atomically replace the configuration consumed by the real-time ISR. */
void mt_engine_commit_config(const mt_render_config_t *config);

/* Timer2 overflow service routine body. */
void mt_engine_service_tick_isr(void);

/* Event entry points safe to call from other interrupt handlers. */
void mt_engine_trigger_isr(uint8_t trigger_mask);
void mt_engine_external_clock_edge_isr(void);

/* Main-loop scheduling and status accessors. */
bool mt_engine_take_control_update_due(void);
uint8_t mt_engine_get_status(void);

#endif /* MEGATIDE_ENGINE_H */
