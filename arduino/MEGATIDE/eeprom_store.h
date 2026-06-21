#ifndef MEGATIDE_EEPROM_STORE_H
#define MEGATIDE_EEPROM_STORE_H

#include <stdbool.h>

/* Load a valid register image from EEPROM; return false on blank/corrupt data. */
bool mt_eeprom_load(void);

/* Save the current persistent register range using update semantics. */
void mt_eeprom_save(void);

#endif /* MEGATIDE_EEPROM_STORE_H */
