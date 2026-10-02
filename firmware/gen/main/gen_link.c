#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "oscil_link_uart.h"
#include "oscil_proto.h"
#include "oscil_pins_gen.h"
#include "oscil_status.h"
#include "gen.h"
#include "gen_link.h"

#define LINK_BAUD 2000000             // same on all three boards

static const char *TAG = "link2";
static const char *const SHAPE[] = { "sine", "square", "saw", "triangle", "dc" };

static link_t s_link2 = { .uart = { .port = 1, .tx_pin = -1, .rx_pin = GEN_LINK2_RX, .baud = LINK_BAUD } };
static proto_gen_set_t s_now;         // what the DDS is running, written by on_link2
static bool            s_have;        // false until the first frame
static TickType_t      s_last_rx;     // tick of the last valid frame
static uint32_t        s_frames;      // valid frames since boot
static portMUX_TYPE    s_mux = portMUX_INITIALIZER_UNLOCKED;

static void log_state(const char *why, const proto_gen_set_t *g)
{
    ESP_LOGI(TAG, "%s: %s %lu.%03lu Hz, codes %u..%u, duty %u%%, %s", why,
             SHAPE[g->shape],
             (unsigned long)(g->freq_mhz / 1000), (unsigned long)(g->freq_mhz % 1000),
             g->lo, g->hi, (unsigned)((g->duty * 100u + 32767u) / 65535u), g->on ? "on" : "off");
}

// Runs on the link RX task (core 0). The LED write masks core 0 for about 30 us; the DDS is on core 1.
static void on_link2(const link_frame_t *f, void *ctx)
{
    if (f->type != MSG_GEN_SET || f->len != sizeof(proto_gen_set_t)) return;
    proto_gen_set_t g;
    memcpy(&g, f->payload, sizeof g);                    // packed and possibly unaligned: copy out
    if (g.shape > DDS_DC) return;
    gen_set((dds_shape_t)g.shape, g.freq_mhz / 1000.0, g.duty / 65535.0, g.lo, g.hi, g.on);

    taskENTER_CRITICAL(&s_mux);
    bool changed = !s_have || memcmp(&g, &s_now, sizeof g) != 0;
    s_now = g; s_have = true; s_last_rx = xTaskGetTickCount(); s_frames++;
    taskEXIT_CRITICAL(&s_mux);
    if (changed) log_state("set", &g);                   // only real changes, not the 500 ms resends

    oscil_status_set(16, 16, 16);                        // white flash: a frame arrived
    vTaskDelay(pdMS_TO_TICKS(30));                       // long enough to see; LINK2 carries 2 frames/s
    oscil_status_set(g.on ? 0 : 4, g.on ? 16 : 0, 0);    // then green = on, dim red = off
}

// Every 5 s: what the output is, and whether board 2 is still talking
static void status_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        taskENTER_CRITICAL(&s_mux);
        proto_gen_set_t g = s_now;
        bool have = s_have;
        uint32_t frames = s_frames;
        uint32_t ago_ms = (xTaskGetTickCount() - s_last_rx) * portTICK_PERIOD_MS;
        taskEXIT_CRITICAL(&s_mux);

        if (!have)            ESP_LOGW(TAG, "no frames yet: output held at code 0");
        else if (ago_ms > 1500) {
            ESP_LOGW(TAG, "LINK2 silent %lu ms, holding last setting", (unsigned long)ago_ms);
            log_state("output", &g);
        } else {
            log_state("output", &g);
            ESP_LOGI(TAG, "frames %lu, last %lu ms ago", (unsigned long)frames, (unsigned long)ago_ms);
        }
    }
}

esp_err_t gen_link_start(void)
{
    s_link2.on_frame = on_link2;
    esp_err_t e = link_start(&s_link2);
    if (e != ESP_OK) return e;
    xTaskCreatePinnedToCore(status_task, "gen_status", 3072, NULL, 2, NULL, 0);
    return ESP_OK;
}