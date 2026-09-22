#include "oscil_afe.h"

float oscil_afe_code_to_volts(uint16_t code, const oscil_afe_cal_t *cal) {
    float v_adc = ((float)(code * cal->vref)) / (float)OSCIL_ADC_CODES;
    return cal->gain * (v_adc - cal->offset_v);
}

uint16_t oscil_afe_volts_to_code(float volts, const oscil_afe_cal_t *cal) {
    float v_adc = (volts / cal->gain) + cal->offset_v;
    float  code = v_adc * (float)OSCIL_ADC_CODES / cal->vref + 0.5f;
    if(code < 0.0f) return 0;
    if(code > (float)(OSCIL_ADC_CODES-1)) return OSCIL_ADC_CODES-1;
    return (uint16_t) code;
}