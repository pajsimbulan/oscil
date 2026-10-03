#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "net.h"
#include "secrets.h"

static const char *TAG = "net";

#define BACKOFF_FIRST_MS 1000
#define BACKOFF_MAX_MS   60000

EventGroupHandle_t g_net_bits;
static TimerHandle_t s_retry;
static uint32_t s_backoff_ms;
static bool s_sntp_started;

static void retry_cb(TimerHandle_t t)
{
    esp_wifi_connect();                  // result arrives as another event
}

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = data;
        xEventGroupClearBits(g_net_bits, NET_ONLINE);                // waiters block again
        s_backoff_ms = s_backoff_ms ? s_backoff_ms * 2 : BACKOFF_FIRST_MS;
        if (s_backoff_ms > BACKOFF_MAX_MS) s_backoff_ms = BACKOFF_MAX_MS;
        ESP_LOGW(TAG, "disconnected (reason %d), retry in %lu ms", d->reason, (unsigned long)s_backoff_ms);
        xTimerChangePeriod(s_retry, pdMS_TO_TICKS(s_backoff_ms), 0);   // also starts it
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *ev = data;
        s_backoff_ms = 0;
        ESP_LOGI(TAG, "online, IP " IPSTR ", RSSI %d dBm", IP2STR(&ev->ip_info.ip), net_rssi());
        if (!s_sntp_started) {                                       // once; it re-syncs by itself
            esp_sntp_config_t c = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            esp_netif_sntp_init(&c);
            s_sntp_started = true;
        }
        xEventGroupSetBits(g_net_bits, NET_ONLINE);
    }
}

esp_err_t net_start(void)
{
    g_net_bits = xEventGroupCreate();
    s_retry = xTimerCreate("wifi_retry", pdMS_TO_TICKS(BACKOFF_FIRST_MS), pdFALSE, NULL, retry_cb);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t ic = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&ic));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL, NULL));

    wifi_config_t wc = { 0 };
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof wc.sta.ssid);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof wc.sta.password);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    return esp_wifi_start();                                         // STA_START comes next
}

bool net_online(void)
{
    return g_net_bits && (xEventGroupGetBits(g_net_bits) & NET_ONLINE);
}

int net_rssi(void)
{
    wifi_ap_record_t ap;
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
}