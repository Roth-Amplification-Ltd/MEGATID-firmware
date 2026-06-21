# First-pass limitations and next engineering steps

This package is intended to establish a complete, buildable firmware
architecture and stable electrical contract. Before treating it as a finished
commercial component, complete bench validation and characterize the analog
support circuits.

## Current limitations

- PWM resolution is 8 bits. A later DAC-backed MEGATIDE variant can reuse most
  of the engine and control code.
- PWM carrier frequency is 7.8125 kHz. The reconstruction filter therefore has
  to balance ripple against the requested 100 Hz maximum modulation bandwidth.
- The register interface uses a small interrupt-safe command FIFO. A host must
  leave `/CFG_CS` high between two-byte transactions and must not stream writes
  indefinitely without allowing the main loop to service them.
- Mode 5 swing is implemented as alternating half-cycle velocity, not as a
  pattern sequencer.
- The envelope mode is attack/decay only in this pass.
- MIDI is intentionally omitted from the fixed 28-pin interface. The UART pins
  are used as mode inputs so the standalone ASIC-style control surface does not
  lose two mode bits.
- Parameter ADC calibration assumes AVCC as the reference. Production hardware
  should characterize gain, endpoint, and noise behavior.
- The firmware has not yet been cycle-counted on physical ATmega328P hardware.

## Recommended validation sequence

1. Verify fuse and oscillator startup behavior.
2. Measure all four PWM carrier frequencies and duty-cycle endpoints.
3. Verify ADC endpoint and midscale behavior with precision DC sources.
4. Exercise every mode at minimum and maximum parameter values.
5. Verify external clock capture over the required frequency range.
6. Verify trigger latency and retrigger behavior.
7. Stress the SPI interface while all outputs are running.
8. Power-cycle during and after EEPROM writes.
9. Measure ISR execution time with a spare debug build pin.
10. Design and characterize the intended output reconstruction filters.
