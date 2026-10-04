#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_lvgl_port.h"
#include "cJSON.h"
#include "account.h"
#include "cloud_http.h"
#include "net.h"
#include "secrets.h"
#include "photo_bmp.h"
#include "ui_photos.h"
#include "ui.h"

#define PAGE_SIZE 20
#define PHOTO_STACK_BYTES 12288

// Token renewal can write NVS. XIP keeps PSRAM accessible during those flash writes.
#if !CONFIG_SPIRAM_XIP_FROM_PSRAM || !CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM
#error "Photo worker requires PSRAM XIP and external task stack support"
#endif
typedef struct { char link[160], date[32]; int shot; } entry_t;
typedef struct { unsigned epoch; int page; bool photo; char user[24]; entry_t entry; } job_t;
static lv_obj_t *s_scr, *s_prev, *s_list, *s_msg, *s_title, *s_image, *s_next, *s_before;
static QueueHandle_t s_queue;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[sizeof(job_t)];
static StaticTask_t s_worker_control;
static StackType_t *s_worker_stack;
static const char *TAG = "photos";
static unsigned s_epoch;
static int s_page, s_count;
static bool s_busy, s_has_next;
static entry_t s_entries[PAGE_SIZE];
static lv_image_dsc_t s_descriptor;
static uint16_t *s_pixels;

static void discard_image(void)
{
    if (s_image) { lv_obj_delete(s_image); s_image = NULL; }
    lv_image_cache_drop(&s_descriptor);
    free(s_pixels); s_pixels = NULL;
    memset(&s_descriptor, 0, sizeof s_descriptor);
}

static bool current(const job_t *j) // worker: never retain UI pointers outside the lock
{
    lvgl_port_lock(0);
    bool ok = j->epoch == s_epoch && lv_screen_active() == s_scr &&
              account_signed_in() && strcmp(j->user, account_username()) == 0;
    lvgl_port_unlock();
    return ok;
}

static void request(bool photo, int index);
static void on_item(lv_event_t *e) { request(true, (int)(intptr_t)lv_event_get_user_data(e)); }
static void render_list(void)
{
    lv_obj_clean(s_list);
    lv_obj_remove_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < s_count; i++) {
        lv_obj_t *b = lv_button_create(s_list);
        lv_obj_set_pos(b, 0, i * 62); lv_obj_set_size(b, 744, 54);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x202c40), 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_add_event_cb(b, on_item, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(b);
        char text[100];
        snprintf(text, sizeof text, "Photo %d    %.10s  %.8s UTC", s_entries[i].shot,
                 s_entries[i].date, strlen(s_entries[i].date) > 11 ? s_entries[i].date + 11 : "");
        lv_label_set_text(l, text); lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
        l = lv_label_create(b); lv_label_set_text(l, LV_SYMBOL_RIGHT);
        lv_obj_align(l, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_title, "My photos  /  Page %d", s_page + 1);
    lv_obj_remove_flag(s_before, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_next, LV_OBJ_FLAG_HIDDEN);
    if (s_page == 0) lv_obj_add_state(s_before, LV_STATE_DISABLED);
    else lv_obj_remove_state(s_before, LV_STATE_DISABLED);
    if (!s_has_next) lv_obj_add_state(s_next, LV_STATE_DISABLED);
    else lv_obj_remove_state(s_next, LV_STATE_DISABLED);
}

static void request(bool photo, int index)
{
    if (s_busy || !account_signed_in()) return;
    if (!net_online()) { lv_label_set_text(s_msg, "Wi-Fi offline. Connect and refresh."); return; }
    if (photo && (index < 0 || index >= s_count)) return;
    discard_image();
    job_t j = { .epoch = ++s_epoch, .page = s_page, .photo = photo };
    strlcpy(j.user, account_username(), sizeof j.user);
    if (photo) j.entry = s_entries[index];
    s_busy = true;
    lv_label_set_text(s_msg, photo ? "Loading photo..." : "Loading your photos...");
    if (!photo) {
        s_count = 0; s_has_next = false;
        memset(s_entries, 0, sizeof s_entries);
        render_list();
    }
    if (photo) {
        lv_obj_add_flag(s_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_before, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_next, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(s_title, "Photo %d", j.entry.shot);
    }
    xQueueReset(s_queue); // at most one queued request; the older worker notices the epoch
    if (xQueueSend(s_queue, &j, 0) != pdTRUE) {
        s_busy = false; lv_label_set_text(s_msg, "Could not start. Refresh to retry.");
    }
}

static bool read_exact(esp_http_client_handle_t c, uint8_t *p, int bytes)
{
    while (bytes > 0) {
        int n = esp_http_client_read(c, (char *)p, bytes);
        if (n <= 0) return false;
        p += n; bytes -= n;
    }
    return true;
}

static uint16_t *download(const job_t *j, const char *jwt, const char *uid, photo_bmp_t *bmp)
{
    // A row must refer to a file inside this user's folder. Never follow arbitrary URLs.
    size_t un = strlen(uid);
    if (strncmp(j->entry.link, uid, un) || j->entry.link[un] != '/') return NULL;
    const char *name = j->entry.link + un + 1;
    if (!*name || strstr(name, "..")) return NULL;
    for (const char *p = name; *p; p++) if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.') return NULL;
    char url[384];
    if (snprintf(url, sizeof url, "%s/storage/v1/object/authenticated/screenshots/%s", SUPABASE_URL, j->entry.link) >= sizeof url) return NULL;
    esp_http_client_config_t cfg = { .url = url, .timeout_ms = 15000, .crt_bundle_attach = esp_crt_bundle_attach,
                                     .buffer_size = 2048, .buffer_size_tx = 2048 };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return NULL;
    char bearer[1560]; snprintf(bearer, sizeof bearer, "Bearer %s", jwt);
    esp_http_client_set_header(c, "apikey", SUPABASE_ANON_KEY);
    esp_http_client_set_header(c, "Authorization", bearer);
    uint16_t *pixels = NULL;
    const char *stage = "connect";
    uint8_t *row = NULL, header[54];
    if (esp_http_client_open(c, 0) != ESP_OK) goto done;
    stage = "headers/BMP format";
    int64_t length = esp_http_client_fetch_headers(c);
    if (length < 0 || esp_http_client_get_status_code(c) != 200 ||
        !current(j) || !read_exact(c, header, sizeof header) || !photo_bmp_header(header, sizeof header, bmp)) goto done;
    uint32_t expected = bmp->offset + bmp->row_bytes * bmp->height;
    if (length > 0 && length != expected) goto done;
    stage = "image allocation";
    row = malloc(bmp->row_bytes);
    pixels = heap_caps_malloc(bmp->width * bmp->height * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!row || !pixels) goto failed;
    stage = "image body";
    for (uint32_t skip = 54; skip < bmp->offset;) {
        uint32_t n = bmp->offset - skip;
        if (n > bmp->row_bytes) n = bmp->row_bytes;
        if (!read_exact(c, row, n)) goto failed;
        skip += n;
    }
    for (uint32_t y = 0; y < bmp->height; y++) {
        if (!(y % 16) && !current(j)) goto failed;
        if (!read_exact(c, row, bmp->row_bytes)) goto failed;
        uint32_t dest = bmp->top_down ? y : bmp->height - 1 - y;
        uint16_t *out = pixels + dest * bmp->width;
        for (uint32_t x = 0; x < bmp->width; x++) out[x] = photo_rgb565(row + x * 3);
    }
    // Also consume chunked trailers and reject extra or incomplete data.
    char extra;
    if (esp_http_client_read(c, &extra, 1) != 0 || !esp_http_client_is_complete_data_received(c)) goto failed;
    goto done;
failed:
    free(pixels); pixels = NULL;
done:
    if (!pixels && current(j)) ESP_LOGW(TAG, "photo %d failed at %s (HTTP %d)", j->entry.shot, stage, esp_http_client_get_status_code(c));
    free(row); esp_http_client_close(c); esp_http_client_cleanup(c);
    return pixels;
}

static void worker(void *arg)
{
    job_t j;
    for (;;) {
        if (xQueueReceive(s_queue, &j, portMAX_DELAY) != pdTRUE || !current(&j)) continue;
        char jwt[1536], uid[40], message[120] = "Could not load. Check Wi-Fi, then refresh.";
        esp_err_t err = account_token(jwt, sizeof jwt, uid, sizeof uid);
        uint16_t *pixels = NULL;
        photo_bmp_t bmp = {0};
        entry_t *entries = NULL;
        int count = 0; bool has_next = false;
        if (err == ESP_ERR_INVALID_STATE) strlcpy(message, "Session expired. Back to Account and sign in again.", sizeof message);
        if (err == ESP_OK && current(&j)) {
            if (j.photo) pixels = download(&j, jwt, uid, &bmp);
            else {
                char path[320];
                snprintf(path, sizeof path, "/rest/v1/screenshots?select=shot_id,link,created_at&owner=eq.%s&order=created_at.desc,id.desc&limit=21&offset=%d", uid, j.page * PAGE_SIZE);
                char *reply = heap_caps_malloc(16384, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                int status = -1;
                if (reply && cloud_json(HTTP_METHOD_GET, path, jwt, NULL, NULL, &status, reply, 16384) == ESP_OK && status == 200) {
                    cJSON *a = cJSON_ParseWithOpts(reply, NULL, true);
                    if (cJSON_IsArray(a)) {
                        entries = calloc(PAGE_SIZE, sizeof *entries);
                        int total = cJSON_GetArraySize(a);
                        has_next = total > PAGE_SIZE;
                        bool valid = entries != NULL;
                        for (int i = 0; valid && i < total && i < PAGE_SIZE; i++) {
                            cJSON *o = cJSON_GetArrayItem(a, i);
                            cJSON *link = cJSON_GetObjectItemCaseSensitive(o, "link");
                            cJSON *date = cJSON_GetObjectItemCaseSensitive(o, "created_at");
                            cJSON *shot = cJSON_GetObjectItemCaseSensitive(o, "shot_id");
                            valid = cJSON_IsString(link) && strlen(link->valuestring) < sizeof entries[i].link && cJSON_IsString(date) && cJSON_IsNumber(shot);
                            if (valid) { strlcpy(entries[i].link, link->valuestring, sizeof entries[i].link); strlcpy(entries[i].date, date->valuestring, sizeof entries[i].date); entries[i].shot = shot->valueint; count++; }
                        }
                        if (!valid) { free(entries); entries = NULL; }
                    }
                    cJSON_Delete(a);
                }
                if (!entries && current(&j)) ESP_LOGW(TAG, "list failed (HTTP %d)", status);
                free(reply);
            }
        }
        memset(jwt, 0, sizeof jwt);
        lvgl_port_lock(0);
        if (j.epoch == s_epoch && lv_screen_active() == s_scr) {
            s_busy = false;
            if (!account_signed_in() || strcmp(j.user, account_username()) != 0) {
                discard_image(); memset(s_entries, 0, sizeof s_entries); s_count = 0; s_has_next = false;
                render_list(); lv_label_set_text(s_msg, "Session expired. Back to Account and sign in again.");
            } else if (pixels) {
                discard_image(); s_pixels = pixels; pixels = NULL;
                s_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
                s_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
                s_descriptor.header.w = bmp.width; s_descriptor.header.h = bmp.height;
                s_descriptor.header.stride = bmp.width * 2;
                s_descriptor.data_size = bmp.width * bmp.height * 2;
                s_descriptor.data = (const uint8_t *)s_pixels;
                s_image = lv_image_create(s_scr); lv_image_set_src(s_image, &s_descriptor);
                uint32_t scale = 760 * 256 / bmp.width;
                if (scale > 340 * 256 / bmp.height) scale = 340 * 256 / bmp.height;
                if (scale > 256) scale = 256;
                lv_image_set_scale(s_image, scale);
                lv_obj_align(s_image, LV_ALIGN_CENTER, 0, 8);
                lv_obj_remove_flag(s_image, LV_OBJ_FLAG_CLICKABLE);
                lv_label_set_text(s_msg, "Close returns to your photo list.");
            } else if (entries) {
                memcpy(s_entries, entries, sizeof s_entries); s_count = count; s_has_next = has_next;
                render_list(); lv_label_set_text(s_msg, count ? "Tap a photo to open it." : "No photos yet. Back to the scope and press SAVE.");
            } else {
                render_list(); lv_label_set_text(s_msg, message);
            }
        }
        lvgl_port_unlock(); free(pixels); free(entries);
    }
}

static void on_close(lv_event_t *e)
{
    if (s_image || lv_obj_has_flag(s_list, LV_OBJ_FLAG_HIDDEN)) {
        ++s_epoch; xQueueReset(s_queue); s_busy = false; discard_image(); render_list();
        lv_label_set_text(s_msg, "Tap a photo to open it.");
    } else lv_screen_load(s_prev);
}
static void on_refresh(lv_event_t *e)
{
    if (s_busy) return;
    if (!net_online() || !account_signed_in()) { request(false, 0); return; }
    s_page = 0; request(false, 0);
}
static void on_page(lv_event_t *e)
{
    if (s_busy) return;
    if (!net_online() || !account_signed_in()) { request(false, 0); return; }
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    if ((dir < 0 && s_page == 0) || (dir > 0 && !s_has_next)) return;
    s_page += dir; request(false, 0);
}
static void on_leave(lv_event_t *e)
{
    ++s_epoch; xQueueReset(s_queue); s_busy = false; discard_image();
    memset(s_entries, 0, sizeof s_entries); s_count = 0;
    lv_obj_clean(s_list);
}
static lv_obj_t *button(const char *name, int x, int y, int width, lv_event_cb_t cb, intptr_t arg)
{
    lv_obj_t *b = lv_button_create(s_scr); lv_obj_set_pos(b, x, y); lv_obj_set_size(b, width, 42);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)arg);
    lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, name); lv_obj_center(l); return b;
}
void ui_photos_open(lv_obj_t *previous)
{
    if (!account_signed_in()) return;
    if (!s_scr) {
        ESP_LOGI(TAG, "startup: internal free %u, largest %u; PSRAM largest %u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        s_worker_stack = heap_caps_malloc(PHOTO_STACK_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_worker_stack) {
            ESP_LOGE(TAG, "cannot allocate %u-byte PSRAM worker stack", PHOTO_STACK_BYTES);
            ui_toast("Photo viewer unavailable: memory"); return;
        }
        s_queue = xQueueCreateStatic(1, sizeof(job_t), s_queue_storage, &s_queue_control);
        TaskHandle_t task = s_queue ? xTaskCreateStaticPinnedToCore(worker, "photos", PHOTO_STACK_BYTES,
                                       NULL, 3, s_worker_stack, &s_worker_control, 0) : NULL;
        if (!task) {
            if (s_queue) vQueueDelete(s_queue);
            s_queue = NULL; free(s_worker_stack); s_worker_stack = NULL;
            ESP_LOGE(TAG, "cannot create gallery queue/task");
            ui_toast("Photo viewer unavailable: worker"); return;
        }
        ESP_LOGI(TAG, "worker ready: %u-byte stack in PSRAM", PHOTO_STACK_BYTES);
        s_scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x101827), 0);
        lv_obj_set_style_text_color(s_scr, lv_color_hex(0xe6edf7), 0);
        lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(s_scr, on_leave, LV_EVENT_SCREEN_UNLOADED, NULL);
        s_title = lv_label_create(s_scr); lv_obj_set_style_text_font(s_title, &lv_font_montserrat_28, 0); lv_obj_set_pos(s_title, 20, 18);
        button("Refresh", 552, 10, 108, on_refresh, 0);
        button("Close", 680, 10, 100, on_close, 0);
        s_list = lv_obj_create(s_scr); lv_obj_set_pos(s_list, 12, 68); lv_obj_set_size(s_list, 776, 326);
        lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(s_list, 0, 0); lv_obj_set_style_pad_all(s_list, 8, 0);
        lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
        s_msg = lv_label_create(s_scr); lv_obj_set_pos(s_msg, 20, 444); lv_obj_set_width(s_msg, 760);
        s_before = button("Previous", 20, 402, 126, on_page, -1);
        s_next = button("Next", 654, 402, 126, on_page, 1);
    }
    s_prev = previous; s_page = 0; s_count = 0; s_has_next = false; s_busy = false;
    lv_screen_load(s_scr); render_list(); request(false, 0);
}
