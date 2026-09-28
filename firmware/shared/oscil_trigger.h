#pragma once
//Edge trigger with hysteresis adn sub-sample position.  Pure C, no ESP-IDf: host-testable
#include <stdint.h>
#include <stddef.h>

typedef enum { TRIG_RISING, TRIG_FALLING } trig_edge_t;

typedef struct {
    uint16_t level; // ADC code
    uint16_t hyst;  // codes
    trig_edge_t edge;
} trig_cfg_t ;

// Searches buf[from .. to-1].  Returns he index i of the first sample at or past the level
//after arming, or -1 *frac (may be NULL) gets the crossing between i-1 and i, 0..1, so the 
// crossing is at (i-1) + *frac
long trig_find(const uint16_t *buf, size_t from, size_t to, const trig_cfg_t *cfg, float *frac);