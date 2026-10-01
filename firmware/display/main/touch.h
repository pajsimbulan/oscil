#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

// GT911 on I2C0, address-select sequence for 0x5D, registered as LVGL's pointer.
esp_err_t touch_start(lv_display_t *disp);
bool touch_ok(void);                    // true once the GT911 answered (OTA self-test)