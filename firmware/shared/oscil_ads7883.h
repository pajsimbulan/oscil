#pragma once
// ADS7883 frame decoding. Pure C, no ESP-IDF: used by the firmware and by the host tests.
#include <stdint.h>

// SLAS594 p.9: two leading zeros, D11..D0, two trailing zeros, MSB first
static inline uint16_t ads7883_decode(uint16_t word) {
    return (word >> 2) & 0x0FFF;
}

// Single-line frame: W0 holds the first byte received in bits 7..0, the second in 15..8.
static inline uint16_t spi2_adc_single_code(uint32_t raw) {
    uint16_t word = (uint16_t)(((raw & 0xFFu) << 8) | ((raw >> 8) & 0xFFu)); // swap bytes
    return ads7883_decode(word);
}

// Obvious version: walk the 32 bits two at a time. w is in wire order (first bit = bit 31).
static inline void deinterleave_loop(uint32_t w, uint16_t *ch1, uint16_t *ch2) {
    uint16_t a = 0;
    uint16_t b = 0;
    for (int i = 15; i >= 0; i--) {
        a = (uint16_t)((a << 1) | ((w >> (2 * i + 1)) & 1u)); // odd bits: Q line, CH1
        b = (uint16_t)((b << 1) | ((w >> (2 * i)) & 1u));     // even bits: D line, CH2
    }
    *ch1 = a;
    *ch2 = b;
}

// Fast version: gather every other bit into 16 contiguous bits in 5 steps.
static inline uint32_t compact_even_bits(uint32_t x) {
    x &= 0x55555555u;                   // keep bits 0, 2, 4, ...
    x = (x | (x >> 1)) & 0x33333333u;   // pairs
    x = (x | (x >> 2)) & 0x0F0F0F0Fu;   // nibbles
    x = (x | (x >> 4)) & 0x00FF00FFu;   // bytes
    x = (x | (x >> 8)) & 0x0000FFFFu;   // one 16-bit value
    return x;
}

// Dual-line frame straight from W0 -> both 12-bit codes.
static inline void ads7883_split(uint32_t raw, uint16_t *ch1, uint16_t *ch2) {
    uint32_t w = __builtin_bswap32(raw);                           // W0's byte 0 arrived first
    *ch1 = ads7883_decode((uint16_t)compact_even_bits(w >> 1));    // odd bits: Q, GPIO13
    *ch2 = ads7883_decode((uint16_t)compact_even_bits(w));         // even bits: D, GPIO11
}