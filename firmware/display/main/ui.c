#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "links.h"
#include "ui.h"
#include "ui_gen.h"

// Panel numbering from acq/main/panel.h (step 09): PANEL_EV_*, PANEL_BTN_*
enum { EV_PRESS = 0, EV_RELEASE = 1, EV_TURN = 2 };
enum { BTN_RUN = 0, BTN_SINGLE = 1, BTN_GEN = 2, BTN_ENC1 = 3, BTN_ENC2 = 4, BTN_ENC3 = 5 };

// rate = R / (10 x s/div) with R = 1600 (step 18)
static const struct { const char *name; uint32_t rate_hz; } TB[] = {
    { "200 us", 800000 }, { "500 us", 320000 }, { "1 ms", 160000 }, { "2 ms", 80000 },
    { "5 ms", 32000 },    { "10 ms", 16000 },   { "20 ms", 8000 },  { "50 ms", 3200 },
    { "100 ms", 1600 },   { "200 ms", 800 },    { "500 ms", 320 },
};
#define TB_COUNT   (int)(sizeof TB / sizeof TB[0])
static const float VDIV[] = { 0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f };
#define VDIV_COUNT (int)(sizeof VDIV / sizeof VDIV[0])
static const uint32_t FSTEP_HZ[] = { 1, 10, 100, 1000 };
#define FREQ_MIN_MHZ 1000u                 // 1 Hz, in millihertz
#define FREQ_MAX_MHZ 10000000u             // 10 kHz

static ui_settings_t g_ui;                 // the one copy; touched only under the LVGL lock
static ui_hooks_t    s_hooks;
static QueueHandle_t s_q;
static lv_obj_t *s_scr_scope, *s_scr_gen, *s_status, *s_run_lbl, *s_toast;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void ui_defaults(ui_settings_t *s)
{
    memset(s, 0, sizeof *s);
    s->mode = MODE_SCOPE;
    s->tb_idx = 0;                                              // 200 us/div
    s->vdiv_idx[0] = s->vdiv_idx[1] = 4;                        // 1 V/div
    for (int ch = 0; ch < 2; ch++) {
        s->view.ch[ch] = (view_ch_t){ .on = true, .volts_per_div = 1.0f, .offset_v = 0, .amps = false, .shunt_ohm = 0 };
    }
    s->trig = (trig_cfg_t){ .level = 2560, .hyst = 50, .edge = TRIG_RISING };
    s->trig_src = 0; s->trig_mode = UI_TRIG_AUTO; s->run = UI_RUN;
    s->fstep_idx = 2;                                           // 100 Hz
    s->gen = (proto_gen_set_t){ .shape = 0, .on = 0, .lo = 0, .hi = 255,
                                .freq_mhz = 1000u * 1000u, .duty = 32768 };   // sine, 1 kHz, full range, 50 %
}

static void send_acq_set(const ui_settings_t *s)
{
    const proto_acq_set_t a = {
        .rate_hz = TB[s->tb_idx].rate_hz,
        .trig_level = s->trig.level, .trig_hyst = s->trig.hyst,
        .trig_edge = (uint8_t)s->trig.edge, .trig_src = s->trig_src,
        .trig_mode = s->trig_mode, .run = s->run,
    };
    links_send_acq(&a);
}

static void refresh_status(const ui_settings_t *s)
{
    oscil_afe_cal_t cal;
    scope_view_get_cal(s->trig_src, &cal);
    char ch[2][32];
    for (int c = 0; c < 2; c++) {
        const view_ch_t *v = &s->view.ch[c];
        if (!v->on) snprintf(ch[c], sizeof ch[c], "CH%d off", c + 1);
        else if (v->amps && v->shunt_ohm > 0)
            snprintf(ch[c], sizeof ch[c], "CH%d %g A/div", c + 1, v->volts_per_div / v->shunt_ohm);
        else snprintf(ch[c], sizeof ch[c], "CH%d %g V/div", c + 1, v->volts_per_div);
    }
    char buf[128];                          // libc snprintf: LVGL's built-in printf has no %f
    snprintf(buf, sizeof buf, "%s/div   %s%s   %s%s   Trig CH%d %s %.2f V %s",
        TB[s->tb_idx].name, s->sel_ch == 0 ? ">" : "", ch[0], s->sel_ch == 1 ? ">" : "", ch[1],
        s->trig_src + 1, s->trig.edge == TRIG_RISING ? "rise" : "fall",
    oscil_afe_code_to_volts(s->trig.level, &cal), s->trig_mode == UI_TRIG_AUTO ? "AUTO" : "NORM");
    lv_label_set_text(s_status, buf);
    lv_label_set_text(s_run_lbl, s->run == UI_STOP ? "STOP" : (s->run == UI_SINGLE ? "SINGLE" : "RUN"));
}

static void gen_set_span(proto_gen_set_t *g, int center, int span)
{
    span = clampi(span, 2, 255);
    center = clampi(center, span / 2, 255 - (span - span / 2));      // keep lo >= 0 and hi <= 255
    g->lo = (uint8_t)(center - span / 2);
    g->hi = (uint8_t)(g->lo + span);
}

static void shunt_done(float ohm, void *ctx)
{
    int ch = (int)(intptr_t)ctx;
    g_ui.view.ch[ch].shunt_ohm = ohm > 0 ? ohm : 0;
    g_ui.view.ch[ch].amps = ohm > 0;
    scope_view_set_view(&g_ui.view);
    if (s_hooks.changed) s_hooks.changed(&g_ui);
    refresh_status(&g_ui);
}

void ui_apply(ui_action_t a, int arg)
{
    ui_settings_t *s = &g_ui;
    proto_gen_set_t *g = &s->gen;
    bool acq = false, gen = false, view = false;
    int c = s->sel_ch;
    bool scope = (s->mode == MODE_SCOPE);

    switch (a) {
    case ACT_ENC1:
        if (scope) { s->tb_idx = (uint8_t)clampi(s->tb_idx + arg, 0, TB_COUNT - 1); acq = true; }
        else {
            int64_t f = (int64_t)g->freq_mhz + (int64_t)arg * FSTEP_HZ[s->fstep_idx] * 1000;
            g->freq_mhz = (uint32_t)(f < FREQ_MIN_MHZ ? FREQ_MIN_MHZ : (f > FREQ_MAX_MHZ ? FREQ_MAX_MHZ : f));
            gen = true;
        }
        break;
    case ACT_ENC2:
        if (scope) {
            s->vdiv_idx[c] = (uint8_t)clampi(s->vdiv_idx[c] - arg, 0, VDIV_COUNT - 1);  // clockwise = zoom in
            s->view.ch[c].volts_per_div = VDIV[s->vdiv_idx[c]];
            view = true;                                        // display zoom only: no message
        } else { gen_set_span(g, (g->hi + g->lo) / 2, g->hi - g->lo + 2 * arg); gen = true; }
        break;
    case ACT_ENC3:
        if (scope) { s->trig.level = (uint16_t)clampi(s->trig.level + arg * 8, 0, 4095); acq = true; }
        else { gen_set_span(g, (g->hi + g->lo) / 2 + 2 * arg, g->hi - g->lo); gen = true; }
        break;
    case ACT_PUSH1:
        if (scope) { s->view.ch[c].offset_v = 0; view = true; }
        else s->fstep_idx = (uint8_t)((s->fstep_idx + 1) % 4);
        break;
    case ACT_PUSH2:
        if (scope) s->sel_ch ^= 1;
        else { g->on = !g->on; gen = true; }
        break;
    case ACT_PUSH3:
        if (scope) { s->trig.edge = s->trig.edge == TRIG_RISING ? TRIG_FALLING : TRIG_RISING; acq = true; }
        else { g->shape = (uint8_t)((g->shape + 1) % 5); gen = true; }
        break;
    case ACT_RUN:    s->run = (s->run == UI_RUN) ? UI_STOP : UI_RUN; acq = true; break;
    case ACT_SINGLE: s->run = UI_SINGLE; acq = true; break;
    case ACT_GEN:
        s->mode = scope ? MODE_GEN : MODE_SCOPE;
        lv_screen_load(s->mode == MODE_GEN ? s_scr_gen : s_scr_scope);
        if (s->mode == MODE_GEN) ui_gen_sync(g);
        break;
    case ACT_CH:
        c = arg & 1;
        if (s->sel_ch == c || !s->view.ch[c].on) s->view.ch[c].on = !s->view.ch[c].on;
        s->sel_ch = (uint8_t)c;
        view = true;
        break;
    case ACT_TRIG:                          // rise -> fall -> other source -> AUTO/NORMAL
        if (s->trig.edge == TRIG_RISING) s->trig.edge = TRIG_FALLING;
        else { s->trig.edge = TRIG_RISING; s->trig_src ^= 1; if (s->trig_src == 0) s->trig_mode ^= 1; }
        acq = true;
        break;
    case ACT_MEAS:     s->view.meas_panel = !s->view.meas_panel; view = true; break;
    case ACT_PHOSPHOR: s->view.phosphor = !s->view.phosphor; view = true; break;
    case ACT_SAVE:     if (s_hooks.save) s_hooks.save(s); return;
    case ACT_SYS:      if (s_hooks.sys) s_hooks.sys(); return;
    case ACT_SHUNT:    ui_keypad_open(arg ? "CH2 shunt, ohm (0 = volts)" : "CH1 shunt, ohm (0 = volts)",
                                      shunt_done, (void *)(intptr_t)(arg & 1));
                       return;
    case ACT_GEN_SHAPE:  g->shape = (uint8_t)clampi(arg, 0, 4); gen = true; break;
    case ACT_GEN_FREQ:   g->freq_mhz = (uint32_t)clampi(arg, 1, 10000) * 1000u; gen = true; break;
    case ACT_GEN_AMP:    gen_set_span(g, (g->hi + g->lo) / 2, arg); gen = true; break;
    case ACT_GEN_OFFSET: gen_set_span(g, arg, g->hi - g->lo); gen = true; break;
    case ACT_GEN_DUTY:   g->duty = (uint16_t)(clampi(arg, 1, 99) * 65535 / 100); gen = true; break;
    case ACT_GEN_ON:     g->on = arg ? 1 : 0; gen = true; break;
    case ACT_ACQ_STATE:  if (s->run == (uint8_t)arg) return; s->run = (uint8_t)arg; break;   // follow, don't echo
    }
    if (acq)  send_acq_set(s);                               // MSG_ACQ_SET to board 1
    if (gen) { links_gen_set(g); if (s->mode == MODE_GEN) ui_gen_sync(g); }   // MSG_GEN_SET to board 3
    if (view) scope_view_set_view(&s->view);
    if (s_hooks.changed) s_hooks.changed(s);
    refresh_status(s);
}

// ---- input from board 1 ----

typedef struct { ui_action_t a; int arg; } ui_msg_t;

void ui_post(ui_action_t a, int arg)
{
    ui_msg_t m = { a, arg };
    if (s_q) xQueueSend(s_q, &m, 0);          // never block the link RX task
}

void ui_on_key(const proto_key_t *k)
{
    if (k->type == EV_TURN && k->id < 3) ui_post((ui_action_t)(ACT_ENC1 + k->id), k->delta);
    else if (k->type == EV_PRESS) {
        switch (k->id) {
        case BTN_RUN:    ui_post(ACT_RUN, 0); break;
        case BTN_SINGLE: ui_post(ACT_SINGLE, 0); break;
        case BTN_GEN:    ui_post(ACT_GEN, 0); break;
        case BTN_ENC1:   ui_post(ACT_PUSH1, 0); break;
        case BTN_ENC2:   ui_post(ACT_PUSH2, 0); break;
        case BTN_ENC3:   ui_post(ACT_PUSH3, 0); break;
        default: break;
        }
    }
}

void ui_on_acq_state(const uint8_t *p, uint16_t len)
{
    if (len >= 1) ui_post(ACT_ACQ_STATE, p[0]);        // first payload byte: run state
}

static void ui_task(void *arg)
{
    ui_msg_t m;
    for (;;) {
        if (xQueueReceive(s_q, &m, portMAX_DELAY) != pdTRUE) continue;
        lvgl_port_lock(0);
        ui_apply(m.a, m.arg);
        lvgl_port_unlock();
    }
}

// ---- widgets ----

static void on_btn(lv_event_t *e)
{
    intptr_t v = (intptr_t)lv_event_get_user_data(e);
    ui_apply((ui_action_t)(v & 0xFF), (int)(v >> 8));
}

static void on_ch_long(lv_event_t *e)
{
    ui_apply(ACT_SHUNT, (int)(intptr_t)lv_event_get_user_data(e));
}

static lv_obj_t *top_button(lv_obj_t *parent, int i, const char *text, ui_action_t a, int arg)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, 76, 34);
    lv_obj_set_pos(b, 2 + i * 80, 3);
    lv_obj_add_event_cb(b, on_btn, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)(a | (arg << 8)));
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (a == ACT_RUN) s_run_lbl = l;
    return b;
}

static void toast_hide(lv_timer_t *t)
{
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
}

void ui_toast(const char *msg)
{
    lvgl_port_lock(0);
    lv_label_set_text(s_toast, msg);
    lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_toast);
    lv_timer_t *t = lv_timer_create(toast_hide, 2000, NULL);
    lv_timer_set_repeat_count(t, 1);                  // deletes itself after one run
    lvgl_port_unlock();
}

// Numeric keypad, shared with the generator screen
static lv_obj_t *s_kp;
static void (*s_kp_done)(float, void *);
static void *s_kp_ctx;

static void on_kp(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        float v = 0;
        sscanf(lv_textarea_get_text(ta), "%f", &v);
        if (s_kp_done) s_kp_done(v, s_kp_ctx);
    }
    lv_obj_delete_async(s_kp);                        // READY or CANCEL both close it
    s_kp = NULL;
}

void ui_keypad_open(const char *title, void (*done)(float value, void *ctx), void *ctx)
{
    if (s_kp) return;
    s_kp_done = done; s_kp_ctx = ctx;
    s_kp = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_kp, 480, 360);
    lv_obj_center(s_kp);
    lv_obj_t *t = lv_label_create(s_kp);
    lv_label_set_text(t, title);
    lv_obj_t *ta = lv_textarea_create(s_kp);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_width(ta, 440);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_t *kb = lv_keyboard_create(s_kp);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, 440, 240);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(kb, on_kp, LV_EVENT_READY, ta);
    lv_obj_add_event_cb(kb, on_kp, LV_EVENT_CANCEL, ta);
}

void ui_start(const ui_settings_t *initial, const ui_hooks_t *hooks)
{
    g_ui = *initial;
    if (hooks) s_hooks = *hooks;
    s_q = xQueueCreate(16, sizeof(ui_msg_t));

    lvgl_port_lock(0);
    s_scr_scope = lv_screen_active();
    lv_obj_set_style_bg_color(s_scr_scope, lv_color_hex(0x000000), 0);
    lv_obj_remove_flag(s_scr_scope, LV_OBJ_FLAG_SCROLLABLE);

    // top bar, 40 px: 10 buttons of 80 px
    top_button(s_scr_scope, 0, "RUN", ACT_RUN, 0);
    top_button(s_scr_scope, 1, "SINGLE", ACT_SINGLE, 0);
    lv_obj_t *c1 = top_button(s_scr_scope, 2, "CH1", ACT_CH, 0);
    lv_obj_t *c2 = top_button(s_scr_scope, 3, "CH2", ACT_CH, 1);
    lv_obj_add_event_cb(c1, on_ch_long, LV_EVENT_LONG_PRESSED, (void *)0);
    lv_obj_add_event_cb(c2, on_ch_long, LV_EVENT_LONG_PRESSED, (void *)1);
    top_button(s_scr_scope, 4, "TRIG", ACT_TRIG, 0);
    top_button(s_scr_scope, 5, "MEAS", ACT_MEAS, 0);
    top_button(s_scr_scope, 6, "PHOS", ACT_PHOSPHOR, 0);
    top_button(s_scr_scope, 7, "GEN", ACT_GEN, 0);
    top_button(s_scr_scope, 8, "SAVE", ACT_SAVE, 0);
    lv_obj_t *sys = top_button(s_scr_scope, 9, "SYS", ACT_SYS, 0);
    if (!s_hooks.sys) lv_obj_add_flag(sys, LV_OBJ_FLAG_HIDDEN);

    scope_view_init(s_scr_scope, 40);                 // plot, 800 x 400
    s_status = lv_label_create(s_scr_scope);          // status bar, 40 px
    lv_obj_set_style_text_color(s_status, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(s_status, 6, 452);

    s_toast = lv_label_create(lv_layer_top());
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(0x303030), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_toast, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(s_toast, 10, 0);
    lv_obj_align(s_toast, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);

    s_scr_gen = ui_gen_create();

    // Board 1 and board 3 start from our state, not theirs
    scope_view_set_view(&g_ui.view);
    send_acq_set(&g_ui);
    links_gen_set(&g_ui.gen);
    refresh_status(&g_ui);
    if (g_ui.mode == MODE_GEN) { lv_screen_load(s_scr_gen); ui_gen_sync(&g_ui.gen); }
    lvgl_port_unlock();

    xTaskCreatePinnedToCore(ui_task, "ui", 4096, NULL, 5, NULL, 1);
}

lv_obj_t *ui_scope_screen(void)
{
    return s_scr_scope;
}