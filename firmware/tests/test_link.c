#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "oscil_link.h"

void setUp(void) {}
void tearDown(void) {}

void test_crc_check_value(void)
{
    // The published check value for CRC-16/CCITT-FALSE
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16_ccitt((const uint8_t *)"123456789", 9));
}

void test_cobs_output_never_contains_zero(void)
{
    uint8_t in[600], enc[700];
    for (int i = 0; i < 600; i++) in[i] = (i % 7 == 0) ? 0 : (uint8_t)i;
    size_t n = cobs_encode(in, 600, enc);
    for (size_t i = 0; i < n; i++) TEST_ASSERT_NOT_EQUAL(0, enc[i]);
}

void test_cobs_round_trip_edge_cases(void)
{
    // all zeros, no zeros for exactly 254 and 255 bytes, a single byte
    static const size_t sizes[] = { 1, 253, 254, 255, 508, 1000 };
    uint8_t in[1000], enc[1100], out[1100];
    for (size_t s = 0; s < 6; s++) {
        for (int fill = 0; fill < 2; fill++) {
            for (size_t i = 0; i < sizes[s]; i++) in[i] = fill ? 0xAB : 0x00;
            size_t e = cobs_encode(in, sizes[s], enc);
            TEST_ASSERT_EQUAL(sizes[s], cobs_decode(enc, e, out, sizeof out));
            TEST_ASSERT_EQUAL_MEMORY(in, out, sizes[s]);
        }
    }
}

static link_rx_t rx;
static uint8_t wire[LINK_MAX_WIRE];

static int feed(const uint8_t *w, size_t n, link_frame_t *f)
{
    int frames = 0;
    for (size_t i = 0; i < n; i++) frames += link_rx_push(&rx, w[i], f);
    return frames;
}

void test_frame_survives_the_wire(void)
{
    link_rx_init(&rx);
    uint8_t p[100]; for (int i = 0; i < 100; i++) p[i] = (uint8_t)(i * 37);
    size_t n = link_pack(0x10, 5, p, 100, wire);
    link_frame_t f;
    TEST_ASSERT_EQUAL(1, feed(wire, n, &f));
    TEST_ASSERT_EQUAL(0x10, f.type);
    TEST_ASSERT_EQUAL(100, f.len);
    TEST_ASSERT_EQUAL_MEMORY(p, f.payload, 100);
}

void test_one_flipped_bit_is_caught_and_next_frame_is_fine(void)
{
    link_rx_init(&rx);
    uint8_t p[50] = {1, 2, 3};
    size_t n = link_pack(0x10, 0, p, 50, wire);
    wire[n / 2] ^= 0x04;                       // corrupt, but not into a zero
    if (wire[n / 2] == 0) wire[n / 2] = 0x01;
    link_frame_t f;
    TEST_ASSERT_EQUAL(0, feed(wire, n, &f));
    TEST_ASSERT_EQUAL(1, rx.crc_err + rx.cobs_err + rx.len_err);
    n = link_pack(0x10, 1, p, 50, wire);
    TEST_ASSERT_EQUAL(1, feed(wire, n, &f));
}

void test_garbage_before_first_frame_is_ignored(void)
{
    link_rx_init(&rx);
    static const uint8_t junk[] = { 0x55, 0x12, 0x99, 0x00, 0x42, 0x00 };
    link_frame_t f;
    feed(junk, sizeof junk, &f);
    uint8_t p[4] = {9, 9, 9, 9};
    size_t n = link_pack(0x01, 0, p, 4, wire);
    TEST_ASSERT_EQUAL(1, feed(wire, n, &f));
}

void test_a_missing_frame_counts_as_a_gap(void)
{
    link_rx_init(&rx);
    uint8_t p[4] = {0};
    link_frame_t f;
    feed(wire, link_pack(1, 10, p, 4, wire), &f);
    feed(wire, link_pack(1, 12, p, 4, wire), &f);   // 11 never sent
    TEST_ASSERT_EQUAL(1, rx.gaps);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_check_value);
    RUN_TEST(test_cobs_output_never_contains_zero);
    RUN_TEST(test_cobs_round_trip_edge_cases);
    RUN_TEST(test_frame_survives_the_wire);
    RUN_TEST(test_one_flipped_bit_is_caught_and_next_frame_is_fine);
    RUN_TEST(test_garbage_before_first_frame_is_ignored);
    RUN_TEST(test_a_missing_frame_counts_as_a_gap);
    return UNITY_END();
}