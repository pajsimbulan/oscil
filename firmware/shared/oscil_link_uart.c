#include "oscil_link_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// One lock for every link on the board: link_pack()'s buffer and wire[] below are shared,
// and board 2 sends on LINK1 and LINK2 from different tasks.
static SemaphoreHandle_t s_tx_lock;

static void rx_task(void *arg)
{
    link_t *l = arg;
    oscil_uart_set_reader(&l->uart, xTaskGetCurrentTaskHandle());
    uint8_t chunk[256];
    link_frame_t f;
    for (;;) {
        size_t n = oscil_uart_read(&l->uart, chunk, sizeof chunk, pdMS_TO_TICKS(20));
        for (size_t i = 0; i < n; i++)
            if (link_rx_push(&l->rxs, chunk[i], &f) && l->on_frame) l->on_frame(&f, l->ctx);
    }
}

esp_err_t link_start(link_t *l)
{
    link_rx_init(&l->rxs);
    if (!s_tx_lock) s_tx_lock = xSemaphoreCreateMutex();    // links start one after another in app_main
    if (!s_tx_lock) return ESP_ERR_NO_MEM;
    esp_err_t e = oscil_uart_start(&l->uart, LINK_RX_RING, LINK_TX_RING);
    if (e != ESP_OK) return e;
    if (l->uart.rx_pin >= 0 &&
        xTaskCreatePinnedToCore(rx_task, "link_rx", 4096, l, 9, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}

esp_err_t link_send(link_t *l, uint8_t type, const void *p, uint16_t len)
{
    static uint8_t wire[LINK_MAX_WIRE];
    if (len > LINK_MAX_PAYLOAD) return ESP_ERR_INVALID_SIZE;
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    size_t n = link_pack(type, l->seq, p, len, wire);
    esp_err_t e = oscil_uart_write(&l->uart, wire, n, pdMS_TO_TICKS(100));
    if (e == ESP_OK) l->seq++;                              // a frame never sent is not a gap
    xSemaphoreGive(s_tx_lock);
    return e;
}