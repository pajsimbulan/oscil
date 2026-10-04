#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cloud_http.h"
#include "secrets.h"

static const char *TAG = "cloud";

esp_err_t cloud_json(esp_http_client_method_t method, const char *path, const char *bearer,
                     const char *prefer, const char *body, int *status, char *resp, size_t n)
{
    *status = -1;
    resp[0] = 0;
    char url[256];
    snprintf(url, sizeof url, "%s%s", SUPABASE_URL, path);
    const esp_http_client_config_t cfg = {
        .url = url, .method = method,
        .crt_bundle_attach = esp_crt_bundle_attach,     // TLS checked against the bundled root CAs
        .timeout_ms = 15000,
        .buffer_size = 2048, .buffer_size_tx = 2048,     // an access token header is about 1 KB
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return ESP_ERR_NO_MEM;
    esp_http_client_set_header(c, "apikey", SUPABASE_ANON_KEY);
    esp_http_client_set_header(c, "content-type", "application/json");
    if (prefer) esp_http_client_set_header(c, "Prefer", prefer);
    if (bearer) {
        size_t m = strlen(bearer) + 8;
        char *auth = malloc(m);                          // the header list keeps its own copy
        if (!auth) { esp_http_client_cleanup(c); return ESP_ERR_NO_MEM; }
        snprintf(auth, m, "Bearer %s", bearer);
        esp_http_client_set_header(c, "Authorization", auth);
        free(auth);
    }

    int len = body ? (int)strlen(body) : 0;
    esp_err_t e = esp_http_client_open(c, len);          // request line + headers
    if (e == ESP_OK && len > 0 && esp_http_client_write(c, body, len) != len) e = ESP_FAIL;
    if (e == ESP_OK) {
        if (esp_http_client_fetch_headers(c) < 0) {
            e = ESP_FAIL;
        } else {
            *status = esp_http_client_get_status_code(c);
            int got = 0;

            while (got < (int)n - 1) {
                int r = esp_http_client_read(
                    c, resp + got, (int)n - 1 - got);
                if (r < 0) {
                    e = ESP_FAIL;
                    break;
                }
                if (r == 0) {
                    if (!esp_http_client_is_complete_data_received(c))
                        e = ESP_FAIL;
                    break;
                }
                got += r;
            }

            resp[got] = 0;
        }
    }

    if (e != ESP_OK)
        ESP_LOGW(TAG, "%s: %s", path, esp_err_to_name(e));
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return e;
}