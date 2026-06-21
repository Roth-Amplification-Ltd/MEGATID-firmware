# Hardware mode parameter map

All parameter inputs are nominally 0-5 V and are internally normalized to
0-65535 after ADC filtering.

## Mode `000`: dual independent LFO

| Input | Function |
|---|---|
| PARAM0 | Engine A frequency, logarithmic 0.01-100 Hz |
| PARAM1 | Engine A waveform selection |
| PARAM2 | Engine A shape / pulse width |
| PARAM3 | Engine B frequency, logarithmic 0.01-100 Hz |
| PARAM4 | Engine B waveform selection |
| PARAM5 | Engine B shape / pulse width |

Outputs default to A, B, average, and A gate.

## Mode `001`: ratio-linked dual LFO

| Input | Function |
|---|---|
| PARAM0 | Master frequency when no external clock is present |
| PARAM1 | Quantized B:A multiplication/division ratio |
| PARAM2 | Engine A waveform |
| PARAM3 | Engine B waveform |
| PARAM4 | Engine B phase offset, 0-360° |
| PARAM5 | Shared waveform shape |

A valid CLOCK_IN period replaces PARAM0. Each accepted clock edge resets phase.

## Mode `010`: polyphase LFO

| Input | Function |
|---|---|
| PARAM0 | Frequency when no external clock is present |
| PARAM1 | Waveform |
| PARAM2 | Shape / pulse width |
| PARAM3 | Adjacent-output spacing, 0-180°; midpoint is 90° |
| PARAM4 | Output depth |
| PARAM5 | Signed output offset; midpoint is zero |

OUT_A through OUT_D use phase offsets of 0, 1×, 2×, and 3× the selected spacing.

## Mode `011`: dual random modulation

| Input | Function |
|---|---|
| PARAM0 | Random A update rate |
| PARAM1 | Random A slew fraction; near zero selects sample-and-hold |
| PARAM2 | Random B update rate |
| PARAM3 | Random B slew fraction; near zero selects sample-and-hold |
| PARAM4 | B correlation toward A |
| PARAM5 | Shared random output depth |

## Mode `100`: dual triggered attack/decay envelope

| Input | Function |
|---|---|
| PARAM0 | Envelope A attack, approximately 1 ms-30 s |
| PARAM1 | Envelope A decay, approximately 1 ms-30 s |
| PARAM2 | Envelope A curve: linear, convex, concave, or S-curve |
| PARAM3 | Envelope B attack, approximately 1 ms-30 s |
| PARAM4 | Envelope B decay, approximately 1 ms-30 s |
| PARAM5 | Envelope B curve |

TRIG_A and TRIG_B start the corresponding envelope. OUT_C is the average of
both envelopes. OUT_D is high while either envelope is active.

## Mode `101`: four-output clock/gate generator

| Input | Function |
|---|---|
| PARAM0 | Internal master frequency when no external clock is present |
| PARAM1 | OUT_B ratio |
| PARAM2 | OUT_C ratio |
| PARAM3 | OUT_D ratio |
| PARAM4 | Gate width, approximately 0-100% |
| PARAM5 | Swing, straight through approximately 75/25 |

OUT_A remains 1:1. CLOCK_IN becomes the master whenever a valid period is
present.

## Mode `110`: cross-modulated dual LFO

| Input | Function |
|---|---|
| PARAM0 | Engine A base rate |
| PARAM1 | Engine B base rate |
| PARAM2 | A-to-B rate-modulation amount |
| PARAM3 | B-to-A rate-modulation amount |
| PARAM4 | Waveform-pair selection |
| PARAM5 | Shared waveform shape |

OUT_D is the re-centered difference between the two engine outputs.

## Mode `111`: register-controlled engine

The SPI register bank establishes base values and output routing. PARAM0 through
PARAM5 operate as bipolar modulation sources according to PARAMx_ROUTE.
Unconnected analog inputs should be tied to a defined voltage or routed to
`NONE`; do not leave ADC pins floating.
