#include "oscil_decimate.h"

void decimate_minmax(const uint16_t *in, size_t n, uint16_t *mn, uint16_t *mx, size_t cols) {
    for(size_t c = 0; c<cols; c++) {
        size_t a = c * n / cols;
        size_t b = (c+1) * n/cols;
        uint16_t lo = in[a];
        uint16_t hi = in[a];
        for(size_t i = a+1; i<b; i++) {
            if(in[i] < lo) lo = in[i];
            if(in[i] > hi) hi = in[i];
        }
        mn[c] = lo;
        mx[c] = hi;
    }
}

