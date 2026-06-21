#ifndef MEGATIDE_TYPES_H
#define MEGATIDE_TYPES_H

/*
 * MEGATIDE shared data types
 * --------------------------
 *
 * This header contains only architecture-neutral integer types and enums.
 * Keeping these definitions separate makes it possible to reuse the signal
 * engine in the avr-gcc and Arduino builds without tying the conceptual model
 * to one framework.
 */

#include <stdint.h>
#include <stdbool.h>

#define MT_PARAMETER_COUNT            6u
#define MT_OUTPUT_COUNT               4u
#define MT_ENGINE_COUNT               2u
#define MT_REGISTER_COUNT             32u
#define MT_PERSISTENT_REGISTER_COUNT  28u

/* The firmware's externally visible hardware modes. */
typedef enum
{
    MT_MODE_DUAL_LFO = 0,
    MT_MODE_RATIO_LFO = 1,
    MT_MODE_POLYPHASE = 2,
    MT_MODE_RANDOM = 3,
    MT_MODE_ENVELOPE = 4,
    MT_MODE_CLOCK_GATE = 5,
    MT_MODE_CROSS_MOD = 6,
    MT_MODE_REGISTER = 7
} mt_mode_t;

/* Waveforms implemented by the first-pass engine. */
typedef enum
{
    MT_WAVE_SINE = 0,
    MT_WAVE_TRIANGLE,
    MT_WAVE_SAW_UP,
    MT_WAVE_SAW_DOWN,
    MT_WAVE_SQUARE,
    MT_WAVE_PULSE,
    MT_WAVE_TRAPEZOID,
    MT_WAVE_EXP_RISE,
    MT_WAVE_SAMPLE_HOLD,
    MT_WAVE_SLEWED_RANDOM,
    MT_WAVE_COUNT
} mt_waveform_t;

/* Sources that may be routed to one of the four physical PWM outputs. */
typedef enum
{
    MT_ROUTE_ENGINE_A = 0,
    MT_ROUTE_ENGINE_B,
    MT_ROUTE_MIX,
    MT_ROUTE_DIFFERENCE,
    MT_ROUTE_ENGINE_A_QUADRATURE,
    MT_ROUTE_ENGINE_B_QUADRATURE,
    MT_ROUTE_ENGINE_A_GATE,
    MT_ROUTE_ENGINE_B_GATE,
    MT_ROUTE_ZERO,
    MT_ROUTE_FULL,
    MT_ROUTE_COUNT
} mt_output_route_t;

/* Destinations available to PARAM0..PARAM5 in register-controlled mode. */
typedef enum
{
    MT_PARAM_DEST_NONE = 0,
    MT_PARAM_DEST_RATE_A,
    MT_PARAM_DEST_RATE_B,
    MT_PARAM_DEST_WAVE_A,
    MT_PARAM_DEST_WAVE_B,
    MT_PARAM_DEST_SHAPE_A,
    MT_PARAM_DEST_SHAPE_B,
    MT_PARAM_DEST_DEPTH_A,
    MT_PARAM_DEST_DEPTH_B,
    MT_PARAM_DEST_PHASE_B,
    MT_PARAM_DEST_CROSS_A_TO_B,
    MT_PARAM_DEST_CROSS_B_TO_A,
    MT_PARAM_DEST_GATE_WIDTH
} mt_parameter_destination_t;

/* Envelope state is intentionally small because it is read every service tick. */
typedef enum
{
    MT_ENV_IDLE = 0,
    MT_ENV_ATTACK,
    MT_ENV_DECAY
} mt_envelope_state_t;

/*
 * Engine configuration copied atomically from the control-rate code to the
 * real-time interrupt. Values are normalized to full integer ranges so the ISR
 * never needs floating-point arithmetic.
 */
typedef struct
{
    uint32_t phase_increment;
    uint16_t shape;
    uint16_t depth;
    int16_t offset;
    uint8_t waveform;
} mt_engine_config_t;

/* Mutable state owned by the real-time service interrupt. */
typedef struct
{
    uint32_t phase;
    uint16_t random_start;
    uint16_t random_target;
    uint32_t envelope_level_q16;
    uint8_t envelope_state;
    uint16_t last_sample;
} mt_engine_state_t;

/*
 * Complete render configuration. The main loop constructs one of these from
 * the mode pins, ADC values, external clock state, and configuration registers.
 * It is then copied with interrupts briefly disabled.
 */
typedef struct
{
    mt_engine_config_t engine[MT_ENGINE_COUNT];

    uint8_t mode;
    uint8_t outputs_enabled;
    uint8_t sync_on_external_clock;
    uint8_t trigger_resets_phase;

    uint32_t phase_offset_b;
    uint16_t polyphase_spacing;
    uint16_t random_correlation;
    uint16_t cross_a_to_b;
    uint16_t cross_b_to_a;

    uint32_t envelope_attack_step[MT_ENGINE_COUNT];
    uint32_t envelope_decay_step[MT_ENGINE_COUNT];
    uint8_t envelope_curve[MT_ENGINE_COUNT];

    uint32_t gate_increment_first[MT_OUTPUT_COUNT];
    uint32_t gate_increment_second[MT_OUTPUT_COUNT];
    uint16_t gate_width;

    uint8_t output_route[MT_OUTPUT_COUNT];
} mt_render_config_t;

/* A stable ADC snapshot copied out of the interrupt-owned filter state. */
typedef struct
{
    uint16_t value[MT_PARAMETER_COUNT];
} mt_parameter_snapshot_t;

#endif /* MEGATIDE_TYPES_H */
