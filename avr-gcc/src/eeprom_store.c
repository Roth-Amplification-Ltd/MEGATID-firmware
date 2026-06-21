#include "eeprom_store.h"
#include "config_bus.h"
#include "megatide_types.h"

#include <avr/eeprom.h>
#include <stdint.h>

#define MT_EEPROM_MAGIC   0x4D54u /* ASCII-like marker: 'MT'. */
#define MT_EEPROM_VERSION 1u

typedef struct __attribute__((packed))
{
    uint16_t magic;
    uint8_t version;
    uint8_t length;
    uint8_t registers[MT_PERSISTENT_REGISTER_COUNT];
    uint8_t checksum;
} mt_eeprom_image_t;

static mt_eeprom_image_t EEMEM g_eeprom_image;

static uint8_t calculate_checksum(const mt_eeprom_image_t *image)
{
    const uint8_t *bytes = (const uint8_t *)image;
    uint8_t checksum = 0x5Au;

    /* Exclude the checksum byte itself. */
    for (uint8_t i = 0u; i < (uint8_t)(sizeof(*image) - 1u); ++i)
    {
        checksum = (uint8_t)((checksum << 1) | (checksum >> 7));
        checksum ^= bytes[i];
    }
    return checksum;
}

bool mt_eeprom_load(void)
{
    mt_eeprom_image_t image;
    eeprom_read_block(&image, &g_eeprom_image, sizeof(image));

    if (image.magic != MT_EEPROM_MAGIC)
    {
        return false;
    }
    if (image.version != MT_EEPROM_VERSION)
    {
        return false;
    }
    if (image.length != MT_PERSISTENT_REGISTER_COUNT)
    {
        return false;
    }
    if (image.checksum != calculate_checksum(&image))
    {
        return false;
    }

    mt_config_bus_load_persistent_registers(image.registers);
    return true;
}

void mt_eeprom_save(void)
{
    mt_eeprom_image_t image;

    image.magic = MT_EEPROM_MAGIC;
    image.version = MT_EEPROM_VERSION;
    image.length = MT_PERSISTENT_REGISTER_COUNT;
    mt_config_bus_copy_persistent_registers(image.registers);
    image.checksum = calculate_checksum(&image);

    /* update_block avoids unnecessary erase/write cycles for unchanged bytes. */
    eeprom_update_block(&image, &g_eeprom_image, sizeof(image));
}
