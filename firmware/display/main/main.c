#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "oscil_pins_display.h"
#include "oscil_status.h"
#include "oscil_pinwalk.h"
#include "oscil_link_uart.h"
#include "oscil_proto.h"

static const char *TAG = "disp";
#include "oscil_pinwalk.h"

static const oscil_pin_t DISP_PINS[] __attribute__((unused))  = {
    { DISP_LCD_R5, "R5" },  { DISP_LCD_R6, "R6" },  { DISP_LCD_R7, "R7" },  { DISP_LCD_G2, "G2" },
    { DISP_LCD_G3, "G3" },  { DISP_LCD_G4, "G4" },  { DISP_LCD_B5, "B5" },  { DISP_LCD_B6, "B6" },
    { DISP_LCD_B7, "B7" },  { DISP_LCD_G5, "G5" },  { DISP_LCD_DCLK, "DCLK" }, { DISP_LCD_G7, "G7" },
    { DISP_LCD_B3, "B3" },  { DISP_LCD_HSYNC, "HSYNC" }, { DISP_LCD_VSYNC, "VSYNC" }, { DISP_LCD_DE, "DE" },
    { DISP_TOUCH_SDA, "SDA" }, { DISP_TOUCH_SCL, "SCL" }, { DISP_TOUCH_INT, "T_INT" }, { DISP_LINK2_TX, "LINK2_TX" },
    { DISP_LCD_R3, "R3" },  { DISP_LINK1_TX, "LINK1_TX" }, { DISP_LINK1_RX, "LINK1_RX" }, { DISP_LCD_R4, "R4" },
    { DISP_LCD_G6, "G6" },  { DISP_LCD_B4, "B4" },
};

#define LINK_BAUD 2000000                 // same on all three boards

static link_t s_l1 = { .uart = { .port = 1, .tx_pin = DISP_LINK1_TX, .rx_pin = DISP_LINK1_RX, .baud = LINK_BAUD } };
static volatile uint32_t s_rtt_us;

// LINK1 RX task (core 0): print or copy, never block for long.
static void on_link1(const link_frame_t *f, void *ctx)
{
    switch (f->type) {
    case MSG_FRAME:                                       // counted in s_l1.rxs.ok; not drawn yet
        break;
    case MSG_KEY:
        if (f->len == sizeof(proto_key_t)) {
            const proto_key_t *k = (const proto_key_t *)f->payload;   // bytes only: no alignment issue
            printf("key %u %u %d\n", k->type, k->id, k->delta);
        }
        break;
    case MSG_ACQ_STATE:
        if (f->len == sizeof(proto_acq_state_t)) {
            proto_acq_state_t s;
            memcpy(&s, f->payload, sizeof s);             // packed u32 at offset 1: copy out
            printf("acq state run %u rate %lu\n", s.run, (unsigned long)s.rate_hz);
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
        if (s == 10) {                                    // temporary: one settings change
            const proto_acq_set_t a = { .rate_hz = 16000, .trig_level = 2560, .trig_hyst = 50,
                                        .trig_edge = 0, .trig_src = 0, .trig_mode = 0, .run = 0 };
            link_send(&s_l1, MSG_ACQ_SET, &a, sizeof a);
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


void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_bytes = 0;
    esp_flash_get_size(NULL, &flash_bytes);

    ESP_LOGI(TAG, "Oscil board 2: display");
    ESP_LOGI(TAG, "cores %d, silicon v%d.%d",
             chip.cores, chip.revision / 100, chip.revision % 100);
    ESP_LOGI(TAG, "flash %" PRIu32 " MB", flash_bytes / (1024 * 1024));
    ESP_LOGI(TAG, "psram %u MB", (unsigned)(esp_psram_get_size() / (1024 * 1024)));
    ESP_LOGI(TAG, "tick %d Hz, LINK1 on GPIO%d/%d",
             configTICK_RATE_HZ, DISP_LINK1_TX, DISP_LINK1_RX);
    ESP_ERROR_CHECK(oscil_status_init(DISP_TOUCH_RESET));
    ESP_ERROR_CHECK(oscil_status_start_heartbeat(16,0,0)); //red
    ESP_LOGI(TAG,"heartbeat on GPIO%d", DISP_TOUCH_RESET);

    s_l1.on_frame = on_link1;
    ESP_ERROR_CHECK(link_start(&s_l1));                           // app_main is on core 0
    xTaskCreatePinnedToCore(ping_task, "ping", 3072, NULL, 3, NULL, 0);


    //test_pinwalk(DISP_PINS, sizeof DISP_PINS / sizeof DISP_PINS[0]);
}