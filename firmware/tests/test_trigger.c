#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "unity.h"
#include "oscil_trigger.h"

#define N 1000
#define PI_F 3.14159265f

static uint16_t buf[N];

void setUp(void) {}
void tearDown(void) {}

// 2048 + 1000 sin(2 pi ((float)i + phase) / period) + uniform noise of +-noise_amp/2, rounded
static void make_sine(float period, float phase, float noise_amp, unsigned seed) {
    srand(seed);
    for(int i=0; i< N; i++) {
        float n = noise_amp * ((float)rand() / (float)RAND_MAX - 0.5f);
        buf[i] = (uint16_t)lroundf(2048.0f + 1000.0f * sinf(2.0f * PI_F * ((float)i + phase) / period) + n);
    }
}

void test_rising_on_clean_sine_is_where_the_math_says(void) {
    make_sine(200, 0, 0, 1); //rises through 2048 at i=0, 200, 400...
    trig_cfg_t c = {2048, 20, TRIG_RISING};
    float f;
    long i = trig_find(buf, 100, N, &c, &f); //arms on the way down after 100
    TEST_ASSERT_EQUAL(200, i);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 200.0f, (float)(i-1) +f);
}

void test_fraction_finds_a_crossing_between_samples(void) {
    make_sine(200, 0.3f, 0, 1); //true crossing at 199.7
    trig_cfg_t c = {2048, 20, TRIG_RISING};
    float f;
    long i = trig_find(buf, 100, N, &c, &f);     // arms on the way down after 100
    TEST_ASSERT_EQUAL(200, i);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 199.7f, (float)(i - 1) + f);
}

void test_hysteresis_rejects_chatter_at_the_level(void) {
    for (int i=0; i < N; i++) buf[i] = (uint16_t) (2048 + ((i%2) ? 10: -10));
    trig_cfg_t c = {2048, 30, TRIG_RISING};
    TEST_ASSERT_EQUAL(-1, trig_find(buf, 0, N, &c, NULL));
}

void test_falling_edge(void) {
    make_sine(200, 0, 0, 1);
    trig_cfg_t c = {2048, 20, TRIG_FALLING};
    TEST_ASSERT_EQUAL(100, trig_find(buf, 1, N, &c, NULL));
}

void test_noisy_sine_triggers_once_per_period(void) {
    make_sine(200, 0, 60, 7); // +- 30 codes of noise, hysteresis 50
    trig_cfg_t c = {2048, 50, TRIG_RISING};
    long a = trig_find(buf, 50, N, &c, NULL);
    long b = trig_find(buf, (size_t)a+1, N, &c, NULL);
    TEST_ASSERT_TRUE(a>0);
    TEST_ASSERT_INT_WITHIN(5, 200, b-a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rising_on_clean_sine_is_where_the_math_says);
    RUN_TEST(test_fraction_finds_a_crossing_between_samples);
    RUN_TEST(test_hysteresis_rejects_chatter_at_the_level);
    RUN_TEST(test_falling_edge);
    RUN_TEST(test_noisy_sine_triggers_once_per_period);
    return UNITY_END();
}