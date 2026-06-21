# MEGATIDE configuration register map

## Transaction format

Each transaction is exactly two bytes while `/CFG_CS` is low.

```text
Byte 0: R/W A6 A5 A4 A3 A2 A1 A0
Byte 1: DATA
```

- `R/W = 0`: write `DATA` to the selected register
- `R/W = 1`: read the selected register; transmit a dummy second byte while
  receiving the result on `CFG_MISO`

SPI mode 0 and MSB-first ordering are used.

## Register-controlled mode

Hardware mode `111` enables the register-controlled signal engine. The analog
parameter pins remain available and can be routed as bipolar modulation sources
with registers `0x12` through `0x17`.

## Register table

| Address | Name | Access | Description |
|---:|---|---|---|
| `0x00` | `GLOBAL_CONTROL` | R/W | Bit 0 outputs enable; bit 1 external-clock sync; bit 2 trigger phase reset |
| `0x01` | `ENGINE_CONTROL` | R/W | Reserved for future engine-type selection; currently dual LFO |
| `0x02` | `WAVE_A` | R/W | Engine A waveform number |
| `0x03` | `WAVE_B` | R/W | Engine B waveform number |
| `0x04` | `SHAPE_A` | R/W | Engine A shape, pulse width, or random slew |
| `0x05` | `SHAPE_B` | R/W | Engine B shape, pulse width, or random slew |
| `0x06` | `RATE_A` | R/W | Logarithmic Engine A rate, 0.01-100 Hz |
| `0x07` | `RATE_B` | R/W | Logarithmic Engine B rate, 0.01-100 Hz |
| `0x08` | `DEPTH_A` | R/W | Engine A depth, 0-100% |
| `0x09` | `DEPTH_B` | R/W | Engine B depth, 0-100% |
| `0x0A` | `OFFSET_A` | R/W | Engine A signed offset; `0x80` is zero |
| `0x0B` | `OFFSET_B` | R/W | Engine B signed offset; `0x80` is zero |
| `0x0C` | `PHASE_B` | R/W | Engine B phase offset over one cycle |
| `0x0D` | `RATIO` | R/W | Quantized B:A rate ratio index |
| `0x0E` | `OUT_A_ROUTE` | R/W | Output A source selection |
| `0x0F` | `OUT_B_ROUTE` | R/W | Output B source selection |
| `0x10` | `OUT_C_ROUTE` | R/W | Output C source selection |
| `0x11` | `OUT_D_ROUTE` | R/W | Output D source selection |
| `0x12` | `PARAM0_ROUTE` | R/W | PARAM0 modulation route |
| `0x13` | `PARAM1_ROUTE` | R/W | PARAM1 modulation route |
| `0x14` | `PARAM2_ROUTE` | R/W | PARAM2 modulation route |
| `0x15` | `PARAM3_ROUTE` | R/W | PARAM3 modulation route |
| `0x16` | `PARAM4_ROUTE` | R/W | PARAM4 modulation route |
| `0x17` | `PARAM5_ROUTE` | R/W | PARAM5 modulation route |
| `0x18` | `CROSS_A_TO_B` | R/W | Engine A modulation of Engine B rate |
| `0x19` | `CROSS_B_TO_A` | R/W | Engine B modulation of Engine A rate |
| `0x1A` | `GATE_WIDTH` | R/W | Clock/gate high-time fraction |
| `0x1B` | `SYNC_CONTROL` | R/W | Additional synchronization behavior |
| `0x1C` | `STATUS` | R | Runtime status bits |
| `0x1D` | `FW_MAJOR` | R | Firmware major version |
| `0x1E` | `FW_MINOR` | R | Firmware minor version |
| `0x1F` | `COMMAND` | W | Write `0xA5` to save persistent registers to EEPROM; `0x5A` restores defaults |

## Waveform numbers

| Value | Waveform |
|---:|---|
| 0 | Sine |
| 1 | Triangle |
| 2 | Rising saw |
| 3 | Falling saw |
| 4 | Square |
| 5 | Variable-width pulse |
| 6 | Trapezoid |
| 7 | Exponential rise |
| 8 | Sample-and-hold random |
| 9 | Slewed random |

Values above 9 are wrapped into the implemented range.

## Output route values

| Value | Source |
|---:|---|
| 0 | Engine A |
| 1 | Engine B |
| 2 | Average of A and B |
| 3 | A minus B, re-centered |
| 4 | Engine A at +90° phase |
| 5 | Engine B at +90° phase |
| 6 | Engine A square/gate |
| 7 | Engine B square/gate |
| 8 | Constant zero |
| 9 | Constant full scale |

## Parameter-route byte

```text
Bit 7..5: amount code, 0 = 1/8 scale through 7 = full scale
Bit 4:    invert modulation polarity
Bit 3..0: destination
```

The analog input is treated as bipolar around 2.5 V. The route modulates the
corresponding register-controlled parameter around its stored base value.

| Destination | Parameter |
|---:|---|
| 0 | None |
| 1 | Rate A |
| 2 | Rate B |
| 3 | Wave A |
| 4 | Wave B |
| 5 | Shape A |
| 6 | Shape B |
| 7 | Depth A |
| 8 | Depth B |
| 9 | Phase B |
| 10 | A-to-B cross modulation |
| 11 | B-to-A cross modulation |
| 12 | Gate width |
| 13-15 | Reserved |

## Persistent registers

Registers `0x00` through `0x1B` are saved. Status, firmware version, and command
registers are never persisted.
