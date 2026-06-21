#ifndef MEGATIDE_CONFIG_BUS_H
#define MEGATIDE_CONFIG_BUS_H

#include <stdint.h>
#include <stdbool.h>

/* Configure the AVR hardware SPI peripheral as the MEGATIDE slave port. */
void mt_config_bus_init(void);

/* Interrupt entry points. */
void mt_config_bus_spi_transfer_isr(void);
void mt_config_bus_chip_select_changed_isr(bool selected);

/* Drain queued writes in ordinary main-loop context. */
void mt_config_bus_service(void);

/* Register-bank access used by mode processing and EEPROM storage. */
uint8_t mt_config_bus_read_register(uint8_t address);
void mt_config_bus_write_register(uint8_t address, uint8_t value);
void mt_config_bus_copy_persistent_registers(uint8_t *destination);
void mt_config_bus_load_persistent_registers(const uint8_t *source);
void mt_config_bus_restore_defaults(void);

/* One-shot command flags produced by the COMMAND register. */
bool mt_config_bus_take_save_request(void);
bool mt_config_bus_take_restore_request(void);

/* The status provider is kept separate to avoid register-bank polling races. */
void mt_config_bus_set_status(uint8_t status);

#endif /* MEGATIDE_CONFIG_BUS_H */
