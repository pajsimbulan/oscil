#include <stdbool.h>
#include <string.h>
#include "sampler.h"
#include "hal/gdma_types.h"
#include "spi2_adc.h"
#include "soc/spi_struct.h" // GPSPI2
#include "soc/interrupts.h" // ETS_SPI2_INTR_SOURCE
#include "esp_attr.h" //IRAM_ATTR
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_heap_caps.h"
#include "esp_private/gdma.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SEG_LOW_APB 124u //CS low per sample, APB cycles: (1 setup +16) SCLKS x3 = 51
#define SEG_MIN_GAP 0u //smallest_conf_bitlen that still gives correct data

#define APB_HZ 80000000u
#define CONF_BITLEN_MAX 0x3FFFAu //18-bit field,  ESP-IDF's SOC_SPI_SCT_CONF_BITLEN_MAX    
#define SEG_MAGIC 0xAu //any 4bit vale, checked against every conf buffer
#define CONF_WORDS 15 // 1 bitmap word +14 registers (TRM table 30.15-11)
#define CONF_BITMAP_ALL 0x7FFFu   // all 15 words valid (same as ESP-IDF)

//GDMA linked-list descriptor, three words. TRM ch.3 3.4.1  p360 to 361
typedef struct seg_desc {
    uint32_t size : 12; //buffer size in bytes
    uint32_t length : 12; //valid bytes (TX:written by us,  RX writthen by the GDMA)
    uint32_t : 4;
    uint32_t err_eof:1;
    uint32_t : 1;
    uint32_t suc_eof:1; //TX:1 = end of this segment's CONF data. RX: we write 0
    uint32_t owner :1; //1 = GDMA may use the buffer
    void *buf;
    struct seg_desc *next; // NULL on the last one
} seg_desc_t;

_Static_assert(sizeof(seg_desc_t) == 12, "GDMA descriptor must be 3 words");

static uint32_t s_conf_mid[CONF_WORDS]; //segments 0 .. n-2: usr_conf_nxt = 1
static uint32_t s_conf_last[CONF_WORDS]; //final segment: usr_conf_nxt = 0
static seg_desc_t *s_tx; // one CONF descriptor per sample
static seg_desc_t *s_rx; // one 4-byte RX descriptor per sample
static uint32_t *s_raw; //one 32-bit dual-line word per sample
static size_t s_max;
static gdma_channel_handle_t s_dma_tx;
static gdma_channel_handle_t s_dma_rx;
static TaskHandle_t volatile s_waiter;
static volatile uint32_t s_int_st; //what ended the burst, for the caller

//Fires once per burst: SEG_TRANS_DONE when the last word is in memory, with
//SEG_MAGIC_ERR also set if a CONF buffer was rejected (TRM p.1134).
static void IRAM_ATTR seg_done_isr(void *arg) {
    uint32_t st = GPSPI2.dma_int_st.val;
    GPSPI2.dma_int_clr.val = st;
    s_int_st |= st;
    BaseType_t woke = pdFALSE;
    if(s_waiter) vTaskNotifyGiveFromISR(s_waiter, &woke);
    portYIELD_FROM_ISR(woke);
}

//Copy the 14 registers into a CONF buffer, in TRM Table 30.5-11 order
//(same as ESP_IDF spi_ll_init_conf_buffer()).  Every segment reloads the same frame setup.
static void conf_snapshot(uint32_t *c, bool last) {
    c[0] = (SEG_MAGIC << 28) | CONF_BITMAP_ALL; //bitmap word, magic in bits 31..28
    c[1] = GPSPI2.addr;
    c[2] = GPSPI2.ctrl.val;
    c[3] = GPSPI2.clock.val;
    typeof(GPSPI2.user) u = {
        .val = GPSPI2.user.val
    };
    u.usr_conf_nxt = last? 0:1;
    c[4] = u.val;
    c[5] = GPSPI2.user1.val;
    c[6] = GPSPI2.user2.val;
    c[7] = GPSPI2.ms_dlen.val;
    c[8] = GPSPI2.misc.val;
    c[9] = GPSPI2.din_mode.val;
    c[10] = GPSPI2.din_num.val;
    c[11] = GPSPI2.dout_mode.val;
    c[12] = GPSPI2.dma_conf.val;
    c[13] = GPSPI2.dma_int_ena.val;
    c[14] = 0; // DMA_INT_CLR: clear nothing;
}

esp_err_t sampler_init(size_t max_n) {
    if(s_max) return ESP_ERR_INVALID_STATE; //once only
    if(max_n == 0) return ESP_ERR_INVALID_ARG;

     // 1. Descriptors and samples in DMA-capable internal RAM (TRM p.360: linked lists
    //    must be in internal RAM). 3200 samples: 38.4 KB + 38.4 KB + 12.8 KB.
    s_tx = heap_caps_calloc(max_n, sizeof (*s_tx), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_rx = heap_caps_calloc(max_n, sizeof(*s_rx), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_raw = heap_caps_calloc(max_n, sizeof(*s_raw), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if(!s_tx || !s_rx || !s_raw) return ESP_ERR_NO_MEM;

    //2. LOOKUP: a TX/RX DGMA pair routed to SPI2, allocated the way ESP-IDF's spi_common.c
    //does it (GDMA_PERI_SEL = 0 is SPI2, TRM p.361). everytihng elese is register code
    gdma_channel_alloc_config_t tx_cfg = {
        .direction = GDMA_CHANNEL_DIRECTION_TX,
        .flags.reserve_sibling = 1,
    };
    esp_err_t e = gdma_new_ahb_channel(&tx_cfg,&s_dma_tx);
    if( e != ESP_OK) return e;
    gdma_channel_alloc_config_t rx_cfg = {
        .direction = GDMA_CHANNEL_DIRECTION_RX,
        .sibling_chan = s_dma_tx,
    };
    e=gdma_new_ahb_channel(&rx_cfg, &s_dma_rx);
    if(e != ESP_OK) return e;
    e = gdma_connect(s_dma_tx, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_SPI, 2));
    if(e != ESP_OK) return e;
    e = gdma_connect(s_dma_rx, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_SPI, 2));
    if(e != ESP_OK) return e;

    //3. GP-SPI2 into DMA-controlled segmented transfer (TRM p.1132 to 1133, steps 11 to 13)
    GPSPI2.dma_conf.dma_rx_ena = 1; // received word go t othe GDMA, not W0
    GPSPI2.dma_conf.dma_tx_ena = 1; // CONF buffers arrive through the TX GDMA
    GPSPI2.dma_conf.rx_eof_en = 0; //step 11: EOF on seg_trans_done
    GPSPI2.dma_conf.tx_seg_trans_clr_en = 1; // as ESP-IDf spi_ll_master_init()
    GPSPI2.dma_conf.rx_seg_trans_clr_en = 1;
    GPSPI2.slave.dma_seg_magic_value = SEG_MAGIC; // TRM p.1133
    GPSPI2.slave.usr_conf = 1; // step 12: CONF state before each segment
    GPSPI2.dma_int_ena.val = 0;
    GPSPI2.dma_int_ena.dma_seg_trans_done = 1; //step 13: one interrupt per burst
    GPSPI2.dma_int_ena.seg_magic_err = 1;
    GPSPI2.dma_int_clr.val = ~0u;

    //4. CONF buffers: snapshopts of the registers as they are now
    conf_snapshot(s_conf_mid, false);
    conf_snapshot(s_conf_last, true);

    e = esp_intr_alloc(ETS_SPI2_INTR_SOURCE, ESP_INTR_FLAG_IRAM, seg_done_isr, NULL, NULL);
    if (e != ESP_OK) return e;
    s_max = max_n;
    return ESP_OK;
}
    //Link n segments (TRM p.1132, steps 7 to 9): TX = n CONF descriptors, RX = n 4-byte ones.
static void build_lists(size_t n) {
        for (size_t i=0; i<n; i++) {
            bool last = (i == (n-1));
            s_tx[i] = (seg_desc_t) {
                .size = sizeof(s_conf_mid),
                .length = sizeof(s_conf_mid),
                .suc_eof = 1,
                .owner = 1,
                .buf = last? s_conf_last: s_conf_mid,
                .next = last? NULL: &s_tx[i+1],
            };
            s_rx[i] = (seg_desc_t) {
                .size = 4,
                .length = 0,
                .suc_eof = 0,
                .owner = 1,
                .buf = &s_raw[i],
                .next = last? NULL : &s_rx[i+1],
            };
        }
    }

esp_err_t sampler_capture(uint16_t *ch1, uint16_t *ch2, size_t n, uint32_t rate_hz, sampler_info_t *info) {
        if(s_max == 0) return ESP_ERR_INVALID_STATE;
        if(n == 0 || n >s_max || rate_hz == 0) return ESP_ERR_INVALID_ARG;

        //Sample period = CS low + CS high,  CS high = (conf_bitlen +5) APB cycles (TRM p.1134).
        uint32_t period = APB_HZ / rate_hz;
        uint32_t min_period = SEG_LOW_APB + SEG_MIN_GAP + 5u;
        uint32_t max_period = SEG_LOW_APB + CONF_BITLEN_MAX + 5u; //about 305 Sa/s
        if(period < min_period) period = min_period;
        if(period > max_period) period = max_period;
        GPSPI2.cmd.conf_bitlen = period - SEG_LOW_APB - 5u;

        build_lists(n);

        //RX first, then TX, as ESP_IDF's s_sct_load_dma_link().  FIFO resets are TRM step 15
        gdma_reset(s_dma_rx);
        GPSPI2.dma_conf.rx_afifo_rst = 1;
        GPSPI2.dma_conf.rx_afifo_rst = 0;
        GPSPI2.dma_conf.buf_afifo_rst = 1;
        GPSPI2.dma_conf.buf_afifo_rst = 0;
        gdma_start(s_dma_rx, (intptr_t)s_rx);
        gdma_reset(s_dma_tx);
        GPSPI2.dma_conf.dma_afifo_rst = 1;
        GPSPI2.dma_conf.dma_afifo_rst = 0;
        gdma_start(s_dma_tx, (intptr_t)s_tx);

        //Go (TRM step 16).  from here the hardware runs n segments on its own
        ulTaskNotifyTake(pdTRUE,0); // drop a late notification from a timeout burst
        s_int_st = 0;
        s_waiter = xTaskGetCurrentTaskHandle();
        GPSPI2.dma_int_clr.val = ~0u;
        GPSPI2.cmd.update = 1;
        while(GPSPI2.cmd.update) {}
        GPSPI2.cmd.usr = 1;

        //Sleep until the ISR says the burst ended (TRM step 17) Timeout:: 2x the burst + 10ms
        uint32_t burst_ms = (uint32_t) ( (uint64_t)n * period * 1000u / APB_HZ);
        uint32_t got = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2 * burst_ms + 10));
        s_waiter = NULL;
        if(!got) {
            gdma_stop(s_dma_tx);
            gdma_stop(s_dma_rx);
            return ESP_ERR_TIMEOUT;
        }
        typeof(GPSPI2.dma_int_st) st = {
            .val = s_int_st
        };
        if(st.seg_magic_err) return ESP_ERR_INVALID_STATE;

        for(size_t i=0; i<n; i++) ads7883_split(s_raw[i], &ch1[i], &ch2[i]);
        if(info) {
            info->period_apb = period;
            info->rate_hz = APB_HZ / period;
            info->n = n;
        }
        return ESP_OK;
}
