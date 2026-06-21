# MEGATIDE hardware interface

## Electrical assumptions

MEGATIDE is designed around an ATmega328P-PU operated at 5 V and 16 MHz.

- `VCC` and `AVCC`: 5 V nominal
- Logic inputs: 5 V CMOS
- Parameter inputs: 0-5 V, never below ground or above AVCC
- PWM outputs: 0/5 V digital PWM requiring external filtering for analog use
- Clock and trigger inputs: active-low, with internal pull-ups enabled
- Mode inputs: logic-low = `0`, logic-high/open = `1`

Every external jack should receive suitable ESD protection, current limiting,
and voltage clamping. The firmware cannot protect an AVR pin from excessive
voltage, negative voltage, or a low-impedance source outside the supply rails.

## 28-pin DIP pinout

| DIP | AVR signal | MEGATIDE signal | Direction | Description |
|---:|---|---|---|---|
| 1 | PC6 `/RESET` | `/RESET` | Input | ISP/programming reset; 10 kΩ pull-up recommended |
| 2 | PD0 | `MODE0` | Input | Mode-select bit 0; internal pull-up |
| 3 | PD1 | `MODE1` | Input | Mode-select bit 1; internal pull-up |
| 4 | PD2 / INT0 | `CLOCK_IN` | Input | Active-low tap/external clock input |
| 5 | PD3 / OC2B | `OUT_B` | Output | 8-bit PWM modulation output B |
| 6 | PD4 | `TRIG_A` | Input | Active-low trigger/reset input A |
| 7 | VCC | `VCC` | Power | +5 V digital supply |
| 8 | GND | `GND` | Power | Digital ground |
| 9 | PB6 / XTAL1 | `XTAL1` | Clock | 16 MHz crystal/resonator |
| 10 | PB7 / XTAL2 | `XTAL2` | Clock | 16 MHz crystal/resonator |
| 11 | PD5 / OC0B | `OUT_C` | Output | 8-bit PWM modulation output C |
| 12 | PD6 / OC0A | `OUT_D` | Output | 8-bit PWM modulation/gate output D |
| 13 | PD7 | `MODE2` | Input | Mode-select bit 2; internal pull-up |
| 14 | PB0 | `TRIG_B` | Input | Active-low trigger/reset input B |
| 15 | PB1 / OC1A | `OUT_A` | Output | 8-bit PWM modulation output A |
| 16 | PB2 / SS | `/CFG_CS` | Input | Active-low configuration chip select |
| 17 | PB3 / MOSI | `CFG_MOSI` | Input | Configuration command/data input |
| 18 | PB4 / MISO | `CFG_MISO` | Output | Optional configuration readback |
| 19 | PB5 / SCK | `CFG_SCK` | Input | Configuration clock input |
| 20 | AVCC | `AVCC` | Power | Filtered +5 V analog supply |
| 21 | AREF | `AREF` | Reference | 100 nF to ground for AVCC-referenced ADC |
| 22 | GND | `AGND` | Power | Analog ground connection |
| 23 | PC0 / ADC0 | `PARAM0` | Analog input | Mode-dependent parameter input |
| 24 | PC1 / ADC1 | `PARAM1` | Analog input | Mode-dependent parameter input |
| 25 | PC2 / ADC2 | `PARAM2` | Analog input | Mode-dependent parameter input |
| 26 | PC3 / ADC3 | `PARAM3` | Analog input | Mode-dependent parameter input |
| 27 | PC4 / ADC4 | `PARAM4` | Analog input | Mode-dependent parameter input |
| 28 | PC5 / ADC5 | `PARAM5` | Analog input | Mode-dependent parameter input |

## Recommended minimum support circuit

- 100 nF ceramic bypass capacitor at VCC-GND
- 100 nF ceramic bypass capacitor at AVCC-AGND
- 4.7 µF to 22 µF local bulk capacitance
- Ferrite bead or 47-100 Ω resistor feeding AVCC from VCC
- 100 nF AREF bypass capacitor
- 10 kΩ `/RESET` pull-up
- 16 MHz crystal plus manufacturer-recommended load capacitors, or a 16 MHz
  three-pin ceramic resonator
- Six 1 kΩ to 10 kΩ series resistors at `PARAM0..5`
- External clamps/buffers for any parameter input exposed to a connector
- RC or active low-pass filters after PWM outputs used as analog control voltage

## Mode input wiring

The three mode pins have internal pull-ups. The simplest mode selector grounds
selected bits. Open inputs read as logic `1`.

Example: dual LFO mode (`000`):

```text
MODE2 -- switch/jumper -- GND
MODE1 -- switch/jumper -- GND
MODE0 -- switch/jumper -- GND
```

Example: register-controlled mode (`111`): leave all three pins open, or drive
all three high.

## Analog parameter wiring

A direct panel potentiometer can be wired as:

```text
5 V ---- top of 10 kΩ to 100 kΩ pot
GND ---- bottom of pot
wiper -- 1 kΩ series resistor -- PARAMx
```

For external CV, use an op-amp or resistor network that maps the desired source
range into 0-5 V and provides input protection. Bipolar modular-synth voltages
must never be connected directly.

## PWM output filtering

A basic two-pole passive filter can be used for control-rate outputs, but an
active filter is recommended when output impedance and ripple matter. The
cutoff should be chosen according to the intended maximum modulation rate.

A useful starting point for pedal LFO duties is two cascaded 10 kΩ / 1 µF poles
(approximately 15.9 Hz each). For operation approaching 100 Hz, use a higher
cutoff and accept more PWM ripple, or use an active reconstruction filter.

## Configuration interface

The configuration interface uses the AVR hardware SPI peripheral in slave mode:

- SPI mode 0
- MSB first
- `/CFG_CS` low for one two-byte transaction
- First byte: read/write bit plus 7-bit address
- Second byte: write data or dummy byte for a read

The interface is optional. Hardware modes `000` through `110` work without a
host controller.
