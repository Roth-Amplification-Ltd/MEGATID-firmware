# MEGATIDE Arduino sketch

Open `MEGATIDE.ino` with Arduino IDE 2.x. Every `.cpp` and `.h` file in this
folder is part of the sketch and must remain beside the `.ino` file.

This is not a conventional Arduino application. It is the same fixed-function
MEGATIDE firmware built through the Arduino AVR toolchain. The code directly
owns Timer0, Timer1, Timer2, ADC, SPI, INT0, and pin-change interrupts.

Do not add code that uses:

- `delay()`, `millis()`, or `micros()`
- `analogRead()` or `analogWrite()`
- `tone()`
- `attachInterrupt()`
- the Arduino SPI library

Recommended development target: Arduino Uno or a bare ATmega328P-PU at 16 MHz.
For a custom PCB, upload with an ISP programmer rather than depending on a
bootloader.
