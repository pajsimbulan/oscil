#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "nvs.h"
#include "lvgl.h"
#include "cJSON.h"

#include "account.h"
#include "cloud_http.h"
#include "net.h"
#include "secrets.h"
#include "ui.h"
#include "upload.h"

static const char *TAG = "upload";

#define JWT_MAX 1536

typedef struct {
    lv_obj_t *screen;
    char user[24];
} save_job_t;

static QueueHandle_t s_queue;
static SemaphoreHandle_t s_slot;

// Only a counter goes into NVS. Photo bytes never go into flash.
static esp_err_t next_id(const char *jwt, const char *uid, uint32_t *id)
{
    // The account outlives this board. Never restart at an existing cloud ID.
    char path[160], resp[256];
    snprintf(path, sizeof path,
             "/rest/v1/screenshots?select=shot_id&owner=eq.%s&order=shot_id.desc&limit=1", uid);
    int status;
    esp_err_t e = cloud_json(HTTP_METHOD_GET, path, jwt, NULL, NULL,
                             &status, resp, sizeof resp);
    if (e != ESP_OK) return e;
    if (status == 401) return ESP_ERR_INVALID_STATE;
    if (status != 200) {
        ESP_LOGW(TAG, "photo numbering HTTP %d: %s", status, resp);
        return ESP_FAIL;
    }
    cJSON *rows = cJSON_Parse(resp);
    if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) > 1) {
        cJSON_Delete(rows);
        return ESP_ERR_INVALID_RESPONSE;
    }
    uint32_t minimum = 1;
    if (cJSON_GetArraySize(rows)) {
        cJSON *last = cJSON_GetObjectItem(cJSON_GetArrayItem(rows, 0), "shot_id");
        if (!cJSON_IsNumber(last) || last->valuedouble < 1 ||
            last->valuedouble >= INT32_MAX - 1 ||
            last->valuedouble != (double)last->valueint) {
            cJSON_Delete(rows);
            return ESP_ERR_INVALID_RESPONSE;
        }
        minimum = (uint32_t)last->valueint + 1;
    }
    cJSON_Delete(rows);

    nvs_handle_t h;
    e = nvs_open("photos", NVS_READWRITE, &h);
    if (e != ESP_OK) return e;

    uint32_t next = 1;
    e = nvs_get_u32(h, "next", &next);
    if (e == ESP_ERR_NVS_NOT_FOUND) e = ESP_OK;
    if (e == ESP_OK && next < minimum) next = minimum;

    if (e == ESP_OK && (next == 0 || next >= INT32_MAX))
        e = ESP_ERR_INVALID_STATE;

    if (e == ESP_OK) e = nvs_set_u32(h, "next", next + 1);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);

    if (e == ESP_OK) *id = next;
    return e;
}

static void le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

// HTTP writes may be partial.
static esp_err_t write_all(esp_http_client_handle_t c,
                           const void *data, size_t size)
{
    const char *p = data;

    while (size) {
        int n = esp_http_client_write(c, p, (int)size);
        if (n <= 0) return ESP_FAIL;
        p += n;
        size -= (size_t)n;
    }
    return ESP_OK;
}

static esp_err_t put_photo(const char *jwt, const char *obj,
                           const lv_draw_buf_t *image, uint32_t *bytes, bool *exists)
{
    *exists = false;
    uint32_t w = image->header.w;
    uint32_t h = image->header.h;

    if (!w || !h || w > 800 || h > 480 ||
        image->header.cf != LV_COLOR_FORMAT_RGB565)
        return ESP_ERR_INVALID_ARG;

    uint32_t row_size = (w * 3 + 3) & ~3u;
    uint32_t pixel_size = row_size * h;
    *bytes = 54 + pixel_size;

    uint8_t header[54] = {0};
    header[0] = 'B';
    header[1] = 'M';
    le32(header + 2, *bytes);
    le32(header + 10, 54);
    le32(header + 14, 40);
    le32(header + 18, w);

    // Negative height: rows are stored top to bottom.
    le32(header + 22, (uint32_t)(-(int32_t)h));
    le16(header + 26, 1);
    le16(header + 28, 24);
    le32(header + 34, pixel_size);

    uint8_t *row = calloc(1, row_size);
    char *auth = malloc(strlen(jwt) + 8);
    if (!row || !auth) {
        free(row);
        free(auth);
        return ESP_ERR_NO_MEM;
    }
    sprintf(auth, "Bearer %s", jwt);

    char url[256];
    snprintf(url, sizeof url,
             "%s/storage/v1/object/screenshots/%s",
             SUPABASE_URL, obj);

    const esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };

    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        free(row);
        free(auth);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t e = esp_http_client_set_header(
        c, "apikey", SUPABASE_ANON_KEY);
    if (e == ESP_OK)
        e = esp_http_client_set_header(c, "Authorization", auth);
    if (e == ESP_OK)
        e = esp_http_client_set_header(c, "Content-Type", "image/bmp");
    free(auth);

    if (e == ESP_OK) e = esp_http_client_open(c, (int)*bytes);
    if (e == ESP_OK) e = write_all(c, header, sizeof header);

    for (uint32_t y = 0; y < h && e == ESP_OK; y++) {
        const uint8_t *src =
            image->data + y * image->header.stride;

        for (uint32_t x = 0; x < w; x++) {
            uint16_t p;
            memcpy(&p, src + x * 2, sizeof p);

            unsigned r = (p >> 11) & 31;
            unsigned g = (p >> 5) & 63;
            unsigned b = p & 31;

            row[x * 3]     = (uint8_t)((b << 3) | (b >> 2));
            row[x * 3 + 1] = (uint8_t)((g << 2) | (g >> 4));
            row[x * 3 + 2] = (uint8_t)((r << 3) | (r >> 2));
        }

        e = write_all(c, row, row_size);
    }

    int status = -1;
    char response[384] = {0};
    if (e == ESP_OK) {
        if (esp_http_client_fetch_headers(c) < 0)
            e = ESP_FAIL;
        else
            status = esp_http_client_get_status_code(c);
    }

    if (e == ESP_OK && status != 200 && status != 201) {
        int got = 0;
        while (got < sizeof response - 1) {
            int n = esp_http_client_read(c, response + got, sizeof response - 1 - got);
            if (n <= 0) break;
            got += n;
        }
        response[got] = 0;
        cJSON *error = cJSON_Parse(response);
        const cJSON *code = cJSON_GetObjectItem(error, "error");
        const cJSON *message = cJSON_GetObjectItem(error, "message");
        *exists = (status == 400 || status == 409) &&
            ((cJSON_IsString(code) &&
              (!strcmp(code->valuestring, "Duplicate") ||
               !strcmp(code->valuestring, "ResourceAlreadyExists") ||
               !strcmp(code->valuestring, "already_exists"))) ||
             (cJSON_IsString(message) &&
              (!strcmp(message->valuestring, "The resource already exists") ||
               !strcmp(message->valuestring, "Asset Already Exists"))));
        cJSON_Delete(error);
    }

    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    free(row);

    if (e != ESP_OK) return e;
    if (status == 401) return ESP_ERR_INVALID_STATE;
    if (status != 200 && status != 201) {
        ESP_LOGW(TAG, "photo HTTP %d: %s", status, response);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t add_row(const char *jwt, const char *uid,
                         const char *obj, uint32_t id,
                         uint32_t bytes, time_t captured)
{
    char when[32] = "null";

    // Ignore an unset clock.
    if (captured >= 1704067200) {
        struct tm tm;
        gmtime_r(&captured, &tm);
        strftime(when, sizeof when,
                 "\"%Y-%m-%dT%H:%M:%SZ\"", &tm);
    }

    char body[320], resp[256];
    snprintf(body, sizeof body,
             "{\"owner\":\"%s\",\"shot_id\":%lu,"
             "\"link\":\"%s\",\"bytes\":%lu,\"captured_at\":%s}",
             uid, (unsigned long)id, obj,
             (unsigned long)bytes, when);

    int status;
    esp_err_t e = cloud_json(
        HTTP_METHOD_POST, "/rest/v1/screenshots", jwt,
        "return=minimal", body, &status, resp, sizeof resp);

    if (e != ESP_OK) return e;
    if (status == 401) return ESP_ERR_INVALID_STATE;
    if (status != 201) {
        ESP_LOGW(TAG, "photo row HTTP %d: %s", status, resp);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void save_task(void *arg)
{
    save_job_t job;

    for (;;) {
        xQueueReceive(s_queue, &job, portMAX_DELAY);

        lv_draw_buf_t *image = NULL;
        char *jwt = malloc(JWT_MAX);
        char uid[40], obj[96];
        uint32_t id = 0, bytes = 0;
        time_t captured = 0;
        const char *message = "Cloud save failed. Try again";
        esp_err_t e = ESP_FAIL;

        if (!account_signed_in() ||
            strcmp(job.user, account_username()) != 0) {
            message = "Account changed. Press SAVE again";
            goto done;
        }
        if (!net_online()) {
            message = "Wi-Fi offline. Nothing saved";
            goto done;
        }
        if (!jwt) {
            message = "Not enough memory to save";
            goto done;
        }

        e = account_token(jwt, JWT_MAX, uid, sizeof uid);
        if (e != ESP_OK) {
            message = e == ESP_ERR_INVALID_STATE
                ? "Session expired. Sign in again"
                : "Can't reach the cloud. Try again";
            goto done;
        }

        // Account may have changed while the token was refreshed.
        if (strcmp(job.user, account_username()) != 0) {
            message = "Account changed. Press SAVE again";
            goto done;
        }

        e = next_id(jwt, uid, &id);
        if (e != ESP_OK) goto done;

        lvgl_port_lock(0);
        image = lv_snapshot_take(job.screen, LV_COLOR_FORMAT_RGB565);
        captured = time(NULL);
        lvgl_port_unlock();

        if (!image) {
            message = "Not enough memory for a screenshot";
            goto done;
        }

        ui_toast("Uploading photo...");
        snprintf(obj, sizeof obj, "%s/%010lu.bmp",
                 uid, (unsigned long)id);

        for (unsigned attempt = 0; attempt < 3; attempt++) {
            bool exists;
            e = put_photo(jwt, obj, image, &bytes, &exists);
            if (!exists || attempt == 2) break;
            // An orphaned upload or another device can still occupy the path.
            ESP_LOGW(TAG, "photo %lu already exists; trying the next number", (unsigned long)id);
            e = next_id(jwt, uid, &id);
            if (e != ESP_OK) break;
            snprintf(obj, sizeof obj, "%s/%010lu.bmp", uid, (unsigned long)id);
        }
        if (e == ESP_OK) {
            e = add_row(jwt, uid, obj, id, bytes, captured);
            if (e != ESP_OK)
                message = "Photo uploaded, but listing failed";
        }

        if (e == ESP_OK)
            message = "Photo saved to your account";
        else if (e == ESP_ERR_INVALID_STATE)
            message = "Session expired. Sign in again";

    done:
        if (image) {
            lvgl_port_lock(0);
            lv_draw_buf_destroy(image);
            lvgl_port_unlock();
        }
        free(jwt);
        ESP_LOGI(TAG, "photo %lu: %s",
                 (unsigned long)id, esp_err_to_name(e));
        ui_toast(message);
        xSemaphoreGive(s_slot);
    }
}

esp_err_t upload_start(void)
{
    if (s_queue) return ESP_ERR_INVALID_STATE;

    s_queue = xQueueCreate(1, sizeof(save_job_t));
    s_slot = xSemaphoreCreateBinary();

    if (!s_queue || !s_slot) {
        if (s_queue) vQueueDelete(s_queue);
        if (s_slot) vSemaphoreDelete(s_slot);
        s_queue = NULL;
        s_slot = NULL;
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreatePinnedToCore(save_task, "photo", 8192,
                               NULL, 3, NULL, 0) != pdPASS) {
        vQueueDelete(s_queue);
        vSemaphoreDelete(s_slot);
        s_queue = NULL;
        s_slot = NULL;
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreGive(s_slot);
    return ESP_OK;
}

void upload_save(const ui_settings_t *s)
{
    if (!account_signed_in()) {
        ui_toast("Sign in to save photos");
        return;
    }
    if (!net_online()) {
        ui_toast("Wi-Fi offline. Nothing saved");
        return;
    }
    if (s->mode != MODE_SCOPE || lv_screen_active() != ui_scope_screen()) {
        ui_toast("Open the scope screen to save");
        return;
    }
    if (!s_queue || !s_slot) {
        ui_toast("Photo saving unavailable");
        return;
    }
    if (xSemaphoreTake(s_slot, 0) != pdTRUE) {
        ui_toast("Photo upload busy");
        return;
    }

    save_job_t job = { .screen = ui_scope_screen() };
    snprintf(job.user, sizeof job.user, "%s", account_username());

    if (!job.screen || xQueueSend(s_queue, &job, 0) != pdTRUE) {
        xSemaphoreGive(s_slot);
        ui_toast("Could not queue screenshot");
    }
}
