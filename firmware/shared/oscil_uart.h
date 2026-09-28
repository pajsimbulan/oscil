#pragma once
// Register-level UART1/UART2 driver: one ISR, an RX ring and a TX ring.
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oscil_ring.h"

// Must live in internal RAM (a static, not PSRAM): the IRAM ISR reads it.
typedef struct {
    int      port;              // 1 or 2
    int      tx_pin, rx_pin;    // -1 = not used
    uint32_t baud;
    // filled in by oscil_uart_start()
    void    *hw;                // uart_dev_t *
    intr_handle_t isr;
    oscil_ring_t rx, tx;
    TaskHandle_t volatile reader;                  // task the ISR notifies when bytes arrive
    volatile uint32_t rx_drops, hw_overflows, frame_errors;
} oscil_uart_t;

// Call from the core the ISR should run on. Ring sizes: powers of two.
esp_err_t oscil_uart_start(oscil_uart_t *u, uint32_t rx_size, uint32_t tx_size);
void      oscil_uart_set_reader(oscil_uart_t *u, TaskHandle_t t);
// Up to max bytes; sleeps up to wait ticks if none are waiting. Returns the count.
size_t    oscil_uart_read(oscil_uart_t *u, uint8_t *dst, size_t max, TickType_t wait);
// All n bytes or none: waits up to wait ticks for room. One writer at a time per UART.
esp_err_t oscil_uart_write(oscil_uart_t *u, const uint8_t *src, size_t n, TickType_t wait);