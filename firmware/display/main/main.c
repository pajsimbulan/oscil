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
#include "touch.h"
#include "touch_test.h"
#include "links.h"
#include "scope_view.h"
#include "ui.h"
#include "settings.h"

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

    ESP_ERROR_CHECK(settings_init());
    ui_settings_t s;
    if (settings_load(&s) != ESP_OK) { ui_defaults(&s); ESP_LOGW(TAG, "settings: defaults"); }

    esp_lcd_panel_handle_t panel = lcd_start(10, 2);                // 10-line bounce buffers, 2 frame buffers
    lv_display_t *disp = gui_start(panel);
    touch_start(disp);                                              // failure is logged; the self-test sees it
    const links_handlers_t lh = { .on_frame = scope_view_submit, .on_key = ui_on_key,
                                  .on_acq_state = ui_on_acq_state, .on_cal = scope_view_set_cal };
    ESP_ERROR_CHECK(links_start(&lh));
    const ui_hooks_t hooks = { .changed = settings_mark_dirty };
    ui_start(&s, &hooks);

    // test_lcd(panel);
}