#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "panel.h"
#include "panel_test.h"

//never returns
void test_panel(void) {
    QueueHandle_t q = panel_start();
    static const char *NAME[PANEL_BTN_COUNT] = {
        "RUN", "SINGLE", "GEN", "ENC1", "ENC2", "ENC3"
    };
    bool led[3] = {0};
    panel_event_t ev;

    printf("panel test: press buttons, RUN/SINGLE/GEN toggle an LED\n");
    while(1) {

        //sleeps until an event arrives
        if(xQueueReceive(q, &ev, portMAX_DELAY)) {
            if(ev.type == PANEL_EV_TURN) {
                printf("enc%d %+d \n", ev.id+1, ev.delta); //e.g "enc1 +1"
                continue;
            }
            printf("%s %s\n", NAME[ev.id], ev.type == PANEL_EV_PRESS? "press":"release");

            if(ev.type == PANEL_EV_PRESS && ev.id <3) {
                //RUN->LED_RUN, SINGLE->LED_TRIG, GEN->LED_ARM
                led[ev.id] = !led[ev.id];
                panel_led((panel_led_t)ev.id, led[ev.id]);
            }
        }

    }
}