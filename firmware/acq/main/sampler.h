#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct {
    uint32_t rate_hz; //rate actually achieved, not the requested
    size_t n; //sampels per channel
    uint32_t period_apb; //one sample per period in 12.5 ns APB cycles
} sampler_info_t;

//max_n: longest burst you will ask for.  Call once, after spi2_adc_init(3, ADC_MODE_FUL, true)
//Calling spi2_adc_init() again afterwards resets GP-SPI2 and undoes the DMA setuop.
esp_err_t sampler_init(size_t max_n);

//Blocks the cvalling task until tjhe DMA burst is done (it sleeps , it doesnt spin).
esp_err_t sampler_capture(uint16_t *ch1, uint16_t *ch2, size_t n, uint32_t rate_hz, sampler_info_t *info);