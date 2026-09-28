#include "soc/ledc_struct.h" //LEDC
#include "soc/system_struct.h"
#include "soc/gpio_sig_map.h" // LEDC_LS_SIG_OUT0_IDX
#include "oscil_gpio.h"
#include "freertos/FreeRTOS.h"
#include "testsig.h"

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

//50% square wave on 'pin' from LEDC timer 0, channel 0.  TRM ch.35.
void testsig_start(int pin, uint32_t hz) {
    portENTER_CRITICAL(&s_lock);
    SYSTEM.perip_clk_en0.ledc_clk_en = 1; 
    SYSTEM.perip_rst_en0.ledc_rst = 1;
    SYSTEM.perip_rst_en0.ledc_rst = 0;
    portEXIT_CRITICAL(&s_lock);

    LEDC.conf.apb_clk_sel = 1; // 80 MHz APB clock (TRM p.1312)
    LEDC.conf.clk_en = 1;

    //timer 0: f = 80 MHz / (DIV x 2^10).  DIV in 1/256 units (TRM p.1312).
    uint32_t div_q8 = (uint32_t)((80000000ull * 256u) / ((uint64_t)hz * 1024u));
    LEDC.timer_group[0].timer[0].conf.duty_resolution = 10;
    LEDC.timer_group[0].timer[0].conf.clock_divider = div_q8; // 1 kHz -> 20000 = 78.125
    LEDC.timer_group[0].timer[0].conf.tick_sel = 0;
    LEDC.timer_group[0].timer[0].conf.pause = 0;
    LEDC.timer_group[0].timer[0].conf.rst = 1;
    LEDC.timer_group[0].timer[0].conf.rst = 0;
    LEDC.timer_group[0].timer[0].conf.low_speed_update = 1; //PARA_UP: apply

    //Channel 0: high for 512 of 1024 counts = 50%
    LEDC.channel_group[0].channel[0].conf0.timer_sel = 0;
    LEDC.channel_group[0].channel[0].hpoint.hpoint = 0;
    LEDC.channel_group[0].channel[0].duty.duty = 512u << 4;
    LEDC.channel_group[0].channel[0].conf1.duty_inc = 1;
    LEDC.channel_group[0].channel[0].conf1.duty_num = 1;
    LEDC.channel_group[0].channel[0].conf1.duty_cycle = 1;
    LEDC.channel_group[0].channel[0].conf1.duty_scale = 0;
    LEDC.channel_group[0].channel[0].conf1.duty_start = 1;
    LEDC.channel_group[0].channel[0].conf0.sig_out_en = 1;
    LEDC.channel_group[0].channel[0].conf0.low_speed_update = 1; //apply

    //Route LEDC channel 0 to the pin through the GPIO matrix (TRM p.477 steps 1 and 3).
    GPIO.func_out_sel_cfg[pin].func_sel = LEDC_LS_SIG_OUT0_IDX;
    GPIO.func_out_sel_cfg[pin].oen_sel = 1; //alwas an output
    REG_SET_FIELD(OSCIL_IOMUX_REG(pin), MCU_SEL, 1);
    if(pin < 32) GPIO.enable_w1ts = 1u << pin;
    else GPIO.enable1_w1ts.val = 1u << (pin-32);
}