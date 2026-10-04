#pragma once
#include "lvgl.h"

// LVGL lock held. Metadata first; only the tapped photo is downloaded.
void ui_photos_open(lv_obj_t *previous);
