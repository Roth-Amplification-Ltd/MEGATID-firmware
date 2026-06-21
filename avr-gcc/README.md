# MEGATIDE avr-gcc firmware

This tree is the freestanding production-oriented implementation. It uses no
Arduino core, bootloader, heap allocation, C library timing function, or
floating-point processing in the real-time path.

## Module map

- `main.c` — initialization and cooperative foreground loop
- `interrupts.c` — exclusive interrupt-vector ownership
- `io.c` — physical pin contract and PWM timer setup
- `adc.c` — filtered round-robin parameter scanning
- `clock.c` — external period measurement and clock-loss detection
- `engine.c` — 7,812.5 Hz deterministic render engine
- `waveforms.c` — fixed-point waveform and envelope shaping
- `modes.c` — hardware-mode and register-mode parameter interpretation
- `config_bus.c` — SPI-compatible register interface
- `eeprom_store.c` — checked nonvolatile register image
- `tables.c` — generated sine/rate/envelope lookup tables

Run `make` to build. See the repository-level build documentation for flashing
and fuse guidance.
