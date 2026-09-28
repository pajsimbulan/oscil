#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "spi2_adc.h"
#include "sampler.h"
#include "testsig.h"
#include "burst_test.h"

#define BURST_N 3200 //2 x R for r = 1600
#define TESTSIG_PIN 47 //spare, header 28
#define TESTSIG_HZ 1000 
#define MID_CODE 2560 //halfway between the square's two levels (2062 and 3086)
#define BURST_DUMP 1 //1. print CH1 of the first rate between BEGIN and END for capture.py

  static uint16_t s_ch1[BURST_N];
  static uint16_t s_ch2[BURST_N];

//count rising crossings of MID_CODE, smaples per square period. averaged first to last.
static float samples_per_period(const uint16_t *b, size_t n) {
    int first = -1;
    int last = -1;
    int edges = 0;
    for(size_t i=1; i<n; i++) {
        if(b[i-1] < MID_CODE && b[i] >= MID_CODE) {
            if(first<0) first = (int) i;
            last = (int) i;
            edges++;
        }
    }
    return edges>1 ? (float)(last-first) / (float)(edges-1):0.0f;
}

static void min_max(const uint16_t *b, size_t n, uint16_t *mn, uint16_t *mx) {
    *mn = 4095;
    *mx = 0;
    for(size_t i=0; i<n; i++) {
        if(b[i] < *mn) *mn = b[i];
        if(b[i] > *mx) *mx = b[i];
    }
}

static void dump(const uint16_t *b, size_t n) {
    printf("BEGIN\n");
    for (size_t i = 0; i < n; i++) {
        printf("%u\n", b[i]);
        if ((i & 127) == 127) vTaskDelay(pdMS_TO_TICKS(10));   // let the USB console drain
    }
    printf("END\n");
}

void test_burst(void) {
    static const uint32_t RATES[] = { 100000, 1000000, 20000 };   // add your measured maximum
    const int NR = sizeof(RATES) / sizeof(RATES[0]);

    testsig_start(TESTSIG_PIN, TESTSIG_HZ);
    spi2_adc_init(3, ADC_MODE_FULL, true); //26.67 MHz, dual-line
    for(int i=0; i<3; i++) (void)spi2_adc_frame(); //ADS883 p.4 first frames invalid
    ESP_ERROR_CHECK(sampler_init(BURST_N)); //DMA owns GP-SPI2 from here

    
     while (1) {
        for (int r = 0; r < NR; r++) {
            sampler_info_t info = { 0 };
            esp_err_t e = sampler_capture(s_ch1, s_ch2, BURST_N, RATES[r], &info);
            if (e != ESP_OK) {
                printf("rate %lu: %s\n", (unsigned long)RATES[r], esp_err_to_name(e));
                continue;
            }
            uint16_t mn1, mx1, mn2, mx2;
            min_max(s_ch1, BURST_N, &mn1, &mx1);
            min_max(s_ch2, BURST_N, &mn2, &mx2);
            float spp = samples_per_period(s_ch1, BURST_N);
            float meas_apb = spp > 0 ? 80000000.0f / (spp * TESTSIG_HZ) : 0;   // true period
            printf("rate %7lu  period %5lu APB  measured %8.2f APB  (%.0f Sa/s)  "
                   "ch1 %u..%u  ch2 %u..%u\n",
                   (unsigned long)info.rate_hz, (unsigned long)info.period_apb,
                   meas_apb, spp * TESTSIG_HZ, mn1, mx1, mn2, mx2);
            if (BURST_DUMP && r == 0) dump(s_ch1, BURST_N);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}