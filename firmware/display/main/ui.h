#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "oscil_proto.h"
#include "oscil_trigger.h"
#include "scope_view.h"

typedef enum { MODE_SCOPE, MODE_GEN } ui_mode_t;

// Same numbering as board 1's acq_run_t and trig_mode_t (acq_task.c, step 18)
enum { UI_RUN = 0, UI_STOP = 1, UI_SINGLE = 2 };
enum { UI_TRIG_AUTO = 0, UI_TRIG_NORMAL = 1 };

typedef enum {
    ACT_ENC1, ACT_ENC2, ACT_ENC3,       // arg: detents, + clockwise
    ACT_PUSH1, ACT_PUSH2, ACT_PUSH3,
    ACT_RUN, ACT_SINGLE, ACT_GEN,
    ACT_CH,                             // arg: channel; on/off, and it becomes the V/div channel
    ACT_TRIG,                           // cycle edge, then source, then AUTO/NORMAL
    ACT_MEAS, ACT_PHOSPHOR, ACT_SAVE, ACT_SYS,
    ACT_SHUNT,                          // arg: channel; opens the keypad (0 ohm = volts mode)
    ACT_GEN_SHAPE,                      // arg: dds_shape_t
    ACT_GEN_FREQ,                       // arg: Hz
    ACT_GEN_AMP,                        // arg: hi - lo, codes
    ACT_GEN_OFFSET,                     // arg: (hi + lo) / 2, codes
    ACT_GEN_DUTY,                       // arg: percent
    ACT_GEN_ON,                         // arg: 0 or 1
    ACT_ACQ_STATE,                      // arg: board 1's run state (SINGLE ends in STOP)
} ui_action_t;

typedef struct {
    ui_mode_t  mode;
    uint8_t    tb_idx;                  // index into the timebase table
    uint8_t    sel_ch;                  // channel the V/div knob controls
    uint8_t    vdiv_idx[2];
    view_t     view;                    // what scope_view draws
    trig_cfg_t trig;
    uint8_t    trig_src, trig_mode;
    uint8_t    run;                     // UI_RUN / UI_STOP / UI_SINGLE
    uint8_t    fstep_idx;               // generator frequency step: 1, 10, 100, 1000 Hz
    proto_gen_set_t gen;
} ui_settings_t;

// Optional features plug in here from main.c, so ui.c never depends on them
typedef struct {
    void (*changed)(const ui_settings_t *s);    // settings module: save after 2 s of quiet
    void (*save)(const ui_settings_t *s);       // store module: SAVE pressed
    void (*sys)(void);                          // system panel; SYS button hidden if NULL
} ui_hooks_t;

void ui_defaults(ui_settings_t *s);
void ui_start(const ui_settings_t *initial, const ui_hooks_t *hooks);   // LVGL lock NOT held
void ui_apply(ui_action_t a, int arg);          // LVGL lock held (touch handlers already have it)
void ui_post(ui_action_t a, int arg);           // any task: queued to ui_task, which locks
void ui_on_key(const proto_key_t *k);           // LINK1 RX task
void ui_on_acq_state(const uint8_t *p, uint16_t len);   // LINK1 RX task
void ui_toast(const char *msg);                 // any task
void ui_keypad_open(const char *title, void (*done)(float value, void *ctx), void *ctx);   // LVGL lock held