#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define LINK_MAX_PAYLOAD 7168
#define LINK_MAX_RAW     (LINK_MAX_PAYLOAD + 6)                 // + type, seq, len, crc
#define LINK_MAX_WIRE    (LINK_MAX_RAW + LINK_MAX_RAW / 254 + 2) // + COBS overhead + 0x00

uint16_t crc16_ccitt(const uint8_t *d, size_t n);
size_t   cobs_encode(const uint8_t *in, size_t n, uint8_t *out);
size_t   cobs_decode(const uint8_t *in, size_t n, uint8_t *out, size_t cap);   // 0 on error or if > cap

// Builds a complete wire frame, delimiter included. Returns its length.
size_t link_pack(uint8_t type, uint8_t seq, const void *payload, uint16_t len, uint8_t *wire);

typedef struct {
    uint8_t  type, seq;
    uint16_t len;
    const uint8_t *payload;          // points into the decoder's buffer
} link_frame_t;

typedef struct {
    uint8_t  enc[LINK_MAX_WIRE];
    uint8_t  raw[LINK_MAX_RAW];
    size_t   n;
    bool     overflow;
    uint32_t ok, crc_err, len_err, cobs_err, overflows, gaps;
    int      last_seq;               // -1 until the first frame
} link_rx_t;

void link_rx_init(link_rx_t *rx);
// Feed one received byte. Returns true when *f holds a valid frame.
bool link_rx_push(link_rx_t *rx, uint8_t byte, link_frame_t *f);