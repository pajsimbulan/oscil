#pragma once
#include <stdbool.h>
#include "esp_lcd_types.h"

#define LCD_H_RES 800
#define LCD_V_RES 480

// Starts the RGB panel. bounce_lines 0 = no bounce buffers; num_fbs 1 or 2.
esp_lcd_panel_handle_t lcd_start(int bounce_lines, int num_fbs);
bool lcd_ok(void);                      // true once the panel is running (OTA self-test)