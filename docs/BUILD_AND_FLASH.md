# Building and flashing

## avr-gcc version

Required tools:

- avr-gcc
- avr-libc
- avr-objcopy
- avrdude
- GNU make

Build:

```sh
cd avr-gcc
make
```

Flash with an ISP programmer:

```sh
make flash PROGRAMMER=usbasp PORT=usb
```

The Makefile defaults to `atmega328p`, 16 MHz, and `usbasp`. Override variables
on the command line as needed.

The recommended production fuse intent is:

- External 16 MHz crystal/resonator
- Full-swing or appropriate crystal oscillator setting
- Brown-out detection appropriate to the analog system, commonly 4.3 V
- Bootloader disabled
- Reset pin retained for ISP

Fuse bytes vary with oscillator choice and production policy. Verify them
against the current ATmega328P data sheet and the exact clock circuit before
programming production parts.

## Arduino version

Open `arduino/MEGATIDE/MEGATIDE.ino` in Arduino IDE 2.x or compile with
Arduino CLI using the Arduino AVR Boards core.

Recommended board selection for development:

- Board: Arduino Uno
- Programmer: your ISP programmer
- Upload using programmer

For a bare ATmega328P-PU, program the bootloader/fuses once if using the Uno
board definition, then upload with ISP. A serial bootloader is not required and
is not used by the firmware.

## Arduino runtime warning

The sketch directly owns the AVR peripherals. Arduino timing and peripheral
APIs are intentionally unavailable after `setup()`:

- `millis()` / `micros()` / `delay()`
- `analogRead()` / `analogWrite()`
- `tone()`
- `attachInterrupt()`
- `SPI`

Use the supplied MEGATIDE modules instead.
