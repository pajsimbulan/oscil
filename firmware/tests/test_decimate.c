#include <stdint.h>
#include "unity.h"
#include "oscil_decimate.h"

#define N 1600
#define COLS 800

static uint16_t in[N];
static uint16_t mn[COLS];
static uint16_t mx[COLS];

void setUp(void) {}
void tearDown(void) {}

void test_one_sample_spike_survives(void) {
    for(int i=0; i<N; i++) in[i] = 2000;
    in[777] = 4000; //one sample, lands in column 388
    decimate_minmax(in, N, mn, mx, COLS);
    TEST_ASSERT_EQUAL_UINT16(4000, mx[388]);
    TEST_ASSERT_EQUAL_UINT16(2000, mn[388]);
}

void test_constant_stays_constant(void) {
    for(int i=0; i<N; i++) in[i] = 1234;
    decimate_minmax(in, N, mn, mx, COLS);
    for(int c = 0; c<COLS; c++) {
        TEST_ASSERT_EQUAL_UINT16(1234, mn[c]);
        TEST_ASSERT_EQUAL_UINT16(1234, mx[c]);
    }
}

void test_n_equal_cols_is_identity(void) {
    for(int i=0; i<COLS; i++) in[i] = (uint16_t)(i*5);
    decimate_minmax(in, COLS, mn, mx, COLS);
    for(int c = 0; c<COLS; c++) {
        TEST_ASSERT_EQUAL_UINT16(c*5, mn[c]);
        TEST_ASSERT_EQUAL_UINT16(c*5, mx[c]);
    }
}

void test_ramp_every_sample_covered(void) {
    const size_t n = 1003; //not a multiple of COLS
    for (size_t i=0; i<n; i++) in[i] = (uint16_t) i;
    decimate_minmax(in, n, mn, mx, COLS);
    TEST_ASSERT_EQUAL_UINT16(0, mn[0]);
    TEST_ASSERT_EQUAL_UINT16(n-1, mx[COLS-1]);
    for(int c = 1; c<COLS; c++) TEST_ASSERT_EQUAL_UINT16(mx[c-1]+1, mn[c]); // no gaps
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_one_sample_spike_survives);
    RUN_TEST(test_constant_stays_constant);
    RUN_TEST(test_n_equal_cols_is_identity);
    RUN_TEST(test_ramp_every_sample_covered);
    return UNITY_END();
}

