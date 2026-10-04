#include <stdio.h>
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "account.h"
#include "net.h"
#include "ota.h"
#include "scope_view.h"
#include "ui.h"
#include "ui_account.h"
#include "ui_sys.h"

static lv_obj_t *s_panel, *s_info;

static void refresh(void)
{
    char buf[200];                        // libc snprintf: LVGL's built-in printf has no %f
    snprintf(buf, sizeof buf, "Oscil display %s\nWi-Fi %s, %d dBm\n%s%s\nfree heap %u KB (min %u KB)\n%.1f fps",
        ota_version(), net_online() ? "online" : "offline", net_rssi(),
        account_signed_in() ? "Signed in as " : "Not signed in", account_username(),
        (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024),
        (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT) / 1024), scope_view_fps());
    lv_label_set_text(s_info, buf);
}

static void on_update(lv_event_t *e)
{
    if (!net_online()) { ui_toast("Wi-Fi offline"); return; }
    ui_toast("Restarting for update...");
    esp_err_t err = ota_request_update();
    char msg[80];
    snprintf(msg, sizeof msg, "Update failed: %s", esp_err_to_name(err));
    ui_toast(msg);
}

static void on_account(lv_event_t *e)
{
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);     // the panel lives on the top layer
    ui_account_open();
}

static lv_obj_t *panel_button(const char *text, lv_align_t align, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(s_panel);
    lv_obj_align(b, align, 0, 0);
    lv_label_set_text(lv_label_create(b), text);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

void ui_sys_toggle(void)
{
    if (!s_panel) {
        s_panel = lv_obj_create(lv_layer_top());
        lv_obj_set_size(s_panel, 360, 250);
        lv_obj_align(s_panel, LV_ALIGN_TOP_RIGHT, -4, 44);
        s_info = lv_label_create(s_panel);
        panel_button("Update", LV_ALIGN_BOTTOM_LEFT, on_update);
        panel_button("Account", LV_ALIGN_BOTTOM_RIGHT, on_account);
        refresh();
        return;
    }
    if (lv_obj_has_flag(s_panel, LV_OBJ_FLAG_HIDDEN)) { refresh(); lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_HIDDEN); }
    else lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
}