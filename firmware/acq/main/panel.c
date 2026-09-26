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
#define COUNTS_PER_DETENT 4 //measured below; change to 2 if yours are half-cycle


static QueueHandle_t s_q;

static const int ENC_A[3] = {ACQ_ENC1_A, ACQ_ENC2_A, ACQ_ENC3_A};
static const int ENC_B[3] = {ACQ_ENC1_B, ACQ_ENC2_B, ACQ_ENC3_B};
static uint8_t s_ab[3]; //last AB per encoder
static int8_t s_acc[3]; //transitions not yet turned into a detent

//index = (previous AB <<2) | current AB, with A as bit 1 and B as bit 0.
//Clockwise: 00->10, 10->11, 11->01, 01->00 are +1.  the reverse moves are -1.
static const int8_t QDEC[16] = {
    0, -1, +1, 0, /* 00 */
    +1, 0, 0, -1, /* 01 */
    -1, 0, 0, +1, /* 10 */
    0, +1, -1, 0, /* 11 */
};
static inline uint8_t enc_read(int e) {
    return (uint8_t)((oscil_gpio_read(ENC_A[e]) << 1) | oscil_gpio_read(ENC_B[e]));
}

static void enc_init(void) {
    for (int e = 0; e<3; e++) {
        //no external pull-ups on the schematic
        oscil_gpio_input(ENC_A[e], true);
        oscil_gpio_input(ENC_B[e], true);
        s_ab[e] = enc_read(e);
    }
}

//called every 1 ms from panel_task
static void enc_scan(void) {

    for(int e=0; e<3; e++) {
        uint8_t now = enc_read(e);
        s_acc[e] += QDEC[(s_ab[e] <<2) | now];
        s_ab[e] = now;
        if(s_acc[e] >= COUNTS_PER_DETENT  || s_acc[e] <= -COUNTS_PER_DETENT) {
            int8_t d = s_acc[e] / COUNTS_PER_DETENT; //whole detents
            s_acc[e] -= (d * COUNTS_PER_DETENT); // keep any partial count
            panel_event_t ev = {
                .type = PANEL_EV_TURN,
                .id = e,
                .delta = d
            };
            xQueueSend(s_q, &ev, 0);
        }
    }
}

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
       
        enc_scan();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SCAN_MS));
    }
}

QueueHandle_t panel_start(void) {
    for(int i=0; i<3; i++) oscil_gpio_output(LED_GPIO[i]); //starts low: no boot flash
    for(int i=0; i<PANEL_BTN_COUNT; i++) oscil_gpio_input(BTN_GPIO[i], true); //pull-up on
    enc_init();
    s_q = xQueueCreate(16, sizeof(panel_event_t));
    xTaskCreatePinnedToCore(panel_task, "panel", 3072,NULL,5,NULL,0);
    return s_q;
}