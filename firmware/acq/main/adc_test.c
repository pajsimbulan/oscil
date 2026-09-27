#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "spi2_adc.h"
#include "oscil_afe.h"
#include "adc_test.h"

#define ADC_DIV 8 // 80 Mhz / 8 = 10 MHz to start, 20 for the analyzer capture
#define ADC_MODE 0

//measured: VREF offset = 0.75 x 2.214 V, +3V3_A = 3.314V
static oscil_afe_cal_t s_cal1 = {
    .gain = 4.0f,
    .offset_v = 1.672f,
    .vref = 3.314f
};

//never returns
void test_adc(void) {
    spi2_adc_init(ADC_DIV, ADC_MODE, false);

    //ADS7883 p.4: first frames after power-up are invalid
    for(int i=0; i<3; i++) (void)spi2_adc_frame();
        while (1) {
        uint32_t sum = 0;
        for (int i = 0; i < 64; i++) {
            sum += spi2_adc_single_code(spi2_adc_frame());
        }
        float code = sum / 64.0f;
        printf("ch1 code %.1f -> %.3f V at BNC\n", code,
               oscil_afe_code_to_volts((uint16_t)(code + 0.5f), &s_cal1));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}