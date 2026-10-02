#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "oscil_gpio.h"
#include "oscil_trigger.h"
#include "oscil_decimate.h"
#include "oscil_proto.h"
#include "oscil_pins_acq.h"
#include "panel.h"
#include "spi2_adc.h"
#include "sampler.h"
#include "acq_task.h"
#include "oscil_measure.h"


#define R2            (2 * ACQ_R)      // burst length: the trigger is searched in the middle half
#define TIMING_PIN    14               // spare GPIO14, header 20
#define TRIG_LED_MS   30               // TRIG flash length
#define AUTO_BURSTS   3                // AUTO: show an untriggered frame after this many misses
#define LINK_BAUD     2000000          // same on all three boards
#define ACQ_STANDALONE 0

#define TIMING(v) do { if (v) oscil_gpio_set(TIMING_PIN); else oscil_gpio_clr(TIMING_PIN); } while (0)

typedef enum { TRIG_AUTO, TRIG_NORMAL } trig_mode_t;
#define ACQ_MODE_DEFAULT TRIG_AUTO     // until board 2 can set it

typedef struct {                       // written by the link and panel tasks, read by acq_task
    uint32_t    rate_hz;
    trig_cfg_t  trig;
    uint8_t     trig_src;              // 0 = CH1, 1 = CH2
    trig_mode_t mode;
    acq_run_t   run;
} acq_settings_t;

static acq_settings_t s_set = {        // defaults until board 2 sends settings
        .rate_hz = 320000,                 // 500 us/div at R = 1600 (800k is above the measured 620k ceiling)
    .trig = { .level = 2560, .hyst = 50, .edge = TRIG_RISING },
    .trig_src = 0,
    .mode = ACQ_MODE_DEFAULT,
    .run = ACQ_RUN,
};
static SemaphoreHandle_t s_set_lock;
static SemaphoreHandle_t s_wake;        // given to leave STOP (the task notification is the sampler's)
static acq_frame_t s_frame;            // the link to board 2 sends this; until then it is just built

link_t s_link1 = { .uart = { .port = 1, .tx_pin = ACQ_LINK1_TX, .rx_pin = ACQ_LINK1_RX, .baud = LINK_BAUD } };
volatile uint32_t s_cap_errors;

// MSG_FRAME payload: header, then CH1 min, CH1 max, CH2 min, CH2 max (800 x u16 each).
#define FRAME_BYTES (sizeof(proto_frame_hdr_t) + 4u * ACQ_COLS * sizeof(uint16_t))   // 6464
static uint8_t s_tx[FRAME_BYTES] __attribute__((aligned(4)));
static_assert(ACQ_COLS == PROTO_COLS, "board 2 draws PROTO_COLS columns");
static_assert(FRAME_BYTES <= LINK_MAX_PAYLOAD, "frame too big for one link frame");


typedef struct { uint32_t frames, trigs, autos, errors, busy; } acq_stats_t;
static acq_stats_t s_stats;

void acq_set_run(acq_run_t r)
{
    xSemaphoreTake(s_set_lock, portMAX_DELAY);
    s_set.run = r;
    xSemaphoreGive(s_set_lock);
    if (r != ACQ_STOP) xSemaphoreGive(s_wake);             // wake it if it is parked in STOP
}

// Decimate R samples of both channels into the frame (min/max per column).
static void frame_build(const uint16_t *c1, const uint16_t *c2, const sampler_info_t *info,
                        float frac, bool triggered)
{
    static uint32_t frame_no;
    proto_frame_hdr_t *hdr = (proto_frame_hdr_t *)s_tx;
    uint16_t *d = (uint16_t *)(s_tx + sizeof *hdr);       // 64-byte header: d stays 4-aligned
    decimate_minmax(c1, ACQ_R, &d[0 * ACQ_COLS], &d[1 * ACQ_COLS], ACQ_COLS);
    decimate_minmax(c2, ACQ_R, &d[2 * ACQ_COLS], &d[3 * ACQ_COLS], ACQ_COLS);
    hdr->frame_no  = ++frame_no;
    hdr->rate_hz   = info->rate_hz;
    hdr->cols      = ACQ_COLS;
    hdr->triggered = triggered;
    hdr->ch_mask   = 0x3;
    hdr->trig_frac = frac;
    static const oscil_afe_cal_t CAL_NOM = OSCIL_AFE_CAL_NOMINAL;   // calibration later swaps in cal_get(ch)
    oscil_measure(c1, ACQ_R, info->rate_hz, &CAL_NOM, &hdr->meas[0]);   // full-resolution record, achieved rate
    oscil_measure(c2, ACQ_R, info->rate_hz, &CAL_NOM, &hdr->meas[1]);
}

// MSG_ACQ_STATE whenever the run state or the requested rate changes: first byte = run state.
static void state_send(const acq_settings_t *st, uint32_t achieved_hz)
{
    static acq_run_t sent_run = (acq_run_t)-1;
    static uint32_t sent_rate;
    if (st->run == sent_run && st->rate_hz == sent_rate) return;
    const proto_acq_state_t m = { .run = (uint8_t)st->run, .rate_hz = achieved_hz };
    if (link_send(&s_link1, MSG_ACQ_STATE, &m, sizeof m) == ESP_OK) {
        sent_run = st->run;
        sent_rate = st->rate_hz;
    }
}

// Once a second: throughput and the worst-case stack use of this task.
static void stats_line(TickType_t *last)
{
    TickType_t now = xTaskGetTickCount();
    if (now - *last < pdMS_TO_TICKS(1000)) return;
    printf("acq: %lu frames/s  trig %lu  auto %lu  errors %lu  link busy %lu  stack free %u bytes\n",
           (unsigned long)s_stats.frames, (unsigned long)s_stats.trigs,
           (unsigned long)s_stats.autos, (unsigned long)s_stats.errors,
           (unsigned long)s_stats.busy, (unsigned)uxTaskGetStackHighWaterMark(NULL));
    s_stats = (acq_stats_t){ 0 };
    *last = now;
}

static void acq_task(void *arg)
{
    static uint16_t ch1[R2];
    static uint16_t ch2[R2];
    int misses = 0;
    uint32_t achieved_hz = s_set.rate_hz;
    TickType_t trig_led_off = 0;
    TickType_t last_stats = xTaskGetTickCount();
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));           // watch this task, not core 1's idle task

    for (;;) {
        acq_settings_t st;
        xSemaphoreTake(s_set_lock, portMAX_DELAY);
        st = s_set;
        xSemaphoreGive(s_set_lock);
        state_send(&st, achieved_hz);

        if (st.run == ACQ_STOP) {
            panel_led(PANEL_LED_ARM, false);
            panel_led(PANEL_LED_TRIG, false);
            esp_task_wdt_delete(NULL);                  // blocking forever is correct here
            xSemaphoreTake(s_wake, portMAX_DELAY);
            ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
            continue;
        }
        esp_task_wdt_reset();
        if (trig_led_off && xTaskGetTickCount() >= trig_led_off) {
            panel_led(PANEL_LED_TRIG, false);
            trig_led_off = 0;
        }
        stats_line(&last_stats);

        panel_led(PANEL_LED_ARM, true);
        TIMING(1);
        sampler_info_t info;
        esp_err_t e = sampler_capture(ch1, ch2, R2, st.rate_hz, &info);
        TIMING(0);
        if (e != ESP_OK) {                              // never draw a failed burst
            s_stats.errors++;
            s_cap_errors++;
            continue;
        }
        achieved_hz = info.rate_hz;

        const uint16_t *src = st.trig_src ? ch2 : ch1;
        float frac = 0.0f;
        long t = trig_find(src, ACQ_R / 2, ACQ_R / 2 + ACQ_R, &st.trig, &frac);
        bool triggered = (t >= 0);
        if (triggered) {
            misses = 0;
            s_stats.trigs++;
            panel_led(PANEL_LED_TRIG, true);
            trig_led_off = xTaskGetTickCount() + pdMS_TO_TICKS(TRIG_LED_MS);
        } else {
            if (st.mode == TRIG_NORMAL || ++misses < AUTO_BURSTS) continue;   // wait for a trigger
            misses = 0;
            s_stats.autos++;
            t = ACQ_R;                                   // AUTO: show the middle, untriggered
            frac = 0.0f;
        }
        panel_led(PANEL_LED_ARM, false);

        size_t start = (size_t)t - ACQ_R / 2;            // trigger at screen centre, start in [0, R]
        TIMING(1);
        frame_build(&ch1[start], &ch2[start], &info, frac, triggered);
        TIMING(0);
        if (link_send(&s_link1, MSG_FRAME, s_tx, FRAME_BYTES) == ESP_OK) s_stats.frames++;
        else s_stats.busy++;                             // link still sending the last one: skip

        if (st.run == ACQ_SINGLE) {                      // one frame done; stop unless RUN was pressed meanwhile
            xSemaphoreTake(s_set_lock, portMAX_DELAY);
            if (s_set.run == ACQ_SINGLE) s_set.run = ACQ_STOP;
            xSemaphoreGive(s_set_lock);
        }
    }
}

// RUN toggles run/stop, SINGLE arms one frame. Board 1 is usable before the display exists.
static void acq_panel_task(void *arg)
{
    QueueHandle_t q = (QueueHandle_t)arg;
    panel_event_t ev;
    while (1) {
        if (!xQueueReceive(q, &ev, portMAX_DELAY)) continue;
        const proto_key_t k = { .type = (uint8_t)ev.type, .id = ev.id, .delta = ev.delta };
        link_send(&s_link1, MSG_KEY, &k, sizeof k);
        if (!ACQ_STANDALONE || ev.type != PANEL_EV_PRESS) continue;
        if (ev.id == PANEL_BTN_RUN) {
            xSemaphoreTake(s_set_lock, portMAX_DELAY);
            acq_run_t now = s_set.run;
            xSemaphoreGive(s_set_lock);
            acq_set_run(now == ACQ_RUN ? ACQ_STOP : ACQ_RUN);
            panel_led(PANEL_LED_RUN, now != ACQ_RUN);
        } else if (ev.id == PANEL_BTN_SINGLE) {
            acq_set_run(ACQ_SINGLE);
            panel_led(PANEL_LED_RUN, false);
        }
    }
    
}

// LINK1 RX task (core 0): copy out and return, never block for long.
static void on_link1(const link_frame_t *f, void *ctx)
{
    if (f->type == MSG_PING) {
        link_send(&s_link1, MSG_PONG, f->payload, f->len);        // echo the timestamp
    } else if (f->type == MSG_ACQ_SET && f->len == sizeof(proto_acq_set_t)) {
        proto_acq_set_t m;
        memcpy(&m, f->payload, sizeof m);                        // packed, maybe unaligned
        if (m.run > ACQ_SINGLE || m.trig_src > 1 || m.rate_hz == 0) return;
        xSemaphoreTake(s_set_lock, portMAX_DELAY);
        s_set.rate_hz = m.rate_hz;
        s_set.trig = (trig_cfg_t){ .level = m.trig_level, .hyst = m.trig_hyst,
                                   .edge = m.trig_edge ? TRIG_FALLING : TRIG_RISING };
        s_set.trig_src = m.trig_src;
        s_set.mode = m.trig_mode ? TRIG_NORMAL : TRIG_AUTO;
        s_set.run = (acq_run_t)m.run;
        xSemaphoreGive(s_set_lock);
        if (m.run != ACQ_STOP) xSemaphoreGive(s_wake);
        panel_led(PANEL_LED_RUN, m.run == ACQ_RUN);
    }
}

esp_err_t acq_start(void)
{
    s_set_lock = xSemaphoreCreateMutex();
    s_wake = xSemaphoreCreateBinary();
    if (!s_set_lock || !s_wake) return ESP_ERR_NO_MEM;
    oscil_gpio_output(TIMING_PIN);

    spi2_adc_init(3, ADC_MODE_FULL, true);               // 26.67 MHz, dual-line
    for (int i = 0; i < 3; i++) (void)spi2_adc_frame();  // ADS7883 p.4: first frames invalid
    esp_err_t e = sampler_init(R2);
    if (e != ESP_OK) return e;

    s_link1.on_frame = on_link1;
    e = link_start(&s_link1);                            // this core (0) gets the UART interrupt
    if (e != ESP_OK) return e;


    QueueHandle_t q = panel_start();                     // LEDs, buttons, encoders
    panel_led(PANEL_LED_RUN, true);
    if (xTaskCreatePinnedToCore(acq_panel_task, "acq_panel", 3072, q, 6, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (xTaskCreatePinnedToCore(acq_task, "acq", 4096, NULL, 10, NULL, 1) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}