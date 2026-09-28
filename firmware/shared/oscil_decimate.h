#pragma once
//Min/max (peak-detect) decimation.  PureC
#include <stdint.h>
#include <stddef.h>

//Column c covers in [c*n/cols ... (c+1)*n/cols -1]    Requiers n >= cols > 0
void decimate_minmax(const uint16_t *in, size_t n, uint16_t *mn, uint16_t *mx, size_t cols);