#include <math.h>
#include "unity.h"
#include "oscil_measure.h"

static const oscil_afe_cal_t CAL = OSCIL_AFE_CAL_NOMINAL;
static uint16_t buf[1600];

void setUp(void) {}
void tearDown(void) {}

// 1 kHz at 160 kSa/s (1 ms/div): 160 samples per period, 10 periods in 1600 samples
static void make_sine(double hz, double rate, double amp_codes, double phase)
{
    for (int i = 0; i < 1600; i++)
        buf[i] = (uint16_t)lround(2048 + amp_codes * sin(2 * M_PI * hz * i / rate + phase));
}

void test_dc_gives_equal_extremes_and_no_frequency(void)
{
    for (int i = 0; i < 1600; i++) buf[i] = 3000;
    proto_meas_t m;
    oscil_measure(buf, 1600, 160000, &CAL, &m);
    TEST_ASSERT_EQUAL_FLOAT(m.vmin, m.vmax);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, m.vmin, m.vavg);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, m.freq_hz);
}

void test_sine_rms_is_peak_over_root2(void)
{
    make_sine(1000, 160000, 1500, 0.3);
    proto_meas_t m;
    oscil_measure(buf, 1600, 160000, &CAL, &m);
    float peak = (m.vmax - m.vmin) / 2;
    TEST_ASSERT_FLOAT_WITHIN(0.01f * peak, peak / sqrtf(2.0f), m.vrms);
}

void test_sine_frequency_within_0p1_percent(void)
{
    make_sine(1000, 160000, 1500, 0.3);
    proto_meas_t m;
    oscil_measure(buf, 1600, 160000, &CAL, &m);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000.0f, m.freq_hz);
}

void test_quarter_duty_square(void)
{
    for (int i = 0; i < 1600; i++) buf[i] = (i % 160) < 40 ? 3500 : 600;
    proto_meas_t m;
    oscil_measure(buf, 1600, 160000, &CAL, &m);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.25f, m.duty);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000.0f, m.freq_hz);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dc_gives_equal_extremes_and_no_frequency);
    RUN_TEST(test_sine_rms_is_peak_over_root2);
    RUN_TEST(test_sine_frequency_within_0p1_percent);
    RUN_TEST(test_quarter_duty_square);
    return UNITY_END();
}