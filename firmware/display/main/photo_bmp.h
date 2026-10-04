#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct { uint32_t width, height, offset, row_bytes; bool top_down; } photo_bmp_t;
static inline uint32_t photo_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
// Our screenshots are uncompressed 24-bit BMPs. Reject other formats before allocation.
static inline bool photo_bmp_header(const uint8_t *p, size_t n, photo_bmp_t *b)
{
    if (n < 54 || p[0] != 'B' || p[1] != 'M' || photo_le32(p + 14) != 40 ||
        p[26] != 1 || p[27] || p[28] != 24 || p[29] || photo_le32(p + 30)) return false;
    uint32_t w = photo_le32(p + 18), raw_h = photo_le32(p + 22);
    bool top = (raw_h & 0x80000000u) != 0;
    uint32_t h = top ? 0u - raw_h : raw_h, off = photo_le32(p + 10);
    if (!w || w > 800 || !h || h > 480 || off < 54 || off > 1024) return false;
    uint32_t row = (w * 3 + 3) & ~3u;
    if (photo_le32(p + 2) != off + row * h) return false;
    *b = (photo_bmp_t){ w, h, off, row, top };
    return true;
}
static inline uint16_t photo_rgb565(const uint8_t *bgr)
{
    return (uint16_t)((bgr[2] & 0xf8) << 8 | (bgr[1] & 0xfc) << 3 | bgr[0] >> 3);
}
