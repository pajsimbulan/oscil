#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"
#include "oscil_afe.h"
#include "oscil_proto.h"

#define SCOPE_W      800                 // plot size: 10 x 8 divisions of 80 x 50 px
#define SCOPE_H      400
#define SCOPE_DIV_X  80
#define SCOPE_DIV_Y  50
#define SCOPE_FRAME_MAX (sizeof(proto_frame_hdr_t) + 4 * PROTO_COLS * sizeof(uint16_t))

typedef struct {
    bool  on;
    float volts_per_div;
    float offset_v;                      // volts shown at the centre line
    bool  amps;                          // current mode: volts across shunt_ohm
    float shunt_ohm;
} view_ch_t;

typedef struct {
    view_ch_t ch[2];
    bool phosphor;
    bool meas_panel;
} view_t;

esp_err_t scope_view_init(lv_obj_t *parent, int y);         // LVGL lock held; starts scope_task
void      scope_view_set_view(const view_t *v);             // LVGL lock held
void      scope_view_set_cal(const oscil_afe_cal_t cal[2]); // any task
void      scope_view_get_cal(int ch, oscil_afe_cal_t *out); // any task
void      scope_view_submit(const uint8_t *payload, uint16_t len);   // link RX task: copy, wake, return
size_t    scope_view_copy_last(uint8_t *dst, size_t cap);   // last drawn frame, for SAVE; 0 if none
float     scope_view_fps(void);