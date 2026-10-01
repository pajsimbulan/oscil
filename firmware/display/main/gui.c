#include "gui.h"
#include "esp_err.h"
#include "esp_lvgl_port.h"
#include "lcd.h"

lv_display_t *gui_start(esp_lcd_panel_handle_t panel)
{
    lvgl_port_cfg_t lcfg = ESP_LVGL_PORT_INIT_CONFIG();
    lcfg.task_affinity = 1;          // render on core 1; the RGB ISR and the links stay on core 0
    ESP_ERROR_CHECK(lvgl_port_init(&lcfg));

    const lvgl_port_display_cfg_t dcfg = {
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * LCD_V_RES,   // direct mode needs full-screen buffers
        .hres = LCD_H_RES, .vres = LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = { .buff_spiram = true, .direct_mode = true },
    };
    const lvgl_port_display_rgb_cfg_t rcfg = {
        .flags = { .bb_mode = true, .avoid_tearing = true },
    };
    lv_display_t *d = lvgl_port_add_disp_rgb(&dcfg, &rcfg);
    ESP_ERROR_CHECK(d ? ESP_OK : ESP_FAIL);
    return d;
}