# MEGATIDE freestanding avr-gcc build
# -----------------------------------
# Override MCU, F_CPU, PROGRAMMER, PORT, or AVRDUDE_FLAGS on the make command
# line when a production setup differs from these development defaults.

PROJECT      := megatide
MCU          ?= atmega328p
F_CPU        ?= 16000000UL
PROGRAMMER   ?= usbasp
PORT         ?= usb

CC           := avr-gcc
OBJCOPY      := avr-objcopy
SIZE         := avr-size
AVRDUDE      := avrdude

BUILD_DIR    := build
INCLUDE_DIR  := include
SOURCE_DIR   := src

SOURCES      := $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS      := $(patsubst $(SOURCE_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
TARGET_ELF   := $(BUILD_DIR)/$(PROJECT).elf
TARGET_HEX   := $(BUILD_DIR)/$(PROJECT).hex

CPPFLAGS     := -I$(INCLUDE_DIR) -DF_CPU=$(F_CPU)
CFLAGS       := -mmcu=$(MCU) -std=gnu11 -Os -g3 \
                -ffunction-sections -fdata-sections \
                -Wall -Wextra -Wshadow -Wundef -Wconversion \
                -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS      := -mmcu=$(MCU) -Wl,--gc-sections -Wl,-Map,$(BUILD_DIR)/$(PROJECT).map

AVRDUDE_FLAGS ?=

.PHONY: all clean flash size

all: $(TARGET_HEX) size

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET_ELF): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(TARGET_HEX): $(TARGET_ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: $(TARGET_ELF)
	$(SIZE) -C --mcu=$(MCU) $<

flash: $(TARGET_HEX)
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) \
		$(AVRDUDE_FLAGS) -U flash:w:$(TARGET_HEX):i

clean:
	rm -rf $(BUILD_DIR)
