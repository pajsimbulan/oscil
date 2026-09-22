#include "unity.h"
#include "oscil_afe.h"

static const oscil_afe_cal_t CAL = OSCIL_AFE_CAL_NOMINAL;
#define LSB_VOLTS (4.0f *3.3f / 4096.0f)  //one adc step ~3.2mV

void setUp(void) {}
void tearDown(void) {}

void test_midscale_means_zero_volts(void) {
    TEST_ASSERT_FLOAT_WITHIN(LSB_VOLTS, 0.0f, oscil_afe_code_to_volts(2048, &CAL));
}

void test_plus_five_volts_lands_near_2v9_at_adc(void) {
    TEST_ASSERT_UINT16_WITHIN(1, 3600, oscil_afe_volts_to_code(5.0f, &CAL));
}

void test_round_trip_within_one_code(void) {
    for(float v = -5.0f; v <=5.0f; v += 0.25f) {
        uint16_t code = oscil_afe_volts_to_code(v, &CAL);
        TEST_ASSERT_FLOAT_WITHIN(LSB_VOLTS, v, oscil_afe_code_to_volts(code, &CAL));
    }
}

void test_out_of_range_clamps(void) {
    TEST_ASSERT_EQUAL_UINT16(4095, oscil_afe_volts_to_code(50.0f, &CAL));
    TEST_ASSERT_EQUAL_UINT16(0, oscil_afe_volts_to_code(-50.0f, &CAL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_midscale_means_zero_volts);
    RUN_TEST(test_plus_five_volts_lands_near_2v9_at_adc);
    RUN_TEST(test_round_trip_within_one_code);
    RUN_TEST(test_out_of_range_clamps);
    return UNITY_END();
}