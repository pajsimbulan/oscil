#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "oscil_afe.h"
#include "oscil_proto.h"

// Board 2's two links: LINK1 to board 1 (UART1, both ways) and LINK2 to board 3
// (UART2, transmit only). Handlers run on the LINK1 RX task: copy and return, never block.
typedef struct {
    void (*on_frame)(const uint8_t *payload, uint16_t len);
    void (*on_key)(const proto_key_t *k);
    void (*on_acq_state)(const uint8_t *payload, uint16_t len);
    void (*on_cal)(const oscil_afe_cal_t cal[2]);
} links_handlers_t;

esp_err_t links_start(const links_handlers_t *h);   // call from a core-0 task (app_main)
esp_err_t links_send_acq(const proto_acq_set_t *s);
void      links_gen_set(const proto_gen_set_t *g);   // sent now, then every 500 ms
uint32_t  links_ms_since_frame(void);                // UINT32_MAX before the first frame
uint32_t  links_error_count(void);                   // every LINK1 error counter, summed