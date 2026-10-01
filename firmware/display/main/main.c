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
#include "esp_lvgl_port.h"

static const char *TAG = "disp";

static void on_frame(const uint8_t *p, uint16_t len) { scope_view_submit(p, len); }
static void on_key(const proto_key_t *k) { printf("key %u %u %d\n", k->type, k->id, k->delta); }
static void on_cal(const oscil_afe_cal_t cal[2]) { scope_view_set_cal(cal); }


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
    touch_start(disp); 
    const links_handlers_t lh = { .on_frame = on_frame, .on_key = on_key, .on_cal = on_cal };
    ESP_ERROR_CHECK(links_start(&lh));
    lvgl_port_lock(0);
    scope_view_init(lv_screen_active(), 40);
    view_t v = { .ch = { { true, 1.0f, 0, false, 0 }, { true, 1.0f, 0, false, 0 } }, .phosphor = false, .meas_panel = true };
    scope_view_set_view(&v);                                     // step 35: .phosphor = true to try it
    lvgl_port_unlock();

    //test_lcd(panel);
    //test_hello();
    //test_touch();
}