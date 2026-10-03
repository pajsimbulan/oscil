#pragma once
#include <stdbool.h>
#include "esp_err.h"

const char *ota_version(void);               // PROJECT_VER from display/CMakeLists.txt
esp_err_t ota_request_update(void);          // save the request, then reboot into update mode
esp_err_t ota_boot_if_requested(bool *started);  // after NVS, before LCD
void ota_confirm_or_roll_back(void);         // self-test after normal startup of a new image
bool ota_is_running(void);