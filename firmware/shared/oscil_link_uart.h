#pragma once
// The link protocol (oscil_link) carried by the UART driver (oscil_uart).
#include <stdint.h>
#include "esp_err.h"
#include "oscil_link.h"
#include "oscil_uart.h"

typedef void (*link_handler_t)(const link_frame_t *f, void *ctx);   // runs on the link's RX task

typedef struct {
    oscil_uart_t   uart;             // set .port, .tx_pin, .rx_pin, .baud before link_start()
    link_handler_t on_frame; void *ctx;
    link_rx_t      rxs;              // decoder and its error counters
    uint8_t        seq;
} link_t;

#define LINK_RX_RING 16384           // 80 ms at 200 KB/s
#define LINK_TX_RING 8192            // holds one LINK_MAX_WIRE frame

esp_err_t link_start(link_t *l);     // call from a core-0 task: the UART ISR lands on that core
// Queues one whole frame, or returns ESP_ERR_TIMEOUT after 100 ms with nothing sent.
esp_err_t link_send(link_t *l, uint8_t type, const void *p, uint16_t len);