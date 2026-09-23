#include "oscil_status.h"
#include "oscil_gpio.h"
#include "sdkconfig.h"
#include "esp_attr.h" ///IRAM_ATTR
#include "soc/gpio_struct.h" // GPIO register map (TRM ch.6)
#include "esp_cpu.h" //cpu cycle counter
#include "esp_rom_sys.h" //esp_rom_delay_us
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ---- WS2812B timing, datasheet p.5 --------------------
#define CPU_HZ   (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000ULL)


#define T0H   (400ULL  * CPU_HZ / 1000000000ULL) //0.4 us & 240MHz = 96 ticks
#define T1H   (850ULL  * CPU_HZ / 1000000000ULL) //0.85 us * 240Mhz = 204
#define TBIT  (1250ULL * CPU_HZ / 1000000000ULL) // 1.25us * 240MHz = 300
#define T_RESET_US 300 //Res low > 50 us (new parts ~280 us)

static volatile uint32_t *s_set_reg, *s_clr_reg; // out_w1ts/out_w1tc 
static uint32_t s_mask;  // pin's bit in whichever bank.
static uint8_t s_r, s_g, s_b;

esp_err_t oscil_status_init(int gpio) {
    if(gpio<0 || gpio >48) return ESP_ERR_INVALID_ARG;
    if(gpio<32) {
        s_set_reg = &GPIO.out_w1ts;
        s_clr_reg = &GPIO.out_w1tc;
        s_mask = 1u << gpio;
    } else {
        s_set_reg = &GPIO.out1_w1ts.val;
        s_clr_reg = &GPIO.out1_w1tc.val;
        s_mask = 1u << (gpio - 32);
    }

    oscil_gpio_output(gpio); //set as output
    esp_rom_delay_us(T_RESET_US); // idle low = clean reset before data frame
    oscil_status_set(0,0,0); // start from 000 known state, off
    return ESP_OK;
}

// IRAM_ATTR so a flash cacche miss cant stretch a pulse
void IRAM_ATTR oscil_status_set(uint8_t r, uint8_t g, uint8_t b) {
    //datasheet p.6: GRB order, most significant bit first
    uint32_t grb = ((uint32_t)g <<16) | ((uint32_t)r << 8) | b;
    volatile uint32_t *set = s_set_reg, *clr = s_clr_reg;
    uint32_t m = s_mask;

    uint32_t irq = portSET_INTERRUPT_MASK_FROM_ISR(); //~30 us, nothing on this core may cut in

    for(int i=23; i>= 0; i--) {
        uint32_t t0 = esp_cpu_get_cycle_count();
        uint32_t high = ((grb >> i) &1u)?  T1H : T0H;
       
        *set = m; //rising edge starts the bit
        while((esp_cpu_get_cycle_count() - t0) < high) {};
        *clr = m; // failling edge: the width sets 0 or 1 
        while ((esp_cpu_get_cycle_count() -t0) <TBIT) {};
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR(irq);
    esp_rom_delay_us(T_RESET_US);  //hold low
}
 
static void heartbeat_task(void *arg) {
    while(1) {
        oscil_status_set(s_r, s_g, s_b);
        vTaskDelay(pdMS_TO_TICKS(950));
        oscil_status_set(0,0,0);
        vTaskDelay(pdMS_TO_TICKS(950));
    }
}

esp_err_t oscil_status_start_heartbeat(uint8_t r, uint8_t g, uint8_t b) {
    s_r = r; s_g=g; s_b=b;
    if(xTaskCreatePinnedToCore(heartbeat_task, "heartbeat",2048, NULL, 1, NULL,0) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}


