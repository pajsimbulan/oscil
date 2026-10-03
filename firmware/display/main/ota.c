#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "nvs.h"
#include "lcd.h"
#include "links.h"
#include "net.h"
#include "ota.h"
#include "secrets.h"
#include "touch.h"

static const char *TAG = "ota";
static volatile bool s_running;

const char *ota_version(void)
{
    return esp_app_get_description()->version;
}

bool ota_is_running(void)
{
    return s_running;
}

// UPDATE requests one update boot. Downloads never run beside the RGB panel.
esp_err_t ota_request_update(void)
{
    if (s_running) return ESP_ERR_INVALID_STATE;
    nvs_handle_t h;
    esp_err_t e = nvs_open("ota", NVS_READWRITE, &h);
    if (e != ESP_OK) return e;
    e = nvs_set_u8(h, "pending", 1);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    if (e != ESP_OK) return e;

    ESP_LOGI(TAG, "update requested, rebooting without LCD");
    esp_restart();
    return ESP_OK;
}

static void ota_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "update mode: LCD, touch and UI disabled");
    const links_handlers_t lh = { 0 };
    esp_err_t e = links_start(&lh);              // drain incoming frames without UI callbacks
    if (e == ESP_OK) e = net_start();
    if (e == ESP_OK) {
        xEventGroupWaitBits(g_net_bits, NET_ONLINE,
                            pdFALSE, pdTRUE, pdMS_TO_TICKS(30000));
        if (!net_online()) e = ESP_ERR_TIMEOUT;
    }

    if (e == ESP_OK) {
        const esp_http_client_config_t http = {
            .url = OTA_URL,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .timeout_ms = 15000,
            .keep_alive_enable = true,
            .buffer_size = 4096,                // GitHub's redirect Location header is long
            .buffer_size_tx = 2048,
        };
        const esp_https_ota_config_t cfg = { .http_config = &http };
        ESP_LOGI(TAG, "running %s, fetching %s", ota_version(), OTA_URL);
        e = esp_https_ota(&cfg);
    }

    if (e == ESP_OK) {
        ESP_LOGI(TAG, "done, rebooting");
    } else {
        ESP_LOGE(TAG, "failed: %s; returning to normal startup", esp_err_to_name(e));
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
    esp_restart();                              // pending was cleared before this task started
}

// Call after settings_init(), before starting the LCD or any UI tasks.
esp_err_t ota_boot_if_requested(bool *started)
{
    if (!started) return ESP_ERR_INVALID_ARG;
    *started = false;
    nvs_handle_t h;
    esp_err_t e = nvs_open("ota", NVS_READWRITE, &h);
    if (e != ESP_OK) return e;

    uint8_t pending = 0;
    e = nvs_get_u8(h, "pending", &pending);
    if (e == ESP_ERR_NVS_NOT_FOUND) e = ESP_OK;
    if (e != ESP_OK || !pending) {
        nvs_close(h);
        return e;
    }

    e = nvs_set_u8(h, "pending", 0);              // consume first: a crash must not start an update loop
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    if (e != ESP_OK) return e;

    s_running = true;
    if (xTaskCreatePinnedToCore(ota_task, "ota", 8192,
                               NULL, 3, NULL, 0) != pdPASS) {
        s_running = false;
        return ESP_ERR_NO_MEM;
    }
    *started = true;
    return ESP_OK;
}

void ota_confirm_or_roll_back(void)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(run, &st) != ESP_OK || st != ESP_OTA_IMG_PENDING_VERIFY) return;

    ESP_LOGI(TAG, "%s is new: self-test", ota_version());
    for (int i = 0; i < 50 && links_ms_since_frame() > 1000; i++) vTaskDelay(pdMS_TO_TICKS(100));
    bool ok = lcd_ok() && touch_ok() && links_ms_since_frame() < 1000;   // board 1 frames arriving

    if (ok) {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "new image confirmed");
    } else {
        ESP_LOGE(TAG, "self-test failed (lcd %d touch %d link %lu ms), rolling back",
                 lcd_ok(), touch_ok(), (unsigned long)links_ms_since_frame());
        esp_ota_mark_app_invalid_rollback_and_reboot();  // boots the previous slot
    }
}