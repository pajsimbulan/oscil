#include "nvs.h"
#include "cal.h"

// Measured values from step 11 until a two-point calibration replaces them
static oscil_afe_cal_t s_cal[2] = {
    { .gain = 4.0f, .offset_v = 1.672f, .vref = 3.314f },
    { .gain = 4.0f, .offset_v = 1.672f, .vref = 3.314f },
};
static const char *KEY[2] = { "ch1", "ch2" };

void cal_load(void)
{
    nvs_handle_t h;
    if (nvs_open("cal", NVS_READONLY, &h) != ESP_OK) return;
    for (int ch = 0; ch < 2; ch++) {
        oscil_afe_cal_t c;
        size_t len = sizeof c;
        if (nvs_get_blob(h, KEY[ch], &c, &len) == ESP_OK && len == sizeof c && c.gain > 0) s_cal[ch] = c;
    }
    nvs_close(h);
}

const oscil_afe_cal_t *cal_get(int ch)
{
    return &s_cal[ch & 1];
}

esp_err_t cal_store(int ch, const oscil_afe_cal_t *c)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open("cal", NVS_READWRITE, &h);
    if (e != ESP_OK) return e;
    e = nvs_set_blob(h, KEY[ch & 1], c, sizeof *c);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    if (e == ESP_OK) s_cal[ch & 1] = *c;
    return e;
}