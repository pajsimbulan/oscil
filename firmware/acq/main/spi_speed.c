#include <stdio.h>
#include <stdbool.h>
#include "spi2_adc.h"
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define N 10000

typedef struct {
    int div;
    int mode;
    bool dual;
} spi_config; 

static const spi_config CFG[] = {
     { 8, 1, false }, { 8, 1, true },          // 10 MHz: slow enough for the analyzer
    { 3, 0, false }, { 3, 1, false }, { 3, 1, true },   // 26.67 MHz: the real ones
};

void test_spi_speed(void) //never returns
{
    while(1) {
        for(int c=0; c<(sizeof(CFG) / sizeof(CFG[0])); c++) {
            spi2_adc_init(CFG[c].div, CFG[c].mode, CFG[c].dual);

            volatile uint32_t sink = 0;
            uint32_t irq = portSET_INTERRUPT_MASK_FROM_ISR();
            uint32_t t0 = esp_cpu_get_cycle_count();
            for(int i=0; i<N; i++) sink += spi2_adc_frame();
            uint32_t cyc = esp_cpu_get_cycle_count() - t0;
            portCLEAR_INTERRUPT_MASK_FROM_ISR(irq);

            float per = (float)cyc / N; //cpu cycles per frame, 4.17ns each
            printf("SCLK %5.2f MHz  %-6s  mode %d  %6.1f cycles  %5.0f ns  %5.0f kSa/s\n",
                   80.0f / CFG[c].div, CFG[c].dual ? "dual" : "single", CFG[c].mode,
                   per, per * 1000.0f / 240.0f, 240000.0f / per);
            
            //keep framing for 2s so the analyzer can catch this setting
            TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(2000);
            while(xTaskGetTickCount() < end) sink += spi2_adc_frame();
        }
        printf("---\n");
    }
} 