#ifndef MEGATIDE_MODES_H
#define MEGATIDE_MODES_H

/* Initialize mode-debounce and control-rate state. */
void mt_modes_init(void);

/* Rebuild and commit the render configuration from all control sources. */
void mt_modes_control_update(void);

#endif /* MEGATIDE_MODES_H */
