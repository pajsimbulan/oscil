#pragma once
#include "esp_err.h"
#include "ui.h"

esp_err_t settings_init(void);                    // NVS up (erase and retry if needed), save task started
esp_err_t settings_load(ui_settings_t *out);      // ESP_OK only for a blob with our version and a good CRC
esp_err_t settings_save(const ui_settings_t *in);
void      settings_mark_dirty(const ui_settings_t *s);   // saved once 2 s pass without another change