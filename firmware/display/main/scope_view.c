#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "scope_view.h"
#include "ota.h"

static const char *TAG = "scope";

// RGB565 colours
#define COL_GRID    0x39E7                // dark grey
#define COL_AXIS    0x7BEF                // mid grey, centre lines
static const uint16_t COL_CH[2] = { 0xFFE0, 0x07FF };   // yellow, cyan

static uint16_t *s_bg;                    // graticule, drawn once (PSRAM)
static uint16_t *s_px;                    // canvas buffer LVGL renders from (PSRAM)
static uint8_t  *s_int[2];                // Phosphor intensity per channel (PSRAM)
static uint16_t  s_lut[2][256];           // intensity -> colour
static lv_obj_t *s_canvas, *s_meas[2];
static int s_selected, s_xshift[2];
static int16_t (*s_hit_lo)[SCOPE_W], (*s_hit_hi)[SCOPE_W];
static void (*s_drag)(int dx, int dy);
static bool s_pressed, s_dragging;
static int s_press_ch;
static uint32_t s_press_tick;
static lv_point_t s_press_point, s_drag_point;
static view_t    s_view;                  // written and read under the LVGL lock

static oscil_afe_cal_t s_cal[2] = { OSCIL_AFE_CAL_NOMINAL, OSCIL_AFE_CAL_NOMINAL };
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;   // guards s_cal and the slot indices

// Two frame slots: the link writes one while the renderer reads the other
typedef struct { uint16_t len; uint8_t buf[SCOPE_FRAME_MAX]; } slot_t;
static slot_t s_slot[2];                  // internal RAM, 2 x 6.4 KB
static int s_ready = -1, s_drawing = -1, s_copying = -1, s_last = -1;
static TaskHandle_t s_task;
static float s_fps;

static inline uint16_t dim(uint16_t c, int k)           // scale an RGB565 colour by k/255
{
    int r = (c >> 11) * k / 255, g = ((c >> 5) & 0x3F) * k / 255, b = (c & 0x1F) * k / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void draw_graticule(void)
{
    for (int i = 0; i < SCOPE_W * SCOPE_H; i++) s_bg[i] = 0x0000;
    for (int y = 0; y < SCOPE_H; y++)
        for (int x = 0; x < SCOPE_W; x += SCOPE_DIV_X) s_bg[y * SCOPE_W + x] = COL_GRID;
    for (int y = 0; y < SCOPE_H; y += SCOPE_DIV_Y)
        for (int x = 0; x < SCOPE_W; x++) s_bg[y * SCOPE_W + x] = COL_GRID;
    for (int x = 0; x < SCOPE_W; x++) s_bg[(SCOPE_H / 2) * SCOPE_W + x] = COL_AXIS;
    for (int y = 0; y < SCOPE_H; y++) s_bg[y * SCOPE_W + SCOPE_W / 2] = COL_AXIS;
    for (int x = 0; x < SCOPE_W; x++) s_bg[(SCOPE_H - 1) * SCOPE_W + x] = COL_GRID;
    for (int y = 0; y < SCOPE_H; y++) s_bg[y * SCOPE_W + SCOPE_W - 1] = COL_GRID;
        // Minor ticks on the centre axes: 5 per division, 7 px long, like a bench scope
    for (int x = 0; x < SCOPE_W; x += SCOPE_DIV_X / 5)          // every 16 px across
        for (int dy = -3; dy <= 3; dy++)
            s_bg[(SCOPE_H / 2 + dy) * SCOPE_W + x] = COL_AXIS;
    for (int y = 0; y < SCOPE_H; y += SCOPE_DIV_Y / 5)          // every 10 px down
        for (int dx = -3; dx <= 3; dx++)
            s_bg[y * SCOPE_W + SCOPE_W / 2 + dx] = COL_AXIS;
}

static inline int code_to_y(uint16_t code, const view_ch_t *v, const oscil_afe_cal_t *cal)
{
    float volts = oscil_afe_code_to_volts(code, cal);
    int y = SCOPE_H / 2 - (int)((volts - v->offset_v) / v->volts_per_div * SCOPE_DIV_Y);
    return y < 0 ? 0 : (y >= SCOPE_H ? SCOPE_H - 1 : y);    // never index outside the buffer
}

static void draw_normal(const proto_frame_hdr_t *h, const uint16_t *data, const oscil_afe_cal_t *cal)
{
    memset(s_hit_lo, 0xff, 2 * SCOPE_W * sizeof(int16_t));
    memcpy(s_px, s_bg, SCOPE_W * SCOPE_H * sizeof(uint16_t));    // background in one copy
    for (int ch = 0; ch < 2; ch++) {
        if (!(h->ch_mask & (1 << ch)) || !s_view.ch[ch].on) continue;
        const uint16_t *mn = data + ch * 2 * h->cols, *mx = mn + h->cols;
        for (int x = 0; x < h->cols; x++) {                       // one vertical run per column
            int dest_x = x + s_xshift[ch];
            if (dest_x < 0 || dest_x >= SCOPE_W) continue;
            int y0 = code_to_y(mx[x], &s_view.ch[ch], &cal[ch]);
            int y1 = code_to_y(mn[x], &s_view.ch[ch], &cal[ch]);
            s_hit_lo[ch][dest_x] = y0; s_hit_hi[ch][dest_x] = y1;
            for (int y = y0; y <= y1; y++) s_px[y * SCOPE_W + dest_x] = COL_CH[ch];
        }
    }
}

static void draw_phosphor(const proto_frame_hdr_t *h, const uint16_t *data, const oscil_afe_cal_t *cal)
{
    memset(s_hit_lo, 0xff, 2 * SCOPE_W * sizeof(int16_t));
    for (int ch = 0; ch < 2; ch++) {
        uint8_t *I = s_int[ch];
        for (int i = 0; i < SCOPE_W * SCOPE_H; i++) I[i] -= (I[i] + 7) >> 3;   // x 7/8, reaches 0
        if (!(h->ch_mask & (1 << ch)) || !s_view.ch[ch].on) continue;
        const uint16_t *mn = data + ch * 2 * h->cols, *mx = mn + h->cols;
        for (int x = 0; x < h->cols; x++) {
            int dest_x = x + s_xshift[ch];
            if (dest_x < 0 || dest_x >= SCOPE_W) continue;
            int y0 = code_to_y(mx[x], &s_view.ch[ch], &cal[ch]);
            int y1 = code_to_y(mn[x], &s_view.ch[ch], &cal[ch]);
            s_hit_lo[ch][dest_x] = y0; s_hit_hi[ch][dest_x] = y1;
            for (int y = y0; y <= y1; y++) {
                uint8_t *p = &I[y * SCOPE_W + dest_x];
                *p = (*p > 255 - 64) ? 255 : *p + 64;             // saturating add
            }
        }
    }
    const uint8_t *I1 = s_int[0], *I2 = s_int[1];
    for (int i = 0; i < SCOPE_W * SCOPE_H; i++) {                  // CH1 over CH2 over graticule
        uint16_t c = s_bg[i];
        if (I2[i]) c = s_lut[1][I2[i]];
        if (I1[i]) c = s_lut[0][I1[i]];
        s_px[i] = c;
    }
}

static void show_meas(const proto_frame_hdr_t *h)
{
    for (int ch = 0; ch < 2; ch++) {
        bool show = s_view.meas_panel && s_view.ch[ch].on;
        if (!show) { lv_obj_add_flag(s_meas[ch], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(s_meas[ch], LV_OBJ_FLAG_HIDDEN);
        const view_ch_t *v = &s_view.ch[ch];
        float k = (v->amps && v->shunt_ohm > 0) ? 1.0f / v->shunt_ohm : 1.0f;   // V -> A
        const char *u = (v->amps && v->shunt_ohm > 0) ? "A" : "V";
        proto_meas_t m = h->meas[ch];                               // copy out of the packed header
        char buf[160];                                            // libc snprintf: LVGL's built-in one has no %f
        snprintf(buf, sizeof buf,
            "CH%d  min %.3f %s  max %.3f %s  pp %.3f %s\navg %.3f %s  rms %.3f %s  f %.1f Hz  duty %.1f %%",
            ch + 1, m.vmin * k, u, m.vmax * k, u, (m.vmax - m.vmin) * k, u,
            m.vavg * k, u, m.vrms * k, u, m.freq_hz, m.duty * 100.0f);
        lv_label_set_text(s_meas[ch], buf);
    }
}

static void scope_task(void *arg)
{
    int64_t t_win = esp_timer_get_time(), t_draw = 0;
    int frames = 0;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (ota_is_running()) {
            vTaskDelay(pdMS_TO_TICKS(100));   // hold the last trace while flash is written
        }

        oscil_afe_cal_t cal[2];
        taskENTER_CRITICAL(&s_mux);
        int r = s_ready;
        s_ready = -1;
        s_drawing = r;
        cal[0] = s_cal[0]; cal[1] = s_cal[1];
        taskEXIT_CRITICAL(&s_mux);
        if (r < 0) continue;

        const proto_frame_hdr_t *h = (const proto_frame_hdr_t *)s_slot[r].buf;
        const uint16_t *data = (const uint16_t *)(s_slot[r].buf + sizeof *h);   // 2-byte aligned: the header is 64 bytes
        int64_t t0 = esp_timer_get_time();
        lvgl_port_lock(0);
        if (s_view.phosphor) draw_phosphor(h, data, cal);
        else                 draw_normal(h, data, cal);
        show_meas(h);
        lv_obj_invalidate(s_canvas);
        lvgl_port_unlock();
        t_draw += esp_timer_get_time() - t0;

        taskENTER_CRITICAL(&s_mux);
        s_drawing = -1;
        s_last = r;
        taskEXIT_CRITICAL(&s_mux);
        vTaskDelay(pdMS_TO_TICKS(10));   // block so lower-priority tasks can run

        int64_t now = esp_timer_get_time();
        if (++frames, now - t_win >= 5000000) {
            s_fps = frames * 1e6f / (float)(now - t_win);
            ESP_LOGI(TAG, "%.1f fps, draw %lld us/frame", s_fps, (long long)(t_draw / frames));
            frames = 0; t_draw = 0; t_win = now;
        }
    }
}

static void on_trace_touch(lv_event_t *e)
{
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_RELEASED || event == LV_EVENT_PRESS_LOST) {
        s_pressed = s_dragging = false;
        return;
    }
    if (event != LV_EVENT_PRESSED && event != LV_EVENT_PRESSING) return;
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;
    lv_point_t p; lv_indev_get_point(indev, &p);
    if (event == LV_EVENT_PRESSED) {
        lv_area_t area; lv_obj_get_coords(s_canvas, &area);
        int x = p.x - area.x1, y = p.y - area.y1;
        s_pressed = s_dragging = false;
        if (!s_drag || !s_view.ch[s_selected].on || x < 0 || x >= SCOPE_W || y < 0 || y >= SCOPE_H) return;
        // Hold near the selected trace. Other channels cannot arm dragging.
        for (int xx = x - 3; xx <= x + 3; xx++) {
            if (xx < 0 || xx >= SCOPE_W || s_hit_lo[s_selected][xx] < 0) continue;
            if (y >= s_hit_lo[s_selected][xx] - 12 && y <= s_hit_hi[s_selected][xx] + 12) {
                s_pressed = true; s_press_ch = s_selected;
                s_press_point = s_drag_point = p; s_press_tick = lv_tick_get(); break;
            }
        }
    } else if (s_pressed) {
        if (s_press_ch != s_selected || !s_view.ch[s_selected].on) { s_pressed = false; return; }
        if (!s_dragging) {
            if (lv_tick_elaps(s_press_tick) < 300) {
                if (abs(p.x - s_press_point.x) > 16 || abs(p.y - s_press_point.y) > 16) s_pressed = false;
                return;
            }
            s_dragging = true; s_drag_point = p; return;
        }
        int dx = p.x - s_drag_point.x, dy = p.y - s_drag_point.y;
        if (abs(dx) + abs(dy) >= 2) { s_drag_point = p; s_drag(dx, dy); }
    }
}

void scope_view_set_touch(int selected, void (*drag)(int dx, int dy))
{
    if (selected != s_selected) s_pressed = s_dragging = false;
    s_selected = selected & 1; s_drag = drag;
}

void scope_view_set_x_offset(int ch, int pixels)
{
    ch &= 1;
    s_xshift[ch] = pixels < -400 ? -400 : pixels > 400 ? 400 : pixels;
    if (s_view.phosphor) memset(s_int[ch], 0, SCOPE_W * SCOPE_H);
    scope_view_set_view(&s_view);
}

esp_err_t scope_view_init(lv_obj_t *parent, int y)
{
    const size_t px = SCOPE_W * SCOPE_H;
    s_bg = heap_caps_malloc(px * 2, MALLOC_CAP_SPIRAM);
    s_px = heap_caps_malloc(px * 2, MALLOC_CAP_SPIRAM);
    s_hit_lo = heap_caps_malloc(2 * SCOPE_W * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    s_hit_hi = heap_caps_malloc(2 * SCOPE_W * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    s_int[0] = heap_caps_calloc(px, 1, MALLOC_CAP_SPIRAM);
    s_int[1] = heap_caps_calloc(px, 1, MALLOC_CAP_SPIRAM);
    if (!s_bg || !s_px || !s_int[0] || !s_int[1] || !s_hit_lo || !s_hit_hi) return ESP_ERR_NO_MEM;
    for (int ch = 0; ch < 2; ch++)
        for (int i = 0; i < 256; i++) s_lut[ch][i] = dim(COL_CH[ch], 40 + i * 215 / 255);   // never fully dark
    draw_graticule();
    memcpy(s_px, s_bg, px * 2);

    s_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_canvas, s_px, SCOPE_W, SCOPE_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_canvas, 0, y);
    memset(s_hit_lo, 0xff, 2 * SCOPE_W * sizeof(int16_t));
    lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_canvas, on_trace_touch, LV_EVENT_ALL, NULL);
    for (int ch = 0; ch < 2; ch++) {
        s_meas[ch] = lv_label_create(parent);
        lv_obj_set_style_text_color(s_meas[ch], lv_color_hex(ch ? 0x00FFFF : 0xFFFF00), 0);
        lv_obj_set_style_bg_color(s_meas[ch], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_meas[ch], LV_OPA_70, 0);
        lv_obj_set_pos(s_meas[ch], 4, y + SCOPE_H - 84 + ch * 42);
        lv_obj_add_flag(s_meas[ch], LV_OBJ_FLAG_HIDDEN);
    }
    if (xTaskCreatePinnedToCore(scope_task, "scope", 16384,
                                NULL, 5, &s_task, 1) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}


    void scope_view_set_view(const view_t *v)
{
    s_view = *v;
    // Redraw the last frame with the new view, so V/div, channel on/off and MEAS
    // change the picture even in STOP, when no new frames arrive.
    taskENTER_CRITICAL(&s_mux);
    if (s_ready < 0 && s_last >= 0) s_ready = s_last;
    taskEXIT_CRITICAL(&s_mux);
    if (s_task) xTaskNotifyGive(s_task);
}


void scope_view_set_cal(const oscil_afe_cal_t cal[2])
{
    taskENTER_CRITICAL(&s_mux);
    s_cal[0] = cal[0]; s_cal[1] = cal[1];
    taskEXIT_CRITICAL(&s_mux);
}

void scope_view_get_cal(int ch, oscil_afe_cal_t *out)
{
    taskENTER_CRITICAL(&s_mux);
    *out = s_cal[ch & 1];
    taskEXIT_CRITICAL(&s_mux);
}

void scope_view_submit(const uint8_t *payload, uint16_t len)
{
    if (len < sizeof(proto_frame_hdr_t) || len > SCOPE_FRAME_MAX) return;
    const proto_frame_hdr_t *h = (const proto_frame_hdr_t *)payload;
    if (h->cols == 0 || h->cols > PROTO_COLS ||
        len != sizeof *h + 4u * h->cols * sizeof(uint16_t)) return;   // both channels, min and max

    taskENTER_CRITICAL(&s_mux);
    int w = -1;
    for (int i = 0; i < 2 && w < 0; i++)
        if (i != s_drawing && i != s_copying) w = i;   // never a slot someone is reading
    if (w >= 0) {
        if (s_ready == w) s_ready = -1;              // drop an older undrawn frame in it
        if (s_last == w) s_last = -1;                // SAVE must not copy a half-written slot
    }
    taskEXIT_CRITICAL(&s_mux);
    if (w < 0) return;                               // both slots busy: drop this frame

    memcpy(s_slot[w].buf, payload, len);
    s_slot[w].len = len;

    taskENTER_CRITICAL(&s_mux);
    s_ready = w;
    taskEXIT_CRITICAL(&s_mux);
    if (s_task) xTaskNotifyGive(s_task);
}

size_t scope_view_copy_last(uint8_t *dst, size_t cap)
{
    taskENTER_CRITICAL(&s_mux);
    int r = s_last;
    if (r >= 0) s_copying = r;            // the link won't write this slot while we copy
    taskEXIT_CRITICAL(&s_mux);
    if (r < 0) return 0;
    size_t n = s_slot[r].len <= cap ? s_slot[r].len : 0;
    if (n) memcpy(dst, s_slot[r].buf, n);
    taskENTER_CRITICAL(&s_mux);
    s_copying = -1;
    taskEXIT_CRITICAL(&s_mux);
    return n;
}

float scope_view_fps(void)
{
    return s_fps;
}