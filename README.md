# MEGATIDE

MEGATIDE is an open-source, fixed-function modulation processor implemented on a
28-pin ATmega328P-PU. The firmware is deliberately presented as a **custom IC
implementation**, not as a general-purpose Arduino application.

A product designer integrates MEGATIDE by wiring controls and support circuitry
to a stable electrical interface:

- Six 0-5 V parameter inputs
- Three hardware mode-select inputs
- One clock/tap input
- Two trigger/reset inputs
- Four PWM modulation/gate outputs
- One optional SPI-compatible configuration port

The package contains two independent source trees:

- `avr-gcc/` — freestanding C firmware for avr-gcc and avr-libc
- `arduino/MEGATIDE/` — a multi-file Arduino sketch using the AVR register-level
  peripherals directly

Both versions implement the same pinout, control model, modes, register map,
EEPROM configuration format, and real-time engine.

## Implemented modes

| MODE2:0 | Mode |
|---|---|
| `000` | Dual independent LFO |
| `001` | Ratio-linked dual LFO |
| `010` | Polyphase/quadrature LFO |
| `011` | Dual random modulation |
| `100` | Dual triggered attack/decay envelope |
| `101` | Four-output clock/gate generator |
| `110` | Cross-modulated dual LFO |
| `111` | Register-controlled dual modulation engine |

## Important first-pass design decisions

- The real-time service rate is 7,812.5 Hz with a 16 MHz clock.
- All four outputs are 8-bit hardware PWM at 7,812.5 Hz.
- The firmware uses fixed-point arithmetic in the interrupt-time signal path.
- Timer0, Timer1, Timer2, the ADC, SPI, INT0, and pin-change interrupts are owned
  by MEGATIDE.
- The Arduino build intentionally disables Arduino's Timer0 overflow interrupt.
  Consequently `millis()`, `micros()`, `delay()`, `tone()`, `analogWrite()`,
  `attachInterrupt()`, and the SPI library must not be used by application code.
- The Arduino tree is provided as an accessible build route, not as a different
  runtime architecture.

## Documentation

- [`docs/HARDWARE_INTERFACE.md`](docs/HARDWARE_INTERFACE.md)
- [`docs/REGISTER_MAP.md`](docs/REGISTER_MAP.md)
- [`docs/MODE_PARAMETER_MAP.md`](docs/MODE_PARAMETER_MAP.md)
- [`docs/BUILD_AND_FLASH.md`](docs/BUILD_AND_FLASH.md)
- [`docs/FIRST_PASS_LIMITATIONS.md`](docs/FIRST_PASS_LIMITATIONS.md)

## License

MIT. See `LICENSE`.
