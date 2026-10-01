#pragma once
#include <stddef.h>
#include <stdint.h>
#include "oscil_afe.h"
#include "oscil_proto.h"

// Measures one channel's full-resolution record s[0..n-1], sampled at rate_hz.
// Volts through the channel's calibration. Frequency 0 means "not measurable".
void oscil_measure(const uint16_t *s, size_t n, uint32_t rate_hz,
                   const oscil_afe_cal_t *cal, proto_meas_t *m);