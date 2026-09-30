#include <stdint.h>
#include "unity.h"
#include "oscil_dds.h"

void setUp(void) { dds_init(); }
void tearDown(void) {}

void test_tuning_word_for_1khz_at_100khz(void)
{
    TEST_ASSERT_UINT32_WITHIN(1, 42949673u, dds_tuning_word(1000.0, 100000.0));
}

void test_sine_frequency_by_counting_upward_crossings(void)
{
    dds_t d = {0};
    dds_set(&d, DDS_SINE, 1234.0, 100000.0, 0, 0, 255);
    int crossings = 0;
    uint8_t prev = dds_next(&d);
    for (int i = 1; i < 1000000; i++) {                  // 10 s of samples
        uint8_t s = dds_next(&d);
        if (prev <= 127 && s >= 128) crossings++;         // crosses 127.5 going up
        prev = s;
    }
    TEST_ASSERT_INT_WITHIN(1, 12340, crossings);
}

void test_square_10_percent_duty(void)
{
    dds_t d = {0};
    dds_set(&d, DDS_SQUARE, 1000.0, 100000.0, 0.10, 0, 255);
    int high = 0;
    for (int i = 0; i < 100000; i++) high += (dds_next(&d) == 255);
    TEST_ASSERT_INT_WITHIN(100, 10000, high);
}

void test_range_is_respected_and_reached(void)
{
    dds_t d = {0};
    dds_set(&d, DDS_SINE, 1000.0, 100000.0, 0, 50, 200);
    uint8_t mn = 255, mx = 0;
    for (int i = 0; i < 100000; i++) {
        uint8_t s = dds_next(&d);
        if (s < mn) mn = s;
        if (s > mx) mx = s;
    }
    TEST_ASSERT_EQUAL_UINT8(50, mn);
    TEST_ASSERT_EQUAL_UINT8(200, mx);
}

void test_sine_table_landmarks(void)
{
    TEST_ASSERT_UINT8_WITHIN(1, 128, dds_sine_table[0]);
    TEST_ASSERT_EQUAL_UINT8(255, dds_sine_table[64]);
    TEST_ASSERT_EQUAL_UINT8(0, dds_sine_table[192]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tuning_word_for_1khz_at_100khz);
    RUN_TEST(test_sine_frequency_by_counting_upward_crossings);
    RUN_TEST(test_square_10_percent_duty);
    RUN_TEST(test_range_is_respected_and_reached);
    RUN_TEST(test_sine_table_landmarks);
    return UNITY_END();
}