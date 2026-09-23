#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "oscil_pins_display.h"
#include "oscil_status.h"
#include "oscil_pinwalk.h"

static const char *TAG = "disp";
#include "oscil_pinwalk.h"

static const oscil_pin_t DISP_PINS[] = {
    { DISP_LCD_R5, "R5" },  { DISP_LCD_R6, "R6" },  { DISP_LCD_R7, "R7" },  { DISP_LCD_G2, "G2" },
    { DISP_LCD_G3, "G3" },  { DISP_LCD_G4, "G4" },  { DISP_LCD_B5, "B5" },  { DISP_LCD_B6, "B6" },
    { DISP_LCD_B7, "B7" },  { DISP_LCD_G5, "G5" },  { DISP_LCD_DCLK, "DCLK" }, { DISP_LCD_G7, "G7" },
    { DISP_LCD_B3, "B3" },  { DISP_LCD_HSYNC, "HSYNC" }, { DISP_LCD_VSYNC, "VSYNC" }, { DISP_LCD_DE, "DE" },
    { DISP_TOUCH_SDA, "SDA" }, { DISP_TOUCH_SCL, "SCL" }, { DISP_TOUCH_INT, "T_INT" }, { DISP_LINK2_TX, "LINK2_TX" },
    { DISP_LCD_R3, "R3" },  { DISP_LINK1_TX, "LINK1_TX" }, { DISP_LINK1_RX, "LINK1_RX" }, { DISP_LCD_R4, "R4" },
    { DISP_LCD_G6, "G6" },  { DISP_LCD_B4, "B4" },
};

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_bytes = 0;
    esp_flash_get_size(NULL, &flash_bytes);

    ESP_LOGI(TAG, "Oscil board 2: display");
    ESP_LOGI(TAG, "cores %d, silicon v%d.%d",
             chip.cores, chip.revision / 100, chip.revision % 100);
    ESP_LOGI(TAG, "flash %" PRIu32 " MB", flash_bytes / (1024 * 1024));
    ESP_LOGI(TAG, "psram %u MB", (unsigned)(esp_psram_get_size() / (1024 * 1024)));
    ESP_LOGI(TAG, "tick %d Hz, LINK1 on GPIO%d/%d",
             configTICK_RATE_HZ, DISP_LINK1_TX, DISP_LINK1_RX);
    ESP_ERROR_CHECK(oscil_status_init(DISP_TOUCH_RESET));
    ESP_ERROR_CHECK(oscil_status_start_heartbeat(16,0,0)); //red
    ESP_LOGI(TAG,"heartbeat on GPIO%d", DISP_TOUCH_RESET);
    test_pinwalk(DISP_PINS, sizeof DISP_PINS / sizeof DISP_PINS[0]);
}