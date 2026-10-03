#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "spi2_adc.h"
#include "cal.h"
#include "cal_test.h"

#define CAL_SPAN_V  3.273f      // set to the DMM reading of your span source before flashing
#define CAL_N       4096

static void countdown(const char *what, int s)
{
    printf("cal: %s, %d s\n", what, s);
    for (; s > 0; s--) { printf("  %d\n", s); vTaskDelay(pdMS_TO_TICKS(1000)); }
}

static float average_code(int ch)
{
    uint32_t sum = 0;
    for (int i = 0; i < CAL_N; i++) {
        uint16_t a, b;
        ads7883_split(spi2_adc_frame(), &a, &b);
        sum += ch ? b : a;
    }
    return (float)sum / CAL_N;
}

void test_cal(int ch)
{
    spi2_adc_init(3, ADC_MODE_FULL, true);                 // 26.67 MHz, both channels
    for (int i = 0; i < 3; i++) (void)spi2_adc_frame();    // ADS7883: first frames invalid

    oscil_afe_cal_t c = *cal_get(ch);
    countdown("probe to GND", 10);
    float z = average_code(ch);
    countdown("apply the span source", 15);
    float s = average_code(ch);

    // v = gain * (code * vref / 4096 - offset_v): zero -> 0 V, span -> CAL_SPAN_V
    float vz = z * c.vref / OSCIL_ADC_CODES, vs = s * c.vref / OSCIL_ADC_CODES;
    if (vs - vz < 0.01f) { printf("cal: span too small (%.1f vs %.1f codes), not stored\n", s, z); return; }
    c.offset_v = vz;
    c.gain = CAL_SPAN_V / (vs - vz);
    printf("cal CH%d: zero %.1f, span %.1f codes -> gain %.4f, offset %.4f V: %s\n",
           ch + 1, z, s, c.gain, c.offset_v, esp_err_to_name(cal_store(ch, &c)));
}