#include "config_bus.h"
#include "megatide_config.h"
#include "megatide_types.h"

#include <avr/io.h>
#include <util/atomic.h>

#define MT_CONFIG_FIFO_SIZE 4u
#define MT_CONFIG_FIFO_MASK (MT_CONFIG_FIFO_SIZE - 1u)

typedef struct
{
    uint8_t address;
    uint8_t value;
} mt_register_write_t;

static volatile uint8_t g_registers[MT_REGISTER_COUNT];
static volatile uint8_t g_status;

static volatile mt_register_write_t g_fifo[MT_CONFIG_FIFO_SIZE];
static volatile uint8_t g_fifo_head;
static volatile uint8_t g_fifo_tail;
static volatile uint8_t g_fifo_overflow;

static volatile uint8_t g_selected;
static volatile uint8_t g_byte_index;
static volatile uint8_t g_command_byte;

static volatile uint8_t g_save_request;
static volatile uint8_t g_restore_request;

static uint8_t read_register_isr(uint8_t address)
{
    address &= 0x1Fu;

    if (address == MT_REG_STATUS)
    {
        uint8_t status = g_status;
        if (g_fifo_overflow != 0u)
        {
            status |= MT_STATUS_FIFO_OVERFLOW;
        }
        return status;
    }
    if (address == MT_REG_FW_MAJOR)
    {
        return MT_FIRMWARE_MAJOR;
    }
    if (address == MT_REG_FW_MINOR)
    {
        return MT_FIRMWARE_MINOR;
    }
    if (address == MT_REG_COMMAND)
    {
        return 0u;
    }

    return g_registers[address];
}

static void enqueue_write_isr(uint8_t address, uint8_t value)
{
    uint8_t next = (uint8_t)((g_fifo_head + 1u) & MT_CONFIG_FIFO_MASK);

    if (next == g_fifo_tail)
    {
        g_fifo_overflow = 1u;
        return;
    }

    g_fifo[g_fifo_head].address = address;
    g_fifo[g_fifo_head].value = value;
    g_fifo_head = next;
}

void mt_config_bus_restore_defaults(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        for (uint8_t i = 0u; i < MT_REGISTER_COUNT; ++i)
        {
            g_registers[i] = 0u;
        }

        g_registers[MT_REG_GLOBAL_CONTROL] =
            MT_GLOBAL_OUTPUT_ENABLE | MT_GLOBAL_TRIGGER_RESET;
        g_registers[MT_REG_WAVE_A] = MT_WAVE_SINE;
        g_registers[MT_REG_WAVE_B] = MT_WAVE_TRIANGLE;
        g_registers[MT_REG_SHAPE_A] = 128u;
        g_registers[MT_REG_SHAPE_B] = 128u;
        g_registers[MT_REG_RATE_A] = 96u;
        g_registers[MT_REG_RATE_B] = 112u;
        g_registers[MT_REG_DEPTH_A] = 255u;
        g_registers[MT_REG_DEPTH_B] = 255u;
        g_registers[MT_REG_OFFSET_A] = 128u;
        g_registers[MT_REG_OFFSET_B] = 128u;
        g_registers[MT_REG_PHASE_B] = 0u;
        g_registers[MT_REG_RATIO] = 5u;
        g_registers[MT_REG_OUT_A_ROUTE] = MT_ROUTE_ENGINE_A;
        g_registers[MT_REG_OUT_B_ROUTE] = MT_ROUTE_ENGINE_B;
        g_registers[MT_REG_OUT_C_ROUTE] = MT_ROUTE_MIX;
        g_registers[MT_REG_OUT_D_ROUTE] = MT_ROUTE_ENGINE_A_GATE;
        g_registers[MT_REG_GATE_WIDTH] = 128u;
    }
}

void mt_config_bus_init(void)
{
    g_fifo_head = 0u;
    g_fifo_tail = 0u;
    g_fifo_overflow = 0u;
    g_selected = 0u;
    g_byte_index = 0u;
    g_command_byte = 0u;
    g_save_request = 0u;
    g_restore_request = 0u;
    g_status = 0u;

    mt_config_bus_restore_defaults();

    /*
     * SPI slave, interrupt enabled, mode 0, MSB first. MISO remains an input
     * until /CFG_CS is asserted so the pin does not fight another bus device.
     */
    SPCR = _BV(SPE) | _BV(SPIE);
    SPSR = 0u;
    SPDR = 0u;
}

void mt_config_bus_chip_select_changed_isr(bool selected)
{
    g_selected = selected ? 1u : 0u;
    g_byte_index = 0u;

    if (selected)
    {
        MT_CFG_DDR |= _BV(MT_CFG_MISO_BIT);
        SPDR = 0u;
    }
    else
    {
        MT_CFG_DDR &= (uint8_t)~_BV(MT_CFG_MISO_BIT);
    }
}

void mt_config_bus_spi_transfer_isr(void)
{
    uint8_t received = SPDR;

    if (g_selected == 0u)
    {
        SPDR = 0u;
        return;
    }

    if (g_byte_index == 0u)
    {
        g_command_byte = received;
        g_byte_index = 1u;

        if ((received & 0x80u) != 0u)
        {
            SPDR = read_register_isr((uint8_t)(received & 0x7Fu));
        }
        else
        {
            SPDR = 0u;
        }
    }
    else if (g_byte_index == 1u)
    {
        if ((g_command_byte & 0x80u) == 0u)
        {
            enqueue_write_isr((uint8_t)(g_command_byte & 0x7Fu), received);
        }

        g_byte_index = 2u;
        SPDR = 0u;
    }
    else
    {
        /* Ignore extra bytes until chip select returns high. */
        SPDR = 0u;
    }
}

void mt_config_bus_service(void)
{
    for (;;)
    {
        mt_register_write_t item;
        bool available = false;

        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            if (g_fifo_tail != g_fifo_head)
            {
                item.address = g_fifo[g_fifo_tail].address;
                item.value = g_fifo[g_fifo_tail].value;
                g_fifo_tail = (uint8_t)((g_fifo_tail + 1u) & MT_CONFIG_FIFO_MASK);
                available = true;
            }
        }

        if (!available)
        {
            break;
        }

        mt_config_bus_write_register(item.address, item.value);
    }
}

uint8_t mt_config_bus_read_register(uint8_t address)
{
    uint8_t value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        value = read_register_isr(address);
    }
    return value;
}

void mt_config_bus_write_register(uint8_t address, uint8_t value)
{
    address &= 0x1Fu;

    if ((address == MT_REG_STATUS)
        || (address == MT_REG_FW_MAJOR)
        || (address == MT_REG_FW_MINOR))
    {
        return;
    }

    if (address == MT_REG_COMMAND)
    {
        if (value == MT_COMMAND_SAVE_EEPROM)
        {
            g_save_request = 1u;
        }
        else if (value == MT_COMMAND_RESTORE_DEFAULT)
        {
            g_restore_request = 1u;
        }
        return;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        g_registers[address] = value;
    }
}

void mt_config_bus_copy_persistent_registers(uint8_t *destination)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        for (uint8_t i = 0u; i < MT_PERSISTENT_REGISTER_COUNT; ++i)
        {
            destination[i] = g_registers[i];
        }
    }
}

void mt_config_bus_load_persistent_registers(const uint8_t *source)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        for (uint8_t i = 0u; i < MT_PERSISTENT_REGISTER_COUNT; ++i)
        {
            g_registers[i] = source[i];
        }
    }
}

bool mt_config_bus_take_save_request(void)
{
    bool requested;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        requested = (g_save_request != 0u);
        g_save_request = 0u;
    }
    return requested;
}

bool mt_config_bus_take_restore_request(void)
{
    bool requested;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        requested = (g_restore_request != 0u);
        g_restore_request = 0u;
    }
    return requested;
}

void mt_config_bus_set_status(uint8_t status)
{
    g_status = status;
}
