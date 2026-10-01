#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "oscil_pins_display.h"
#include "lcd.h"
#include "lcd_test.h"
#include "gui.h"
#include "gui_test.h"

static const char *TAG = "disp";

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash_bytes = 0;
    esp_flash_get_size(NULL, &flash_bytes);
    ESP_LOGI(TAG, "Oscil board 2: display");
    ESP_LOGI(TAG, "cores %d, silicon v%d.%d, flash %" PRIu32 " MB, psram %u MB",
             chip.cores, chip.revision / 100, chip.revision % 100,
             flash_bytes / (1024 * 1024), (unsigned)(esp_psram_get_size() / (1024 * 1024)));
    // Status LED retired: GPIO48 is TOUCH_RESET, and its 30 us masked window
    // would upset the RGB bounce-buffer refill interrupt.

    esp_lcd_panel_handle_t panel = lcd_start(10, 2);                // 10-line bounce buffers, 2 frame buffers
    lv_display_t *disp = gui_start(panel);
    (void)disp;

    //test_lcd(panel);
    test_hello();
}