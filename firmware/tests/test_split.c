#include <stdint.h>
#include "unity.h"
#include "oscil_ads7883.h"

void setUp(void) {}
void tearDown(void) {}

//build a dual-line wire word the way hardware does,  bit pairs MSB first,
//Ch1 (Q) the higher bit of each pair, Ch2 (D) the lower.
static uint32_t interleave(uint16_t ch1, uint16_t ch2) {
    uint32_t w =0;
    for(int i=15; i>=0; i--) {
        w = (w << 1) | ((ch1 >> i) & 1u);
        w = (w << 1) | ((ch2 >> i) & 1u);
    }
    return w;
}

//tiny deterministic PRNG so the test is repeatable
static uint32_t s_seed = 12345u;
static uint32_t rnd(void) {
    s_seed = s_seed * 1664525u + 1013904223u;
    return s_seed;
}

void test_known_pair(void) {
    //Real ADC words: 0 0 D11...D0 0 0 (codse 2067 and 2720)
    uint16_t A = (uint16_t)(2067u << 2);
    uint16_t B = (uint16_t)(2720u << 2);
    uint32_t raw = __builtin_bswap32(interleave(A, B)); // how W0 stores it
    uint16_t c1;
    uint16_t c2;
    ads7883_split(raw, &c1, &c2);
    TEST_ASSERT_EQUAL_UINT16(2067,c1);
    TEST_ASSERT_EQUAL_UINT16(2720,c2);
}

void test_fast_matches_loop_random(void) {
    for(int n =0; n<10000; n++) {
        uint16_t A = (uint16_t)rnd();
        uint16_t B = (uint16_t)rnd();
        uint32_t w = interleave(A,B);

        uint16_t l1;
        uint16_t l2;
        deinterleave_loop(w, &l1, &l2);
        TEST_ASSERT_EQUAL_UINT16(A, l1);
        TEST_ASSERT_EQUAL_UINT16(B, l2);

        uint16_t f1;
        uint16_t f2;
        ads7883_split(__builtin_bswap32(w), &f1, &f2);
        TEST_ASSERT_EQUAL_UINT16(ads7883_decode(A), f1);
        TEST_ASSERT_EQUAL_UINT16(ads7883_decode(B), f2);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_known_pair);
    RUN_TEST(test_fast_matches_loop_random);
    return UNITY_END();
}
