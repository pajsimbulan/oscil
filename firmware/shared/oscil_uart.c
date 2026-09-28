#include "oscil_uart.h"
#include "oscil_gpio.h"
#include "soc/uart_struct.h"          // UART1, UART2 (uart_dev_t)
#include "soc/system_struct.h"        // SYSTEM: peripheral clock and reset enables
#include "soc/gpio_sig_map.h"         // U1TXD_OUT_IDX, U1RXD_IN_IDX, U2TXD_OUT_IDX, U2RXD_IN_IDX
#include "soc/interrupts.h"           // ETS_UART1_INTR_SOURCE, ETS_UART2_INTR_SOURCE
#include "esp_private/periph_ctrl.h"  // PERIPH_RCC_ATOMIC: ESP-IDF's lock on the SYSTEM registers
#include "esp_heap_caps.h"
#include "esp_attr.h"

#define UART_CLK_HZ   80000000u    // APB_CLK, UART_SCLK_SEL = 1 (TRM p.961)
#define FIFO_LEN      128u         // default RX and TX FIFO size (TRM p.947)

// Bit positions in UART_INT_RAW/ST/ENA/CLR_REG (TRM p.948; uart_struct.h)
#define INT_RXFULL    (1u << 0)
#define INT_TXEMPTY   (1u << 1)
#define INT_FRMERR    (1u << 3)
#define INT_RXOVF     (1u << 4)
#define INT_RXTOUT    (1u << 8)
#define INT_RX_ALL    (INT_RXFULL | INT_RXTOUT | INT_RXOVF | INT_FRMERR)

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;   // guards int_ena between task and ISR

static void IRAM_ATTR uart_isr(void *arg)
{
    oscil_uart_t *u = arg;
    uart_dev_t *hw = u->hw;
    uint32_t st = hw->int_st.val;                         // pending AND enabled
    BaseType_t woken = pdFALSE;

    if (st & INT_RX_ALL) {
        hw->int_clr.val = st & INT_RX_ALL;                // clear first: a byte arriving mid-drain re-raises it
        uint32_t n;
        while ((n = hw->status.rxfifo_cnt) != 0) {        // re-read: bytes keep arriving while we drain
            while (n--) {
                uint8_t b = (uint8_t)hw->fifo.val;        // reading UART_FIFO_REG pops one byte
                if (!ring_put(&u->rx, b)) u->rx_drops++;  // ring full: the task is too slow
            }
        }
        if (st & INT_RXOVF)  u->hw_overflows++;           // the FIFO filled before we got here
        if (st & INT_FRMERR) u->frame_errors++;           // bad stop bit: noise or wrong baud
        TaskHandle_t t = u->reader;
        if (t) vTaskNotifyGiveFromISR(t, &woken);
    }

    if (st & INT_TXEMPTY) {
        uint32_t room = FIFO_LEN - hw->status.txfifo_cnt;
        uint8_t b;
        while (room && ring_get(&u->tx, &b)) { hw->fifo.val = b; room--; }   // 32-bit write per byte
        portENTER_CRITICAL_ISR(&s_lock);                  // tx_kick() may run on the other core
        if (ring_used(&u->tx) == 0) hw->int_ena.txfifo_empty_int_ena = 0;    // nothing left to send
        portEXIT_CRITICAL_ISR(&s_lock);
        hw->int_clr.val = INT_TXEMPTY;
    }

    if (woken) portYIELD_FROM_ISR();                      // run the reader now if it outranks us
}

esp_err_t oscil_uart_start(oscil_uart_t *u, uint32_t rx_size, uint32_t tx_size)
{
    if (u->port != 1 && u->port != 2) return ESP_ERR_INVALID_ARG;
    if ((rx_size & (rx_size - 1)) || (tx_size & (tx_size - 1))) return ESP_ERR_INVALID_SIZE;
    uart_dev_t *hw = (u->port == 1) ? &UART1 : &UART2;
    u->hw = hw;
    u->rx = (oscil_ring_t){ .size = rx_size };            // a buffer only for a direction in use
    u->tx = (oscil_ring_t){ .size = tx_size };
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;   // the IRAM ISR touches them
    if (u->rx_pin >= 0 && !(u->rx.buf = heap_caps_malloc(rx_size, caps))) return ESP_ERR_NO_MEM;
    if (u->tx_pin >= 0 && !(u->tx.buf = heap_caps_malloc(tx_size, caps))) return ESP_ERR_NO_MEM;

    // 1. Bus clock on, then reset, holding the core in reset across it (S3 quirk, uart_ll_reset_register).
    //    SYSTEM registers are shared with every other driver: ESP-IDF's RCC lock.  (TM4C: RCGCUART)
    PERIPH_RCC_ATOMIC() {
        (void)__DECLARE_RCC_ATOMIC_ENV;                   // the macro's marker; uart_ll does the same
        if (u->port == 1) {
            SYSTEM.perip_clk_en0.uart1_clk_en = 1;
            SYSTEM.perip_clk_en0.uart_mem_clk_en = 1;
            SYSTEM.perip_rst_en0.uart1_rst = 0;
            hw->clk_conf.rst_core = 1;
            SYSTEM.perip_rst_en0.uart1_rst = 1;
            SYSTEM.perip_rst_en0.uart1_rst = 0;
            hw->clk_conf.rst_core = 0;
        } else {
            SYSTEM.perip_clk_en1.uart2_clk_en = 1;
            SYSTEM.perip_clk_en0.uart_mem_clk_en = 1;
            SYSTEM.perip_rst_en1.uart2_rst = 0;
            hw->clk_conf.rst_core = 1;
            SYSTEM.perip_rst_en1.uart2_rst = 1;
            SYSTEM.perip_rst_en1.uart2_rst = 0;
            hw->clk_conf.rst_core = 0;
        }
    }

    // 2. Clock source (TRM p.961) and baud (TRM p.928).  (TM4C: IBRD and FBRD)
    hw->clk_conf.sclk_sel = 1;                            // APB_CLK, 80 MHz
    hw->clk_conf.sclk_div_num = 0;                        // pre-divider 0 + 1 = 1
    hw->clk_conf.sclk_en = 1;
    hw->clk_conf.tx_sclk_en = 1;
    hw->clk_conf.rx_sclk_en = 1;
    uint32_t div16 = (UART_CLK_HZ << 4) / u->baud;        // divider in 1/16ths: 2 Mbaud -> 640
    hw->clkdiv.val = ((div16 & 0xFu) << 20) | (div16 >> 4);   // CLKDIV_FRAG [23:20], CLKDIV [11:0]

    // 3. Frame format 8N1.  (TM4C: LCRH = 0x60)
    hw->conf0.bit_num = 3;                                // 3 = 8 data bits
    hw->conf0.parity_en = 0;
    hw->conf0.stop_bit_num = 1;                           // 1 = one stop bit

    // 4. FIFOs and thresholds (TRM p.928, p.930).
    hw->conf0.rxfifo_rst = 1; hw->conf0.rxfifo_rst = 0;
    hw->conf0.txfifo_rst = 1; hw->conf0.txfifo_rst = 0;
    hw->conf1.rxfifo_full_thrhd = 64;                     // RX interrupt above half full
    hw->conf1.txfifo_empty_thrhd = 32;                    // TX interrupt below 32 bytes
    hw->mem_conf.rx_tout_thrhd = 20;                      // idle 20 bit times = 2 bytes
    hw->conf1.rx_tout_en = 1;
    // UART_ID_REG.high_speed resets to 1: registers sync to the core clock by themselves
    // because the core runs from APB. No UART_REG_UPDATE write needed (TRM p.942).

    // 5. Pins through the GPIO matrix: output TRM p.477, input p.475, signal numbers p.483.
    if (u->tx_pin >= 0) {
        int sig = (u->port == 1) ? U1TXD_OUT_IDX : U2TXD_OUT_IDX;
        oscil_gpio_set(u->tx_pin);                        // idle high before the driver turns on
        GPIO.pin[u->tx_pin].pad_driver = 0;               // push-pull
        GPIO.func_out_sel_cfg[u->tx_pin].func_sel = sig;  // this pin carries UARTn TXD
        GPIO.func_out_sel_cfg[u->tx_pin].oen_sel  = 1;    // output enable from GPIO_ENABLE, not the UART
        REG_SET_FIELD(OSCIL_IOMUX_REG(u->tx_pin), MCU_SEL, 1);   // pad function 1 = GPIO matrix
        if (u->tx_pin < 32) GPIO.enable_w1ts = 1u << u->tx_pin;
        else                GPIO.enable1_w1ts.val = 1u << (u->tx_pin - 32);
    }
    if (u->rx_pin >= 0) {
        int sig = (u->port == 1) ? U1RXD_IN_IDX : U2RXD_IN_IDX;
        oscil_gpio_input(u->rx_pin, true);                // pull-up: an open link reads idle
        GPIO.func_in_sel_cfg[sig].func_sel = u->rx_pin;   // UARTn RXD listens to this pin
        GPIO.func_in_sel_cfg[sig].sig_in_sel = 1;         // through the matrix
    }

    // 6. Interrupts. esp_intr_alloc() routes the source to the calling core (TRM p.539).
    hw->int_ena.val = 0;
    hw->int_clr.val = 0xFFFFFFFFu;
    hw->int_ena.val = (u->rx_pin >= 0) ? INT_RX_ALL : 0;  // TX-empty is enabled on demand
    int src = (u->port == 1) ? ETS_UART1_INTR_SOURCE : ETS_UART2_INTR_SOURCE;
    return esp_intr_alloc(src, ESP_INTR_FLAG_IRAM, uart_isr, u, &u->isr);
}

void oscil_uart_set_reader(oscil_uart_t *u, TaskHandle_t t) { u->reader = t; }

size_t oscil_uart_read(oscil_uart_t *u, uint8_t *dst, size_t max, TickType_t wait)
{
    if (ring_used(&u->rx) == 0) ulTaskNotifyTake(pdTRUE, wait);   // sleep until the ISR says so
    size_t n = 0;
    while (n < max && ring_get(&u->rx, &dst[n])) n++;
    return n;
}

static void tx_kick(oscil_uart_t *u)
{
    uart_dev_t *hw = u->hw;
    portENTER_CRITICAL(&s_lock);                          // the ISR clears this bit under the same lock
    hw->int_ena.txfifo_empty_int_ena = 1;                 // fires at once: the FIFO is below 32
    portEXIT_CRITICAL(&s_lock);
}

esp_err_t oscil_uart_write(oscil_uart_t *u, const uint8_t *src, size_t n, TickType_t wait)
{
    if (!u->tx.buf || n > u->tx.size) return ESP_ERR_INVALID_SIZE;
    TickType_t t0 = xTaskGetTickCount();
    while (ring_free(&u->tx) < n) {                       // whole frame or nothing: never half a frame
        tx_kick(u);
        if (xTaskGetTickCount() - t0 >= wait) return ESP_ERR_TIMEOUT;
        vTaskDelay(1);
    }
    for (size_t i = 0; i < n; i++) ring_put(&u->tx, src[i]);
    tx_kick(u);
    return ESP_OK;
}