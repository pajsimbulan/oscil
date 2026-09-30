#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "r2r.h"
#include "r2r_test.h"

void test_dac_bits(void)
{
    static const uint8_t CODES[] = { 0, 1, 2, 4, 8, 16, 32, 64, 128, 255 };
    for (;;)
        for (size_t i = 0; i < sizeof CODES; i++) {
            r2r_write(CODES[i]);
            printf("code %3u\n", CODES[i]);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
}

void test_dac_ramp(void)
{
    for (int n = 0;; n++) {
        for (int c = 0; c < 256; c++) {
            r2r_write((uint8_t)c);
            esp_rom_delay_us(20);
        }
        if ((n & 63) == 0) vTaskDelay(1);    // let the idle task feed the watchdog
    }
}