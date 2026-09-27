#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "spi2_adc.h"
#include "oscil_afe.h"
#include "adc_test.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define ADC_DIV 8 // 80 Mhz / 8 = 10 MHz to start, 20 for the analyzer capture
#define ADC_MODE 0
#define NOISE_N 16384
#define NOISE_DUMP 1 //1: also print the mode 1 sampels between BEGIN and END for capture.py

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

static void adc_stats(int mode, bool dump) {
    spi2_adc_init(3, mode, false);
    for(int i=0; i<3; i++) (void)spi2_adc_frame();
    uint16_t *buf = malloc(NOISE_N * sizeof(*buf));
    if(!buf) {
        printf("no memory\n");
        return;
    }
    for(int i=0; i<NOISE_N; i++) buf[i] = spi2_adc_single_code(spi2_adc_frame());
    double mean = 0;
    double var = 0;
    uint16_t mn = 4095;
    uint16_t mx = 0;
    for(int i=0; i<NOISE_N; i++) {
        mean += buf[i];
        if(buf[i] < mn) mn = buf[i];
        if(buf[i] > mx) mx = buf[i];
    }
    mean /= NOISE_N;
    for(int i=0; i<NOISE_N; i++) var += (buf[i] - mean) * (buf[i] - mean);
    double rms = sqrt(var / NOISE_N);
    printf("mode %d  n %d  mean %.2f  rms %.2f codes (%.2f mV at BNC)  min %u max %u\n",
           mode, NOISE_N, mean, rms, rms * 4.0 * 3314.0 / 4096.0, mn, mx);
        if(dump) {
        printf("BEGIN\n");
        for(int i=0; i<NOISE_N; i++) {
            printf("%u\n", buf[i]);
            if ((i & 127) == 127) vTaskDelay(pdMS_TO_TICKS(10));  // let the USB console drain
        }
        printf("END\n");
    }
    free(buf);
}

void test_noise(void) //never returns
{
    while(1) {
        adc_stats(0, false);
        adc_stats(1, NOISE_DUMP);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

//never returns
void test_dual(void) {
    spi2_adc_init(3, ADC_MODE_FULL, true); //26.67 MHz, dual-line
    for(int i=0; i<3; i++) (void) spi2_adc_frame();
    while(1) {
        uint32_t s1 =0;
        uint32_t s2 =0;
        for(int i=0; i<64; i++) {
            uint16_t a;
            uint16_t b;
            ads7883_split(spi2_adc_frame(), &a, &b);
            s1 +=a;
            s2 +=b;
        }
        printf("ch1 %.1f  ch2 %.1f  diff %.1f\n", s1 / 64.0f, s2 / 64.0f, (s1 - (float)s2) / 64.0f);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
} 