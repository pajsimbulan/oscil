#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "oscil_link_uart.h"
#include "oscil_pins_display.h"
#include "links.h"

#define LINK_BAUD 2000000                 // same on all three boards (step 25)

static link_t s_l1 = { .uart = { .port = 1, .tx_pin = DISP_LINK1_TX, .rx_pin = DISP_LINK1_RX, .baud = LINK_BAUD } };
static link_t s_l2 = { .uart = { .port = 2, .tx_pin = DISP_LINK2_TX, .rx_pin = -1, .baud = LINK_BAUD } };
static links_handlers_t s_h;
static volatile int64_t s_last_frame_us = -1;
static volatile uint32_t s_rtt_us;

static proto_gen_set_t s_gen;             // LINK2 state, broadcast whole every 500 ms
static portMUX_TYPE s_gen_mux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_gen_task;

static void on_link1(const link_frame_t *f, void *ctx)
{
    switch (f->type) {
    case MSG_FRAME:
        s_last_frame_us = esp_timer_get_time();
        if (s_h.on_frame) s_h.on_frame(f->payload, f->len);
        break;
    case MSG_KEY:
        if (f->len == sizeof(proto_key_t) && s_h.on_key) s_h.on_key((const proto_key_t *)f->payload);
        break;
    case MSG_ACQ_STATE:
        if (s_h.on_acq_state) s_h.on_acq_state(f->payload, f->len);
        break;
    case MSG_CAL:
        if (f->len == 2 * sizeof(oscil_afe_cal_t) && s_h.on_cal) {
            oscil_afe_cal_t cal[2];
            memcpy(cal, f->payload, sizeof cal);          // payload may be unaligned
            s_h.on_cal(cal);
        }
        break;
    case MSG_PONG:
        if (f->len == 4) {
            uint32_t t0;
            memcpy(&t0, f->payload, 4);
            s_rtt_us = (uint32_t)esp_timer_get_time() - t0;
        }
        break;
    default:
        break;
    }
}

// Whole generator state now and every 500 ms: LINK2 has no return path (step 28)
static void gen_tx_task(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
        proto_gen_set_t g;
        taskENTER_CRITICAL(&s_gen_mux);
        g = s_gen;
        taskEXIT_CRITICAL(&s_gen_mux);
        link_send(&s_l2, MSG_GEN_SET, &g, sizeof g);
    }
}

// PING every second, LINK line every 10 s, counters zeroed once 3 s after boot
static void ping_task(void *arg)
{
    uint32_t last_ok = 0;
    for (int s = 1;; s++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t t0 = (uint32_t)esp_timer_get_time();
        link_send(&s_l1, MSG_PING, &t0, sizeof t0);
        if (s == 3) {                                     // ROM boot banner has passed
            link_rx_t *r = &s_l1.rxs;
            r->ok = r->crc_err = r->len_err = r->cobs_err = r->overflows = r->gaps = 0;
            s_l1.uart.rx_drops = s_l1.uart.hw_overflows = s_l1.uart.frame_errors = 0;
            last_ok = 0;
        }
        if (s % 10 == 0) {
            const link_rx_t *r = &s_l1.rxs;
            printf("LINK ok %lu fps %.1f crc %lu len %lu cobs %lu gaps %lu drops %lu ovf %lu ferr %lu rtt_us %lu\n",
                   (unsigned long)r->ok, (r->ok - last_ok) / 10.0,
                   (unsigned long)r->crc_err, (unsigned long)r->len_err, (unsigned long)r->cobs_err,
                   (unsigned long)r->gaps, (unsigned long)s_l1.uart.rx_drops,
                   (unsigned long)s_l1.uart.hw_overflows, (unsigned long)s_l1.uart.frame_errors,
                   (unsigned long)s_rtt_us);
            last_ok = r->ok;
        }
    }
}

esp_err_t links_start(const links_handlers_t *h)
{
    s_h = *h;
    s_l1.on_frame = on_link1;
    esp_err_t e = link_start(&s_l1);
    if (e == ESP_OK) e = link_start(&s_l2);
    if (e != ESP_OK) return e;
    xTaskCreatePinnedToCore(gen_tx_task, "gen_tx", 3072, NULL, 6, &s_gen_task, 0);
    xTaskCreatePinnedToCore(ping_task, "ping", 3072, NULL, 3, NULL, 0);
    return ESP_OK;
}

esp_err_t links_send_acq(const proto_acq_set_t *s)
{
    return link_send(&s_l1, MSG_ACQ_SET, s, sizeof *s);
}

void links_gen_set(const proto_gen_set_t *g)
{
    taskENTER_CRITICAL(&s_gen_mux);
    s_gen = *g;
    taskEXIT_CRITICAL(&s_gen_mux);
    if (s_gen_task) xTaskNotifyGive(s_gen_task);
}

uint32_t links_ms_since_frame(void)
{
    int64_t t = s_last_frame_us;
    return t < 0 ? UINT32_MAX : (uint32_t)((esp_timer_get_time() - t) / 1000);
}

uint32_t links_error_count(void)
{
    const link_rx_t *r = &s_l1.rxs;
    return r->crc_err + r->len_err + r->cobs_err + r->overflows + r->gaps +
           s_l1.uart.rx_drops + s_l1.uart.hw_overflows + s_l1.uart.frame_errors;
}