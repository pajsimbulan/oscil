#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum { PANEL_BTN_RUN, PANEL_BTN_SINGLE, PANEL_BTN_GEN,
               PANEL_BTN_ENC1, PANEL_BTN_ENC2, PANEL_BTN_ENC3, PANEL_BTN_COUNT } panel_btn_t;
               
typedef enum {PANEL_LED_RUN, PANEL_LED_TRIG, PANEL_LED_ARM } panel_led_t;

typedef enum { PANEL_EV_PRESS, PANEL_EV_RELEASE, PANEL_EV_TURN } panel_ev_type_t;

typedef struct {
    panel_ev_type_t type;
    uint8_t id; //panel_btn_t or encoder 0..2 for turn
    int8_t delta;
} panel_event_t;

QueueHandle_t panel_start(void); //start the scan task, events arrive on the queue
void panel_led(panel_led_t led, bool on);
