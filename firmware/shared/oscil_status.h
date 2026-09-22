#pragma once
#include <stdint.h>
#include "esp_err.h"

esp_err_t oscil_status_init(int gpio);
void oscil_status_set(uint8_t r, uint8_t g, uint8_t b);
esp_err_t oscil_status_start_heartbeat(uint8_t r, uint8_t g, uint8_t b);

