#include <stdbool.h>
#include "soc/timer_group_struct.h"   // TIMERG0
#include "soc/system_struct.h"        // SYSTEM.perip_clk_en0
#include "soc/interrupts.h"           // ETS_TG0_T0_LEVEL_INTR_SOURCE
#include "esp_private/periph_ctrl.h"  // PERIPH_RCC_ATOMIC
#include "esp_intr_alloc.h"           // esp_intr_alloc()
#include "esp_attr.h"                 // IRAM_ATTR
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oscil_gpio.h"
#include "r2r.h"
#include "gen.h"

#define TIMER_HZ    10000000          // APB 80 MHz / 8
#define TIMING_PIN  13                // spare GPIO13, header 19: high while the ISR runs

static dds_t s_dds;                   // written by gen_set() (core 0), read by the ISR (core 1)
static portMUX_TYPE s_dds_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_out_on;
static intr_handle_t s_isr;

static void IRAM_ATTR dds_isr(void *arg)
{
    oscil_gpio_set(TIMING_PIN);
    TIMERG0.int_clr_timers.val = 1u << 0;              // 1. acknowledge: T0 bit (write-only register)
    TIMERG0.hw_timer[0].config.tn_alarm_en = 1;        // 2. re-arm: hardware cleared it (TRM p.656)
    portENTER_CRITICAL_ISR(&s_dds_lock);               // 3. the work: one sample out
    uint8_t code = s_out_on ? dds_next(&s_dds) : 0;
    portEXIT_CRITICAL_ISR(&s_dds_lock);
    r2r_write(code);
    oscil_gpio_clr(TIMING_PIN);
}

// Runs on core 1, so esp_intr_alloc() puts the interrupt on core 1.
static void dds_timer_start(void *unused)
{
    PERIPH_RCC_ATOMIC() {                               // already on (the task watchdog uses group 0);
        (void)__DECLARE_RCC_ATOMIC_ENV;                 // setting it again is harmless. Never reset
        SYSTEM.perip_clk_en0.timergroup_clk_en = 1;     // the group: that resets the watchdog too.
    }
    TIMERG0.hw_timer[0].config.tn_en = 0;               // stop before touching the prescaler (TRM p.655)
    TIMERG0.hw_timer[0].config.tn_use_xtal = 0;         // APB_CLK, 80 MHz
    TIMERG0.hw_timer[0].config.tn_divider = 8;          // 80 MHz / 8 = 10 MHz
    TIMERG0.hw_timer[0].config.tn_divcnt_rst = 1;       // restart the prescaler
    TIMERG0.hw_timer[0].config.tn_increase = 1;         // count up
    TIMERG0.hw_timer[0].config.tn_autoreload = 1;       // reload on alarm (TRM p.657)

    TIMERG0.hw_timer[0].loadhi.tn_load_hi = 0;          // reload value 0
    TIMERG0.hw_timer[0].loadlo.tn_load_lo = 0;
    TIMERG0.hw_timer[0].load.tn_load = 1;               // any write: counter = reload value now

    TIMERG0.hw_timer[0].alarmhi.tn_alarm_hi = 0;
    TIMERG0.hw_timer[0].alarmlo.tn_alarm_lo = TIMER_HZ / GEN_FS_HZ;   // 100 counts = 10 us

    TIMERG0.int_clr_timers.val = 1u << 0;               // no stale interrupt
    TIMERG0.int_ena_timers.t0_int_ena = 1;              // timer 0 may interrupt (TRM p.669)
    ESP_ERROR_CHECK(esp_intr_alloc(ETS_TG0_T0_LEVEL_INTR_SOURCE,
                                   ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
                                   dds_isr, NULL, &s_isr));

    TIMERG0.hw_timer[0].config.tn_alarm_en = 1;         // arm
    TIMERG0.hw_timer[0].config.tn_en = 1;               // go
    vTaskDelete(NULL);
}

void gen_start(void)
{
    dds_init();
    r2r_init();
    oscil_gpio_output(TIMING_PIN);
    xTaskCreatePinnedToCore(dds_timer_start, "dds_start", 3072, NULL, 5, NULL, 1);
}

void gen_set(dds_shape_t shape, double hz, double duty, uint8_t lo, uint8_t hi, bool on)
{
    dds_t next = {0};
    dds_set(&next, shape, hz, GEN_FS_HZ, duty, lo, hi);  // double math outside the lock
    portENTER_CRITICAL(&s_dds_lock);                     // the ISR waits for a few words' copy at most
    next.phase = s_dds.phase;                            // keep the phase continuous
    s_dds = next;
    s_out_on = on;
    portEXIT_CRITICAL(&s_dds_lock);
}