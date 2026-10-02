#include <stdio.h>
#include "esp_heap_caps.h"
#include "oscil_dds.h"
#include "ui.h"
#include "ui_gen.h"

#define PV_W 256                          // preview: one period, 256 phase steps
#define PV_H 128

static lv_obj_t *s_shapes, *s_freq, *s_amp, *s_off, *s_duty, *s_duty_lbl, *s_on, *s_pv;
static uint16_t *s_pv_buf;

// Same formulas as board 3: dds_next() from shared/oscil_dds.h, one sample per phase step
static void draw_preview(const proto_gen_set_t *g)
{
    dds_t d = { 0 };
    dds_set(&d, (dds_shape_t)g->shape, 1.0, PV_W, g->duty / 65535.0, g->lo, g->hi);
    for (int i = 0; i < PV_W * PV_H; i++) s_pv_buf[i] = 0x0000;
    for (int x = 0; x < PV_W; x++) {
        int y = PV_H - 1 - dds_next(&d) * (PV_H - 1) / 255;   // 0..255 codes = 0..3.3 V
        s_pv_buf[y * PV_W + x] = g->on ? 0xFFE0 : 0x7BEF;
    }
    lv_obj_invalidate(s_pv);
}

static void on_shape(lv_event_t *e)
{
    ui_apply(ACT_GEN_SHAPE, (int)lv_buttonmatrix_get_selected_button(lv_event_get_target(e)));
}

static void freq_done(float hz, void *ctx)
{
    ui_apply(ACT_GEN_FREQ, (int)(hz + 0.5f));
}

static void on_freq(lv_event_t *e)
{
    ui_keypad_open("Frequency, Hz (1 to 10000)", freq_done, NULL);
}

static void on_slider(lv_event_t *e)
{
    lv_obj_t *s = lv_event_get_target(e);
    ui_action_t a = (ui_action_t)(intptr_t)lv_event_get_user_data(e);
    ui_apply(a, (int)lv_slider_get_value(s));
}

static void on_switch(lv_event_t *e)
{
    ui_apply(ACT_GEN_ON, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}

static void on_back(lv_event_t *e)
{
    ui_apply(ACT_GEN, 0);
}

static lv_obj_t *slider(lv_obj_t *scr, const char *name, int y, int lo, int hi, ui_action_t a)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_label_set_text(l, name);
    lv_obj_set_pos(l, 20, y);
    lv_obj_t *s = lv_slider_create(scr);
    lv_slider_set_range(s, lo, hi);
    lv_obj_set_size(s, 400, 20);
    lv_obj_set_pos(s, 120, y + 2);
    lv_obj_add_event_cb(s, on_slider, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)a);
    return s;
}

lv_obj_t *ui_gen_create(void)
{
    static const char *const MAP[] = { "Sine", "Square", "Saw", "Triangle", "DC", "" };   // dds_shape_t order
    dds_init();                                           // the sine table dds_next reads
    s_pv_buf = heap_caps_malloc(PV_W * PV_H * 2, MALLOC_CAP_SPIRAM);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_button_create(scr);
    lv_obj_set_pos(back, 700, 10);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(back), "SCOPE");

    s_shapes = lv_buttonmatrix_create(scr);
    lv_buttonmatrix_set_map(s_shapes, MAP);
    lv_buttonmatrix_set_button_ctrl_all(s_shapes, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(s_shapes, true);
    lv_obj_set_size(s_shapes, 600, 60);
    lv_obj_set_pos(s_shapes, 20, 10);
    lv_obj_add_event_cb(s_shapes, on_shape, LV_EVENT_VALUE_CHANGED, NULL);

    s_freq = lv_label_create(scr);                        // tap to type a frequency
    lv_obj_set_style_text_font(s_freq, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(s_freq, 20, 90);
    lv_obj_add_flag(s_freq, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_freq, on_freq, LV_EVENT_CLICKED, NULL);

    s_amp  = slider(scr, "Amplitude", 160, 2, 255, ACT_GEN_AMP);     // hi - lo, codes
    s_off  = slider(scr, "Offset",    220, 1, 254, ACT_GEN_OFFSET);  // centre, codes
    s_duty = slider(scr, "Duty %",    280, 1, 99,  ACT_GEN_DUTY);
    s_duty_lbl = lv_obj_get_child(scr, lv_obj_get_index(s_duty) - 1);

    s_on = lv_switch_create(scr);
    lv_obj_set_size(s_on, 120, 60);
    lv_obj_set_pos(s_on, 20, 360);
    lv_obj_add_event_cb(s_on, on_switch, LV_EVENT_VALUE_CHANGED, NULL);

    s_pv = lv_canvas_create(scr);
    lv_canvas_set_buffer(s_pv, s_pv_buf, PV_W, PV_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_pv, 540, 160);
    return scr;
}

void ui_gen_sync(const proto_gen_set_t *g)
{
    lv_buttonmatrix_set_button_ctrl(s_shapes, g->shape, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_label_set_text_fmt(s_freq, "%lu.%03lu Hz", (unsigned long)(g->freq_mhz / 1000),
                          (unsigned long)(g->freq_mhz % 1000));
    lv_slider_set_value(s_amp, g->hi - g->lo, LV_ANIM_OFF);   // setters send no events
    lv_slider_set_value(s_off, (g->hi + g->lo) / 2, LV_ANIM_OFF);
    lv_slider_set_value(s_duty, g->duty * 100 / 65535, LV_ANIM_OFF);
    bool sq = (g->shape == DDS_SQUARE);                       // duty only means something for square
    if (sq) { lv_obj_remove_flag(s_duty, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(s_duty_lbl, LV_OBJ_FLAG_HIDDEN); }
    else    { lv_obj_add_flag(s_duty, LV_OBJ_FLAG_HIDDEN);    lv_obj_add_flag(s_duty_lbl, LV_OBJ_FLAG_HIDDEN); }
    if (g->on) lv_obj_add_state(s_on, LV_STATE_CHECKED);
    else       lv_obj_remove_state(s_on, LV_STATE_CHECKED);
    draw_preview(g);
}