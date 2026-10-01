#include <stdio.h>
#include "esp_lvgl_port.h"
#include "touch_test.h"

static lv_obj_t *s_dot;

static void on_press(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_obj_set_pos(s_dot, p.x - 5, p.y - 5);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) printf("touch %d,%d\n", (int)p.x, (int)p.y);
}

void test_touch(void)
{
    lvgl_port_lock(0);
    lv_obj_t *scr = lv_screen_active();
    s_dot = lv_obj_create(scr);
    lv_obj_set_size(s_dot, 10, 10);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot, lv_color_hex(0xFF0000), 0);
    lv_obj_remove_flag(s_dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, on_press, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, on_press, LV_EVENT_PRESSING, NULL);
    lvgl_port_unlock();
}