#include "esp_lvgl_port.h"
#include "gui_test.h"

void test_hello(void)
{
    lvgl_port_lock(0);
    lv_obj_t *l = lv_label_create(lv_screen_active());
    lv_label_set_text(l, "Oscil");
    lv_obj_center(l);
    lvgl_port_unlock();
}