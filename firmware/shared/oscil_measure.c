#include <math.h>
#include "oscil_measure.h"
#include "oscil_trigger.h"

void oscil_measure(const uint16_t *s, size_t n, uint32_t rate_hz,
                   const oscil_afe_cal_t *cal, proto_meas_t *m)
{
    *m = (proto_meas_t){0};
    if (n < 2 || rate_hz == 0) return;

    // One pass: extremes in codes, mean and mean square in volts
    uint16_t lo = 4095, hi = 0;
    double sum = 0, sq = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < lo) lo = s[i];
        if (s[i] > hi) hi = s[i];
        double v = oscil_afe_code_to_volts(s[i], cal);
        sum += v;
        sq += v * v;
    }
    m->vmin = oscil_afe_code_to_volts(lo, cal);
    m->vmax = oscil_afe_code_to_volts(hi, cal);
    m->vavg = (float)(sum / n);
    m->vrms = (float)sqrt(sq / n);                  // true RMS, DC included

    // Frequency: rising crossings of the midpoint, first to last, over the whole record
    uint16_t mid = (uint16_t)((lo + hi) / 2);
    trig_cfg_t c = { .level = mid, .hyst = (uint16_t)((hi - lo) / 10 + 2), .edge = TRIG_RISING };
    float f = 0, f_first = 0, f_last = 0;
    long first = -1, last = -1;
    int k = 0;
    for (long i = trig_find(s, 1, n, &c, &f); i >= 0; i = trig_find(s, (size_t)i + 1, n, &c, &f)) {
        if (first < 0) { first = i; f_first = f; }
        last = i; f_last = f; k++;
    }
    if (k >= 2 && hi - lo > 20) {
        // each crossing sits at (i - 1) + frac
        double period = ((last - 1 + f_last) - (first - 1 + f_first)) / (k - 1);
        if (period > 0) m->freq_hz = (float)(rate_hz / period);
    }

    // Duty: share of samples above the midpoint
    size_t above = 0;
    for (size_t i = 0; i < n; i++) above += s[i] > mid;
    m->duty = (float)above / (float)n;
}