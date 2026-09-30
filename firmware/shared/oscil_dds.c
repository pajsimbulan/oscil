#include <math.h>
#include "oscil_dds.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

uint8_t dds_sine_table[256];

void dds_init(void)
{
    for (int i = 0; i < 256; i++)
        dds_sine_table[i] = (uint8_t)lround(127.5 + 127.5 * sin(2.0 * M_PI * i / 256.0));
}

uint32_t dds_tuning_word(double f_out, double f_s)
{
    return (uint32_t)llround(f_out * 4294967296.0 / f_s);    // f_out * 2^32 / f_s
}

void dds_set(dds_t *d, dds_shape_t shape, double f_out, double f_s,
             double duty, uint8_t lo, uint8_t hi)
{
    if (duty < 0.0) duty = 0.0;
    if (duty > 1.0) duty = 1.0;
    if (lo > hi) { uint8_t t = lo; lo = hi; hi = t; }
    d->shape = shape;
    d->tw    = dds_tuning_word(f_out, f_s);
    d->duty  = (uint32_t)(duty * 4294967295.0);
    d->lo = lo; d->hi = hi;
}