#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "account.h"
#include "net.h"
#include "ui_account.h"
#include "ui_photos.h"

enum { MODE_SIGN_IN, MODE_CREATE, MODE_FORGOT, JOB_SIGN_OUT };

typedef struct { int mode; char user[24], pass[72], phrase[96]; } job_t;

static lv_obj_t *s_scr, *s_prev, *s_status, *s_modes, *s_msg, *s_kb, *s_go, *s_go_lbl, *s_out;
static lv_obj_t *s_user, *s_pass, *s_phrase, *s_user_lbl, *s_pass_lbl, *s_phrase_lbl;
static lv_obj_t *s_photos, *s_photo_info;
static int s_mode;
static QueueHandle_t s_q;
static volatile bool s_working;
static const char *TAG = "ui_account";
#define ACCOUNT_STACK_BYTES 12288
#if !CONFIG_SPIRAM_XIP_FROM_PSRAM || !CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM
#error "Account worker requires PSRAM XIP and external task stack support"
#endif
static StaticTask_t s_worker_control;
static StackType_t *s_worker_stack;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[sizeof(job_t)];

static bool start_worker(void);

static void show(lv_obj_t *o, bool on)
{
    if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void show_mode(void)
{
    bool phrase = s_mode != MODE_SIGN_IN;
    show(s_phrase, phrase);
    show(s_phrase_lbl, phrase);
    lv_label_set_text(s_pass_lbl, s_mode == MODE_FORGOT ? "New password" : "Password");
    lv_label_set_text(s_go_lbl, s_mode == MODE_SIGN_IN ? "Sign in" :
                                s_mode == MODE_CREATE  ? "Create account" : "Reset password");
    lv_label_set_text(s_msg, s_mode == MODE_CREATE
        ? "Pick a secret phrase you'll remember. It's the only way to reset a forgotten password."
        : "");
}

static void refresh(void)                      // LVGL lock held
{
    bool in = account_signed_in();
    char buf[64];
    if (in) snprintf(buf, sizeof buf, "Signed in as %s", account_username());
    else    snprintf(buf, sizeof buf, "Not signed in");
    lv_label_set_text(s_status, buf);
    lv_obj_t *form[] = { s_modes, s_user, s_pass, s_phrase, s_user_lbl, s_pass_lbl, s_phrase_lbl, s_go, s_kb };
    for (size_t i = 0; i < sizeof form / sizeof form[0]; i++) show(form[i], !in);
    show(s_out, in);
    show(s_photos, in);
    show(s_photo_info, in);
    if (in) lv_label_set_text(s_msg, "");
    if (!in) show_mode();
}

// Network work happens here, never on the LVGL task
static void acct_task(void *arg)
{
    job_t j;
    char msg[128];
    for (;;) {
        xQueueReceive(s_q, &j, portMAX_DELAY);
        int mode = j.mode;
        esp_err_t e;
        ESP_LOGI(TAG, "account request started (mode %d)", mode);
        if (mode == JOB_SIGN_OUT)      { account_sign_out(); e = ESP_OK; snprintf(msg, sizeof msg, "Signed out"); }
        else if (!net_online())        { e = ESP_FAIL; snprintf(msg, sizeof msg, "Wi-Fi offline"); }
        else if (mode == MODE_SIGN_IN) e = account_sign_in(j.user, j.pass, msg, sizeof msg);
        else if (mode == MODE_CREATE)  e = account_sign_up(j.user, j.pass, j.phrase, msg, sizeof msg);
        else                           e = account_reset(j.user, j.phrase, j.pass, msg, sizeof msg);
        memset(&j, 0, sizeof j);                                  // wipe password and phrase
        ESP_LOGI(TAG, "account request finished: %s", esp_err_to_name(e));

        lvgl_port_lock(0);
        if (e == ESP_OK) {
            lv_textarea_set_text(s_pass, "");
            lv_textarea_set_text(s_phrase, "");
            if (mode == MODE_FORGOT) {                            // next: sign in with the new password
                s_mode = MODE_SIGN_IN;
                lv_buttonmatrix_set_button_ctrl(s_modes, MODE_SIGN_IN, LV_BUTTONMATRIX_CTRL_CHECKED);
            }
        }
        refresh();
        lv_label_set_text(s_msg, account_signed_in() ? "" : msg);
        lvgl_port_unlock();
        s_working = false;
    }
}

static void submit(void)
{
    if (s_working) return;
    if (!start_worker()) {
        lv_label_set_text(s_msg, "Account unavailable: memory. Try again");
        return;
    }
    job_t j = { .mode = s_mode };
    strlcpy(j.user, lv_textarea_get_text(s_user), sizeof j.user);
    strlcpy(j.pass, lv_textarea_get_text(s_pass), sizeof j.pass);
    strlcpy(j.phrase, lv_textarea_get_text(s_phrase), sizeof j.phrase);
    if (!j.user[0] || !j.pass[0] || (s_mode != MODE_SIGN_IN && !j.phrase[0])) {
        lv_label_set_text(s_msg, "Fill in every field");
        return;
    }
    s_working = true;
    lv_label_set_text(s_msg, "Working...");
    if (xQueueSend(s_q, &j, 0) != pdTRUE) {
        s_working = false;
        lv_label_set_text(s_msg, "Account busy. Try again");
    }
    memset(&j, 0, sizeof j);
}

static void on_photos(lv_event_t *e) { if (!s_working) ui_photos_open(s_scr); }

static void on_go(lv_event_t *e)      { submit(); }
static void on_kb_ready(lv_event_t *e) { submit(); }
static void on_back(lv_event_t *e)    { lv_screen_load(s_prev); }

static void on_out(lv_event_t *e)
{
    if (s_working) return;
    if (!start_worker()) {
        lv_label_set_text(s_msg, "Account unavailable: memory. Try again");
        return;
    }
    job_t j = { .mode = JOB_SIGN_OUT };
    s_working = true;
    if (xQueueSend(s_q, &j, 0) != pdTRUE) {
        s_working = false;
        lv_label_set_text(s_msg, "Account busy. Try again");
    }
}

static bool start_worker(void)
{
    if (s_q) return true;
    s_worker_stack = heap_caps_malloc(ACCOUNT_STACK_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_worker_stack) {
        ESP_LOGE(TAG, "cannot allocate account worker stack");
        return false;
    }
    QueueHandle_t queue = xQueueCreateStatic(1, sizeof(job_t), s_queue_storage, &s_queue_control);
    s_q = queue;
    TaskHandle_t task = queue ? xTaskCreateStaticPinnedToCore(acct_task, "acct", ACCOUNT_STACK_BYTES,
                                   NULL, 3, s_worker_stack, &s_worker_control, 0) : NULL;
    if (!task) {
        if (queue) vQueueDelete(queue);
        s_q = NULL;
        free(s_worker_stack);
        s_worker_stack = NULL;
        ESP_LOGE(TAG, "cannot create account worker");
        return false;
    }
    ESP_LOGI(TAG, "worker ready: %u-byte stack in PSRAM", ACCOUNT_STACK_BYTES);
    return true;
}

static void on_mode(lv_event_t *e)
{
    s_mode = (int)lv_buttonmatrix_get_selected_button(lv_event_get_target(e));
    show_mode();
}

static void on_focus(lv_event_t *e)
{
    lv_keyboard_set_textarea(s_kb, lv_event_get_target(e));
}

static lv_obj_t *field(const char *name, int y, int max, bool secret, lv_obj_t **lbl)
{
    *lbl = lv_label_create(s_scr);
    lv_label_set_text(*lbl, name);
    lv_obj_set_pos(*lbl, 20, y + 10);
    lv_obj_t *ta = lv_textarea_create(s_scr);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, max);
    lv_textarea_set_password_mode(ta, secret);
    lv_obj_set_size(ta, 360, 44);
    lv_obj_set_pos(ta, 170, y);
    lv_obj_add_event_cb(ta, on_focus, LV_EVENT_FOCUSED, NULL);
    return ta;
}

static lv_obj_t *button(const char *text, int x, int y, int w, lv_event_cb_t cb, lv_obj_t **lbl)
{
    lv_obj_t *b = lv_button_create(s_scr);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 44);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (lbl) *lbl = l;
    return b;
}

static void build(void)
{
    static const char *const MODES[] = { "Sign in", "Create account", "Forgot password", "" };
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(s_scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_28, 0);
    lv_label_set_text(t, "Account");
    lv_obj_set_pos(t, 20, 14);
    s_status = lv_label_create(s_scr);
    lv_obj_set_pos(s_status, 180, 24);
    button("Back", 680, 8, 100, on_back, NULL);

    s_modes = lv_buttonmatrix_create(s_scr);
    lv_buttonmatrix_set_map(s_modes, MODES);
    lv_buttonmatrix_set_button_ctrl_all(s_modes, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(s_modes, true);
    lv_buttonmatrix_set_button_ctrl(s_modes, MODE_SIGN_IN, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_set_size(s_modes, 520, 48);
    lv_obj_set_pos(s_modes, 10, 58);
    lv_obj_add_event_cb(s_modes, on_mode, LV_EVENT_VALUE_CHANGED, NULL);

    s_user   = field("Username",      114, 20, false, &s_user_lbl);
    s_pass   = field("Password",      164, 64, true,  &s_pass_lbl);
    s_phrase = field("Secret phrase", 214, 90, false, &s_phrase_lbl);

    s_go  = button("Sign in", 560, 114, 220, on_go, &s_go_lbl);
    s_out = button("Sign out", 560, 114, 220, on_out, NULL);
    s_photos = button("My photos", 20, 130, 300, on_photos, NULL);
    s_photo_info = lv_label_create(s_scr);
    lv_obj_set_pos(s_photo_info, 20, 192);
    lv_obj_set_width(s_photo_info, 480);
    lv_label_set_text(s_photo_info, "Your saved scope screenshots, kept in your account.\nOpen a photo, then close it to return to your list.");
    s_msg = lv_label_create(s_scr);
    lv_label_set_long_mode(s_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_msg, 220);
    lv_obj_set_pos(s_msg, 560, 168);

    s_kb = lv_keyboard_create(s_scr);                  // text keyboard; the check key submits
    lv_obj_set_size(s_kb, 800, 212);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(s_kb, s_user);
    lv_obj_add_event_cb(s_kb, on_kb_ready, LV_EVENT_READY, NULL);

    start_worker();
}

void ui_account_open(void)
{
    if (!s_scr) build();
    s_prev = lv_screen_active();
    refresh();
    lv_screen_load(s_scr);
}
