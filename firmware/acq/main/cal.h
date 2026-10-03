#pragma once
#include "esp_err.h"
#include "oscil_afe.h"

void cal_load(void);                                        // NVS "cal"/"ch1","ch2"; nominal if absent
const oscil_afe_cal_t *cal_get(int ch);                     // 0 = CH1, 1 = CH2
esp_err_t cal_store(int ch, const oscil_afe_cal_t *c);