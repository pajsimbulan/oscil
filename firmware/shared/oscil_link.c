#include <string.h>
#include "oscil_link.h"

uint16_t crc16_ccitt(const uint8_t *d, size_t n)
{
    uint16_t c = 0xFFFF;
    while (n--) {
        c ^= (uint16_t)(*d++) << 8;
        for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}

size_t cobs_encode(const uint8_t *in, size_t n, uint8_t *out)
{
    size_t r = 1, code_at = 0;
    uint8_t code = 1;
    for (size_t i = 0; i < n; i++) {
        if (in[i] == 0) {
            out[code_at] = code; code_at = r++; code = 1;
        } else {
            out[r++] = in[i];
            if (++code == 0xFF) { out[code_at] = code; code_at = r++; code = 1; }
        }
    }
    out[code_at] = code;
    return r;
}

size_t cobs_decode(const uint8_t *in, size_t n, uint8_t *out, size_t cap)
{
    size_t r = 0, i = 0;
    while (i < n) {
        uint8_t code = in[i++];
        if (code == 0) return 0;
        for (uint8_t k = 1; k < code; k++) {
            if (i >= n || r >= cap) return 0;       // truncated, or garbage longer than out
            out[r++] = in[i++];
        }
        if (code != 0xFF && i < n) {
            if (r >= cap) return 0;
            out[r++] = 0;
        }
    }
    return r;
}

size_t link_pack(uint8_t type, uint8_t seq, const void *payload, uint16_t len, uint8_t *wire)
{
    static uint8_t raw[LINK_MAX_RAW];          // shared by every link: callers serialise (link_send's mutex)
    raw[0] = type; raw[1] = seq; raw[2] = len & 0xFF; raw[3] = len >> 8;
    memcpy(&raw[4], payload, len);
    uint16_t c = crc16_ccitt(raw, 4 + len);
    raw[4 + len] = c & 0xFF; raw[5 + len] = c >> 8;
    size_t w = cobs_encode(raw, 6 + len, wire);
    wire[w++] = 0x00;
    return w;
}

void link_rx_init(link_rx_t *rx) { memset(rx, 0, sizeof *rx); rx->last_seq = -1; }

bool link_rx_push(link_rx_t *rx, uint8_t b, link_frame_t *f)
{
    if (b != 0x00) {
        if (rx->n < sizeof rx->enc) rx->enc[rx->n++] = b;
        else rx->overflow = true;
        return false;
    }
    size_t n = rx->n; bool ovf = rx->overflow;
    rx->n = 0; rx->overflow = false;
    if (n == 0) return false;                               // back-to-back delimiters
    if (ovf) { rx->overflows++; return false; }

    size_t m = cobs_decode(rx->enc, n, rx->raw, sizeof rx->raw);
    if (m < 6) { rx->cobs_err++; return false; }
    uint16_t len = rx->raw[2] | (rx->raw[3] << 8);
    if (m != (size_t)len + 6) { rx->len_err++; return false; }
    uint16_t got = rx->raw[4 + len] | (rx->raw[5 + len] << 8);
    if (crc16_ccitt(rx->raw, 4 + len) != got) { rx->crc_err++; return false; }

    f->type = rx->raw[0]; f->seq = rx->raw[1]; f->len = len; f->payload = &rx->raw[4];
    if (rx->last_seq >= 0) rx->gaps += (uint8_t)(f->seq - rx->last_seq - 1);
    rx->last_seq = f->seq;
    rx->ok++;
    return true;
}