#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oscil_link_uart.h"
#include "oscil_pins_acq.h"
#include "link_test.h"

#define LINK_INJECT 0                 // N > 0: corrupt one byte of every Nth frame on purpose
#define TYPE_TEST   0x7E

static link_t s_loop = { .uart = { .port = 1, .tx_pin = ACQ_LINK1_TX, .rx_pin = ACQ_LINK1_RX, .baud = 2000000 } };
static volatile uint32_t s_sent, s_ok, s_bad;

static uint32_t xorshift(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }

// Frame k: 4-byte counter, then pseudo-random bytes seeded by k; length 5 .. LINK_MAX_PAYLOAD.
static uint16_t make_payload(uint32_t k, uint8_t *p)
{
    uint32_t s = k * 2654435761u + 1;
    uint16_t len = (uint16_t)(5 + xorshift(&s) % (LINK_MAX_PAYLOAD - 4));
    memcpy(p, &k, 4);
    for (uint16_t i = 4; i < len; i++) p[i] = (uint8_t)xorshift(&s);
    return len;
}

static void on_frame(const link_frame_t *f, void *ctx)
{
    static uint8_t want[LINK_MAX_PAYLOAD];
    uint32_t k;
    if (f->type != TYPE_TEST || f->len < 4) { s_bad++; return; }
    memcpy(&k, f->payload, 4);
    uint16_t len = make_payload(k, want);
    if (len == f->len && memcmp(want, f->payload, len) == 0) s_ok++;
    else s_bad++;                                          // passed the CRC but wrong: must stay 0
}

static void sender(void *arg)                              // core 1: the ring crosses cores
{
    static uint8_t p[LINK_MAX_PAYLOAD], wire[LINK_MAX_WIRE];
    for (uint32_t k = 0;; k++) {
        uint16_t len = make_payload(k, p);
        size_t n = link_pack(TYPE_TEST, (uint8_t)k, p, len, wire);   // bypasses link_send to inject
        if (LINK_INJECT && k % LINK_INJECT == LINK_INJECT - 1) {
            wire[n / 2] ^= 0x10;                           // corrupt, but never into a delimiter
            if (wire[n / 2] == 0) wire[n / 2] = 0x01;
        }
        while (oscil_uart_write(&s_loop.uart, wire, n, portMAX_DELAY) != ESP_OK) {}
        s_sent++;
    }
}

void test_link_loop(void)
{
    s_loop.on_frame = on_frame;
    ESP_ERROR_CHECK(link_start(&s_loop));                  // from app_main: ISR on core 0
    xTaskCreatePinnedToCore(sender, "loop_tx", 4096, NULL, 5, NULL, 1);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        const link_rx_t *r = &s_loop.rxs;
        printf("LOOP sent %lu ok %lu bad %lu crc %lu len %lu cobs %lu gaps %lu drops %lu ovf %lu ferr %lu\n",
               (unsigned long)s_sent, (unsigned long)s_ok, (unsigned long)s_bad,
               (unsigned long)r->crc_err, (unsigned long)r->len_err, (unsigned long)r->cobs_err,
               (unsigned long)r->gaps, (unsigned long)s_loop.uart.rx_drops,
               (unsigned long)s_loop.uart.hw_overflows, (unsigned long)s_loop.uart.frame_errors);
    }
}