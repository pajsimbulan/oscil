#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "oscil_pins_gen.h"
#include "oscil_status.h"

static const char *TAG = "gen";

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_bytes = 0;
    esp_flash_get_size(NULL, &flash_bytes);

    ESP_LOGI(TAG, "Oscil board 3: generator");
    ESP_LOGI(TAG, "cores %d, silicon v%d.%d",
             chip.cores, chip.revision / 100, chip.revision % 100);
    ESP_LOGI(TAG, "flash %" PRIu32 " MB", flash_bytes / (1024 * 1024));
    ESP_LOGI(TAG, "psram %u MB", (unsigned)(esp_psram_get_size() / (1024 * 1024)));
        ESP_LOGI(TAG, "tick %d Hz, LINK2 RX on GPIO%d",
             configTICK_RATE_HZ, GEN_LINK2_RX);
    ESP_ERROR_CHECK(oscil_status_init(GEN_STATUS_RGB));
    ESP_ERROR_CHECK(oscil_status_start_heartbeat(0,0,16)); //blue
    ESP_LOGI(TAG,"heartbeat on GPIO%d", GEN_STATUS_RGB);
}