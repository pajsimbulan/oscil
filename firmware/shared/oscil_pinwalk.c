#include <stdio.h>
#include "oscil_pinwalk.h"
#include "oscil_gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void pulse(int gpio, int n) {
    for(int i=0; i<n; i++) {
        oscil_gpio_set(gpio);
        esp_rom_delay_us(100);
        oscil_gpio_clr(gpio);
        esp_rom_delay_us(100);
    }
}

void test_pinwalk(const oscil_pin_t *pins, int count) {
    while(1) {
        //1. Walk: pin k (0-based) gets k+1 pulses, one pin after another
        for(int i=0; i<count; i++) {
            oscil_gpio_output(pins[i].gpio);
            pulse(pins[i].gpio, i+1);
            oscil_gpio_input(pins[i].gpio, false);
            esp_rom_delay_us(1000);
        }

        // 2. Read: pull-ups on for every pin, let them settle, then read
        for (int i = 0; i < count; i++) oscil_gpio_input(pins[i].gpio, true);
        vTaskDelay(pdMS_TO_TICKS(10));          // RC settle: 45k x wire capacitance
        for (int i = 0; i < count; i++) {
            printf("%2d pulses  GPIO%-2d %-10s reads %d\n",
                   i + 1, pins[i].gpio, pins[i].name, oscil_gpio_read(pins[i].gpio));
        }
        for (int i = 0; i < count; i++) oscil_gpio_input(pins[i].gpio, false);   // pulls off again
        printf("---\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}