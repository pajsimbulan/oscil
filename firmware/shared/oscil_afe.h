#pragma once
#include <stdint.h>

//Converts between ADS7883 codes and the votlage at the BNC.

typedef struct {
    float gain; 
    float offset_v;
    float vref;
} oscil_afe_cal_t;

#define OSCIL_AFE_CAL_NOMINAL {.gain = 4.0f, .offset_v = 1.65f, .vref = 3.3f}
#define OSCIL_ADC_CODES 4096u

float oscil_afe_code_to_voltes(uint16_t code, const oscil_afe_cal_t *cal);
uint16_t  oscil_afe_volts_to_code(float volts, const oscil_afe_cal_t *cal);