#pragma once
// Phase-accumulator waveform synthesis. Pure C, no ESP-IDF: host-testable.
#include <stdint.h>

typedef enum { DDS_SINE, DDS_SQUARE, DDS_SAW, DDS_TRIANGLE, DDS_DC } dds_shape_t;

typedef struct {
    uint32_t    phase, tw;           // accumulator and tuning word
    dds_shape_t shape;
    uint32_t    duty;                // square: phase threshold, duty * 2^32
    uint8_t     lo, hi;              // output range in codes (amplitude and offset), lo <= hi
} dds_t;

extern uint8_t dds_sine_table[256];  // in RAM, not const: filled by dds_init()

void     dds_init(void);
uint32_t dds_tuning_word(double f_out, double f_s);
// duty is clamped to 0..1; lo and hi are swapped if lo > hi. phase is left alone.
void     dds_set(dds_t *d, dds_shape_t shape, double f_out, double f_s,
                 double duty, uint8_t lo, uint8_t hi);

// One sample. Forced inline so it compiles straight into the IRAM ISR.
// if/else, not switch: GCC may turn a switch into a jump table in flash (.rodata).
static inline __attribute__((always_inline)) uint8_t dds_next(dds_t *d)
{
    d->phase += d->tw;                                   // wraps modulo 2^32: that is the phase
    uint8_t idx = (uint8_t)(d->phase >> 24), raw;
    if      (d->shape == DDS_SINE)     raw = dds_sine_table[idx];
    else if (d->shape == DDS_SQUARE)   raw = (d->phase < d->duty) ? 255 : 0;
    else if (d->shape == DDS_SAW)      raw = idx;
    else if (d->shape == DDS_TRIANGLE) raw = (uint8_t)((idx < 128) ? idx * 2 : (255 - idx) * 2);
    else                               raw = 255;                   // DDS_DC: sits at hi
    return (uint8_t)(d->lo + ((uint32_t)raw * (uint32_t)(d->hi - d->lo) + 127u) / 255u);
}