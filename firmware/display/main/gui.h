#pragma once
#include "esp_lcd_types.h"
#include "lvgl.h"

// Starts LVGL (esp_lvgl_port) on the RGB panel. The panel must have 2 frame buffers
// and bounce buffers on: lcd_start(10, 2). Every LVGL call from another task goes
// between lvgl_port_lock(0) and lvgl_port_unlock().
lv_display_t *gui_start(esp_lcd_panel_handle_t panel);