#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "oscil_dds.h"

#define GEN_FS_HZ  100000            // DDS update rate: one sample every 10 us

// Starts the 100 kHz DDS interrupt on core 1. Output stays at code 0 until gen_set().
void gen_start(void);
// Any task, any core. Phase stays continuous across changes. on = false holds code 0.
void gen_set(dds_shape_t shape, double hz, double duty, uint8_t lo, uint8_t hi, bool on);