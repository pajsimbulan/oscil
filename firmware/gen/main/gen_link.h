#pragma once
#include "esp_err.h"

// LINK2 receiver: applies every MSG_GEN_SET from board 2 and shows it on the status LED.
// Call from app_main (core 0) after gen_start() and oscil_status_init().
esp_err_t gen_link_start(void);