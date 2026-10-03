#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "oscil_link.h"          // crc16_ccitt()
#include "settings.h"

static const char *TAG = "settings";

#define SETTINGS_VERSION 1       // bump whenever ui_settings_t changes
#define QUIET_MS         2000

typedef struct { uint16_t version; uint16_t crc; ui_settings_t s; } settings_blob_t;

static ui_settings_t s_pending;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;

esp_err_t settings_load(ui_settings_t *out)
{
    settings_blob_t b;
    size_t len = sizeof b;
    nvs_handle_t h;
    if (nvs_open("ui", NVS_READONLY, &h) != ESP_OK) return ESP_ERR_NOT_FOUND;
    esp_err_t e = nvs_get_blob(h, "session", &b, &len);
    nvs_close(h);
    if (e != ESP_OK || len != sizeof b || b.version != SETTINGS_VERSION ||
        b.crc != crc16_ccitt((const uint8_t *)&b.s, sizeof b.s)) return ESP_ERR_INVALID_STATE;
    *out = b.s;
    return ESP_OK;
}

esp_err_t settings_save(const ui_settings_t *in)
{
    settings_blob_t b = { SETTINGS_VERSION, crc16_ccitt((const uint8_t *)in, sizeof *in), *in };
    nvs_handle_t h;
    esp_err_t e = nvs_open("ui", NVS_READWRITE, &h);
    if (e != ESP_OK) return e;                           // never abort the UI over a save
    e = nvs_set_blob(h, "session", &b, sizeof b);
    if (e == ESP_OK) e = nvs_commit(h);                  // the moment it's actually safe
    nvs_close(h);
    return e;
}

// Debounce without a timer: wait for a change, then until 2 s pass with no further change
static void save_task(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(QUIET_MS))) {}
        ui_settings_t copy;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        copy = s_pending;
        xSemaphoreGive(s_lock);
        esp_err_t e = settings_save(&copy);
        ESP_LOGI(TAG, "saved: %s", esp_err_to_name(e));
    }
}

void settings_mark_dirty(const ui_settings_t *s)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_pending = *s;
    xSemaphoreGive(s_lock);
    xTaskNotifyGive(s_task);
}

esp_err_t settings_init(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    if (e != ESP_OK) return e;
    s_lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(save_task, "settings", 3072, NULL, 2, &s_task, 0);
    return ESP_OK;
}