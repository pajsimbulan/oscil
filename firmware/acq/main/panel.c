#include "panel.h"
#include "freertos/idf_additions.h"
#include "oscil_gpio.h"
#include "oscil_pins_acq.h"
#include "freertos/task.h"

static const int BTN_GPIO[PANEL_BTN_COUNT] = {
    ACQ_BTN_RUN, ACQ_BTN_SINGLE, ACQ_BTN_GEN, ACQ_ENC1_SW, ACQ_ENC2_SW, ACQ_ENC3_SW 
};

static const int LED_GPIO[3] = {ACQ_LED_RUN, ACQ_LED_TRIG, ACQ_LED_ARM };

#define SCAN_MS 1 //1 kHz scan (one freeRTOS tick)
#define STABLE_N 20 //20 scans x 1 ms = 20 ms of agreement before a change counts

static QueueHandle_t s_q;

void panel_led(panel_led_t led, bool on) {
    if(on) oscil_gpio_set(LED_GPIO[led]);
    else oscil_gpio_clr(LED_GPIO[led]);
}

static void panel_task(void *arg) {
    bool state[PANEL_BTN_COUNT] = {0}; //debouned: true = pressed
    uint8_t count[PANEL_BTN_COUNT] = {0}; //consecutive scans that disagree
    TickType_t last = xTaskGetTickCount();
    
    while(1) {
        for(int i=0; i<PANEL_BTN_COUNT; i++) {
            bool raw = (!oscil_gpio_read(BTN_GPIO[i])); //active low
            
            //aggrees: reset the conter
            if(raw == state[i]) {
                count[i] = 0;
                continue;
            }
            if(++count[i] >= STABLE_N) {
                state[i] = raw;
                count[i] = 0;
                panel_event_t ev = {
                    .type = raw? PANEL_EV_PRESS: PANEL_EV_RELEASE,
                    .id = i
                };
                xQueueSend(s_q, &ev, 0); // never block the scan
            }
        }
        //for encoders, code go here. later
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SCAN_MS));
    }
}

QueueHandle_t panel_start(void) {
    for(int i=0; i<3; i++) oscil_gpio_output(LED_GPIO[i]); //starts low: no boot flash
    for(int i=0; i<PANEL_BTN_COUNT; i++) oscil_gpio_input(BTN_GPIO[i], true); //pull-up on
    s_q = xQueueCreate(16, sizeof(panel_event_t));
    xTaskCreatePinnedToCore(panel_task, "panel", 3072,NULL,5,NULL,0);
    return s_q;
}