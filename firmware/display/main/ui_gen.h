#pragma once
#include "lvgl.h"
#include "oscil_proto.h"

lv_obj_t *ui_gen_create(void);                  // builds the generator screen; LVGL lock held
void      ui_gen_sync(const proto_gen_set_t *g);    // widgets follow the state; LVGL lock held