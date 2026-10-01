#pragma once
// Message ids and wire structs for LINK1 and LINK2. All three boards include this one file.
#include <stdint.h>
#include <assert.h>

enum {
    MSG_PING = 0x01, MSG_PONG = 0x02,     // 2 -> 1: u32 timestamp; 1 -> 2: the same bytes back
    MSG_FRAME = 0x10,                     // 1 -> 2: proto_frame_hdr_t + min/max columns
    MSG_KEY = 0x20,                       // 1 -> 2: front-panel event
    MSG_ACQ_SET = 0x30,                   // 2 -> 1: acquisition settings
    MSG_CAL = 0x32,                       // 1 -> 2: oscil_afe_cal_t[2], at boot and after calibration
    MSG_ACQ_STATE = 0x31,                 // 1 -> 2: run state, rate actually achieved
    MSG_GEN_SET = 0x40,                   // 2 -> 3: generator settings
    MSG_STATS = 0x50,                     // either way, optional
};

#define PROTO_COLS 800

typedef struct __attribute__((packed)) {
    float vmin, vmax, vavg, vrms, freq_hz, duty;   // zero until the measurements exist
} proto_meas_t;

typedef struct __attribute__((packed)) {
    uint32_t     frame_no, rate_hz;
    uint16_t     cols;
    uint8_t      triggered, ch_mask;   // ch_mask bit 0 = CH1, bit 1 = CH2
    float        trig_frac;
    proto_meas_t meas[2];
    // followed by uint16_t: CH1 min[cols], CH1 max[cols], CH2 min[cols], CH2 max[cols]
} proto_frame_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t type, id; int8_t delta;       // panel_event_t from acq/main/panel.h, field for field
} proto_key_t;

typedef struct __attribute__((packed)) {
    uint32_t rate_hz;
    uint16_t trig_level, trig_hyst;
    uint8_t  trig_edge;                   // trig_edge_t: 0 rising, 1 falling
    uint8_t  trig_src;                    // 0 CH1, 1 CH2
    uint8_t  trig_mode;                   // 0 AUTO, 1 NORMAL
    uint8_t  run;                         // acq_run_t: 0 RUN, 1 STOP, 2 SINGLE
} proto_acq_set_t;

typedef struct __attribute__((packed)) {
    uint8_t  run;                         // first byte, acq_run_t numbering
    uint32_t rate_hz;                     // achieved, not requested
} proto_acq_state_t;

typedef struct __attribute__((packed)) {
    uint8_t  shape, on, lo, hi;           // shape: dds_shape_t; lo, hi: DAC codes
    uint32_t freq_mhz;                    // millihertz: 1 kHz = 1000000
    uint16_t duty;                        // 0..65535
} proto_gen_set_t;

static_assert(sizeof(proto_meas_t) == 24, "wire format changed");
static_assert(sizeof(proto_frame_hdr_t) == 64, "wire format changed");
static_assert(sizeof(proto_key_t) == 3, "wire format changed");
static_assert(sizeof(proto_acq_set_t) == 12, "wire format changed");
static_assert(sizeof(proto_acq_state_t) == 5, "wire format changed");
static_assert(sizeof(proto_gen_set_t) == 10, "wire format changed");