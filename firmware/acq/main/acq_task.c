#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "oscil_gpio.h"
#include "oscil_trigger.h"
#include "oscil_decimate.h"
#include "panel.h"
#include "spi2_adc.h"
#include "sampler.h"
#include "acq_task.h"

#define R2            (2 * ACQ_R)      // burst length: the trigger is searched in the middle half
#define TIMING_PIN    14               // spare GPIO14, header 20
#define TRIG_LED_MS   30               // TRIG flash length
#define AUTO_BURSTS   3                // AUTO: show an untriggered frame after this many misses

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

typedef struct { uint32_t frames, trigs, autos, errors; } acq_stats_t;
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
    decimate_minmax(c1, ACQ_R, s_frame.mn[0], s_frame.mx[0], ACQ_COLS);
    decimate_minmax(c2, ACQ_R, s_frame.mn[1], s_frame.mx[1], ACQ_COLS);
    s_frame.seq++;
    s_frame.rate_hz = info->rate_hz;
    s_frame.frac = frac;
    s_frame.triggered = triggered;
}

// Once a second: throughput and the worst-case stack use of this task.
static void stats_line(TickType_t *last)
{
    TickType_t now = xTaskGetTickCount();
    if (now - *last < pdMS_TO_TICKS(1000)) return;
    printf("acq: %lu frames/s  trig %lu  auto %lu  errors %lu  stack free %u words\n",
           (unsigned long)s_stats.frames, (unsigned long)s_stats.trigs,
           (unsigned long)s_stats.autos, (unsigned long)s_stats.errors,
           (unsigned)uxTaskGetStackHighWaterMark(NULL));
    s_stats = (acq_stats_t){ 0 };
    *last = now;
}

static void acq_task(void *arg)
{
    static uint16_t ch1[R2];
    static uint16_t ch2[R2];
    int misses = 0;
    TickType_t trig_led_off = 0;
    TickType_t last_stats = xTaskGetTickCount();
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));           // watch this task, not core 1's idle task

    for (;;) {
        acq_settings_t st;
        xSemaphoreTake(s_set_lock, portMAX_DELAY);
        st = s_set;
        xSemaphoreGive(s_set_lock);

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
            continue;
        }

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
          s_stats.frames++;                                // the link to board 2 will send s_frame here
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
        if (!xQueueReceive(q, &ev, portMAX_DELAY) || ev.type != PANEL_EV_PRESS) continue;
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

    QueueHandle_t q = panel_start();                     // LEDs, buttons, encoders
    panel_led(PANEL_LED_RUN, true);
    if (xTaskCreatePinnedToCore(acq_panel_task, "acq_panel", 3072, q, 6, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (xTaskCreatePinnedToCore(acq_task, "acq", 4096, NULL, 10, NULL, 1) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}