#include <stdio.h>
#include "esp_heap_caps.h"
#include "oscil_dds.h"
#include "ui.h"
#include "ui_gen.h"

#define PV_W 256                          // preview: one period, 256 phase steps
#define PV_H 128
#define CODE_TO_V(c) ((c) * 3.3f / 255.0f)   // DAC code -> volts at the ladder (0..255 = 0..3.3 V)

static lv_obj_t *s_shapes, *s_freq_btn, *s_freq, *s_amp, *s_off, *s_duty, *s_duty_lbl, *s_on, *s_pv;
static lv_obj_t *s_amp_val, *s_off_val, *s_duty_val, *s_on_lbl;
static lv_obj_t *s_dot, *s_status, *s_detail;     // output summary card
static uint16_t *s_pv_buf;

// Same formulas as board 3: dds_next() from shared/oscil_dds.h, one sample per phase step
static void draw_preview(const proto_gen_set_t *g)
{
    dds_t d = { 0 };
    dds_set(&d, (dds_shape_t)g->shape, 1.0, PV_W, g->duty / 65535.0, g->lo, g->hi);
        for (int i = 0; i < PV_W * PV_H; i++) s_pv_buf[i] = 0x0000;
    for (int k = 0; k <= 4; k++) {                              // faint grid: 0, 0.825, 1.65, 2.475, 3.3 V
        int y = k * (PV_H - 1) / 4, x = k * (PV_W - 1) / 4;
        uint16_t c = (k == 2) ? 0x4208 : 0x2104;                // centre line a little brighter
        for (int i = 0; i < PV_W; i++) s_pv_buf[y * PV_W + i] = c;
        for (int j = 0; j < PV_H; j++) s_pv_buf[j * PV_W + x] = c;
    }
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

// Name on the left, slider in the middle, live value on the right
static lv_obj_t *slider(lv_obj_t *scr, const char *name, int y, int lo, int hi, ui_action_t a, lv_obj_t **val)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_label_set_text(l, name);
    lv_obj_set_pos(l, 20, y);
    lv_obj_t *s = lv_slider_create(scr);
    lv_slider_set_range(s, lo, hi);
    lv_obj_set_size(s, 340, 20);
    lv_obj_set_pos(s, 130, y + 2);
    lv_obj_add_event_cb(s, on_slider, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)a);
    *val = lv_label_create(scr);
    lv_obj_set_pos(*val, 490, y);
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
    lv_obj_set_pos(back, 680, 10);
    lv_obj_set_size(back, 100, 60);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(back);
    lv_label_set_text(bl, "SCOPE");
    lv_obj_center(bl);

    s_shapes = lv_buttonmatrix_create(scr);
    lv_buttonmatrix_set_map(s_shapes, MAP);
    lv_buttonmatrix_set_button_ctrl_all(s_shapes, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(s_shapes, true);
    lv_obj_set_size(s_shapes, 640, 60);
    lv_obj_set_pos(s_shapes, 20, 10);
    lv_obj_add_event_cb(s_shapes, on_shape, LV_EVENT_VALUE_CHANGED, NULL);

    // Frequency: a real button, so it looks tappable
    s_freq_btn = lv_button_create(scr);
    lv_obj_set_pos(s_freq_btn, 20, 84);
    lv_obj_set_size(s_freq_btn, 360, 60);
    lv_obj_add_event_cb(s_freq_btn, on_freq, LV_EVENT_CLICKED, NULL);
    s_freq = lv_label_create(s_freq_btn);
    lv_obj_set_style_text_font(s_freq, &lv_font_montserrat_28, 0);
    lv_obj_center(s_freq);
    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, LV_SYMBOL_EDIT "  tap to type a frequency");
    lv_obj_set_pos(hint, 395, 104);

    s_amp  = slider(scr, "Amplitude", 170, 2, 255, ACT_GEN_AMP,    &s_amp_val);   // hi - lo, codes
    s_off  = slider(scr, "Offset",    230, 1, 254, ACT_GEN_OFFSET, &s_off_val);   // centre, codes
    s_duty = slider(scr, "Duty",      290, 1, 99,  ACT_GEN_DUTY,   &s_duty_val);
    s_duty_lbl = lv_obj_get_child(scr, lv_obj_get_index(s_duty) - 1);

    s_on = lv_switch_create(scr);
    lv_obj_set_size(s_on, 100, 50);
    lv_obj_set_pos(s_on, 20, 340);
    lv_obj_add_event_cb(s_on, on_switch, LV_EVENT_VALUE_CHANGED, NULL);
    s_on_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(s_on_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(s_on_lbl, 135, 348);

    s_pv = lv_canvas_create(scr);
    lv_canvas_set_buffer(s_pv, s_pv_buf, PV_W, PV_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_pv, 530, 160);

    // Output summary card: what the generator is set to, readable at a glance
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_size(card, 480, 70);
    lv_obj_set_pos(card, 20, 405);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 8, 0);

    s_dot = lv_obj_create(card);                          // status dot: green on, grey off
    lv_obj_set_size(s_dot, 18, 18);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_dot, 0, 0);
    lv_obj_align(s_dot, LV_ALIGN_LEFT_MID, 4, 0);

    s_status = lv_label_create(card);                     // headline: "2.5 kHz Sine"
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_28, 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, 36, -4);

    s_detail = lv_label_create(card);                     // detail: range and on/off
    lv_obj_set_style_text_color(s_detail, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(s_detail, LV_ALIGN_BOTTOM_LEFT, 36, 2);
    return scr;
}

void ui_gen_sync(const proto_gen_set_t *g)
{
    static const char *const SHAPE_TITLE[] = { "Sine", "Square", "Saw", "Triangle", "DC" };
    char buf[96];                                         // libc snprintf: LVGL's built-in one has no %f
    float lo_v = CODE_TO_V(g->lo), hi_v = CODE_TO_V(g->hi);
    unsigned duty = (unsigned)((g->duty * 100u + 32767u) / 65535u);

    lv_buttonmatrix_set_button_ctrl(s_shapes, g->shape, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_label_set_text_fmt(s_freq, "%lu.%03lu Hz", (unsigned long)(g->freq_mhz / 1000),
                          (unsigned long)(g->freq_mhz % 1000));

    // Sliders and their live values
    lv_slider_set_value(s_amp, g->hi - g->lo, LV_ANIM_OFF);   // setters send no events
    lv_slider_set_value(s_off, (g->hi + g->lo) / 2, LV_ANIM_OFF);
    lv_slider_set_value(s_duty, duty, LV_ANIM_OFF);
    snprintf(buf, sizeof buf, "%.2f Vpp", hi_v - lo_v);
    lv_label_set_text(s_amp_val, buf);
    snprintf(buf, sizeof buf, "%.2f V", (lo_v + hi_v) / 2);
    lv_label_set_text(s_off_val, buf);
    snprintf(buf, sizeof buf, "%u %%", duty);
    lv_label_set_text(s_duty_val, buf);

    bool sq = (g->shape == DDS_SQUARE);                       // duty only means something for square
    lv_obj_t *duty_objs[] = { s_duty, s_duty_lbl, s_duty_val };
    for (int i = 0; i < 3; i++) {
        if (sq) lv_obj_remove_flag(duty_objs[i], LV_OBJ_FLAG_HIDDEN);
        else    lv_obj_add_flag(duty_objs[i], LV_OBJ_FLAG_HIDDEN);
    }

    // On/off switch and its label
    if (g->on) lv_obj_add_state(s_on, LV_STATE_CHECKED);
    else       lv_obj_remove_state(s_on, LV_STATE_CHECKED);
    lv_label_set_text(s_on_lbl, g->on ? "Output ON" : "Output OFF");

    // Output summary card
    const char *shape = SHAPE_TITLE[g->shape < 5 ? g->shape : 0];
    uint32_t mhz = g->freq_mhz;
    if (g->shape == DDS_DC)    snprintf(buf, sizeof buf, "DC");
    else if (mhz >= 1000000u)  snprintf(buf, sizeof buf, "%g kHz %s", mhz / 1e6, shape);   // 2.5 kHz
    else                       snprintf(buf, sizeof buf, "%g Hz %s",  mhz / 1e3, shape);   // 250 Hz
    lv_label_set_text(s_status, buf);

    if (g->shape == DDS_DC) snprintf(buf, sizeof buf, "%.2f V   |   output %s", hi_v, g->on ? "on" : "off");
    else snprintf(buf, sizeof buf, "%.2f to %.2f V   |   output %s", lo_v, hi_v, g->on ? "on" : "off");
    lv_label_set_text(s_detail, buf);

    lv_obj_set_style_bg_color(s_dot, g->on ? lv_palette_main(LV_PALETTE_GREEN)
                                           : lv_palette_main(LV_PALETTE_GREY), 0);
    draw_preview(g);
}