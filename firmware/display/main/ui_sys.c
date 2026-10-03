#include <stdio.h>
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "net.h"
#include "ota.h"
#include "scope_view.h"
#include "ui.h"
#include "ui_sys.h"

static lv_obj_t *s_panel, *s_info;

static void refresh(void)
{
    char buf[160];                        // libc snprintf: LVGL's built-in printf has no %f
    snprintf(buf, sizeof buf, "Oscil display %s\nWi-Fi %s, %d dBm\nfree heap %u KB (min %u KB)\n%.1f fps",
        ota_version(), net_online() ? "online" : "offline", net_rssi(),
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

void ui_sys_toggle(void)
{
    if (!s_panel) {
        s_panel = lv_obj_create(lv_layer_top());
        lv_obj_set_size(s_panel, 360, 220);
        lv_obj_align(s_panel, LV_ALIGN_TOP_RIGHT, -4, 44);
        s_info = lv_label_create(s_panel);
        lv_obj_t *b = lv_button_create(s_panel);
        lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_label_set_text(lv_label_create(b), "Update");
        lv_obj_add_event_cb(b, on_update, LV_EVENT_CLICKED, NULL);
        refresh();
        return;
    }
    if (lv_obj_has_flag(s_panel, LV_OBJ_FLAG_HIDDEN)) { refresh(); lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_HIDDEN); }
    else lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
}