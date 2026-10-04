#pragma once

#include "esp_err.h"
#include "ui.h"

// Start the cloud screenshot worker once, after ui_start().
esp_err_t upload_start(void);

// SAVE hook. Caller holds the LVGL lock.
// Queues one screenshot request; never performs network work here.
void upload_save(const ui_settings_t *s);