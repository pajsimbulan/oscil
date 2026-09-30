#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "gen.h"
#include "gen_test.h"

#define FLASH_WRITE_TEST 0           // 1: 200 NVS writes while the first setting plays

static void flash_writes(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("gen_test", NVS_READWRITE, &h));
    for (uint32_t i = 0; i < 200; i++) {
        ESP_ERROR_CHECK(nvs_set_u32(h, "n", i));
        ESP_ERROR_CHECK(nvs_commit(h));
    }
    nvs_close(h);
    printf("200 NVS writes done\n");
}

void test_gen(void)
{
    static const struct { dds_shape_t shape; double hz, duty; const char *name; } T[] = {
        { DDS_SINE, 1000, 0, "sine 1 kHz" },           { DDS_SQUARE, 1000, 0.5, "square 1 kHz 50%" },
        { DDS_SQUARE, 1000, 0.1, "square 1 kHz 10%" }, { DDS_SAW, 1000, 0, "saw 1 kHz" },
        { DDS_TRIANGLE, 1000, 0, "tri 1 kHz" },        { DDS_SINE, 100, 0, "sine 100 Hz" },
        { DDS_SINE, 5000, 0, "sine 5 kHz" },           { DDS_SINE, 10000, 0, "sine 10 kHz" },
        { DDS_SINE, 20000, 0, "sine 20 kHz" },
    };
    for (int pass = 0;; pass++)
        for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
            gen_set(T[i].shape, T[i].hz, T[i].duty, 0, 255, true);
            printf("%s\n", T[i].name);
            if (FLASH_WRITE_TEST && pass == 0 && i == 0) flash_writes();
            vTaskDelay(pdMS_TO_TICKS(60000));
        }
}