#include "spi2_adc.h"
#include "oscil_pins_acq.h"
#include "oscil_gpio.h" //OSCIL_IOMUX_REG
#include "soc/system_struct.h" //SYSTEM: peripheral clock and reset enables
#include "soc/gpio_sig_map.h" //FSPIQ_IN_ID, FSPID_IN_IDX
#include "freertos/FreeRTOS.h"

static portMUX_TYPE s_rcc_lock = portMUX_INITIALIZER_UNLOCKED;

//Pin straight to SPI2 through the IO Mux (function 4), bypassing the GPIO matrix.
//same as ESP-IDF's gpio_hal_iomux_out() / gpio_hal_iomux_in().
static void iomux_out(int pin) {
    GPIO.func_out_sel_cfg[pin].oen_sel = 0;  //output enable comes from SPI2
    REG_SET_FIELD(OSCIL_IOMUX_REG(pin), MCU_SEL, 4); //FSPI function (soc/spi_pins.h: SPI2_FUNC_NUM)
}

static void iomux_in(int pin, int signal) {
    GPIO.func_in_sel_cfg[signal].sig_in_sel = 0; //SPI2 input comes from the IO MUX, not the matrix
    REG_SET_BIT(OSCIL_IOMUX_REG(pin), FUN_IE);
    REG_SET_FIELD(OSCIL_IOMUX_REG(pin), MCU_SEL, 4);
}

void spi2_adc_init(int div, int mode, bool dual) {
    //1. Clock the peripheral and resit it.  TRM p.825
    portENTER_CRITICAL(&s_rcc_lock);
    SYSTEM.perip_clk_en0.spi2_clk_en = 1;
    SYSTEM.perip_rst_en0.spi2_rst = 1;
    SYSTEM.perip_rst_en0.spi2_rst = 0;
    portEXIT_CRITICAL(&s_rcc_lock);

    //2.  Pins.  SCLK GPIO12, CS GPIO10, Q QPIO13, D GPIO11 are SPI2's IO MUX pins (soc/spi_pins.h).
    iomux_out(ACQ_ADC_SCLK);
    iomux_out(ACQ_ADC_CS);
    iomux_out(ACQ_CH1_SDO); //CH1 data on Q
    iomux_in(ACQ_CH1_SDO, FSPIQ_IN_IDX);  //CH1 data on Q
    
    // CH2 data on D, GPIO11 (Plan B)
    if (dual) {
        iomux_out(ACQ_CH2_SDO);
        iomux_in(ACQ_CH2_SDO, FSPID_IN_IDX);
    }

    //3. Module clock on, master, CPI-controlled (no DMA). spi_ll_master_init().
    GPSPI2.clk_gate.clk_en = 1;
    GPSPI2.clk_gate.mst_clk_active = 1;
    GPSPI2.clk_gate.mst_clk_sel = 1; //80 MHz moduel clock
    GPSPI2.slave.val = 0; //master mode
    GPSPI2.dma_conf.val = 0; //DMA off: data lands in W0..W15
    GPSPI2.user.val = 0; //no command, address, dummy or write phase
    GPSPI2.user1.cs_setup_time = 0; //no extra CS setup/hold clocks
    GPSPI2.user1.cs_hold_time = 0; 
    GPSPI2.ctrl.rd_bit_order = 0; //MSB first (ADS7883 sends MSB first)
    GPSPI2.ctrl.fread_quad = 0; //line mode: set below, everying else single
    GPSPI2.ctrl.fread_oct = 0;

    //4. SCLK = 80Mhz / div. TRM p.1142 clkcnt_h = floor ((N+1)/2 - 1), clkcnt_l = N
    typeof(GPSPI2.clock) clk = {.val = 0};
    clk.clkcnt_n = div-1;
    clk.clkcnt_l = div-1;
    clk.clkcnt_h = div/2 -1;
    clk.clkdiv_pre = 0;
    clk.clk_equ_sysclk = 0;
    GPSPI2.clock.val = clk.val; //one store for the whole register

    //5. Mode, phases, length. spi_ll_master_set_mode(), spi_hal_setup_trans().
    GPSPI2.misc.ck_idle_edge = 0; //CPOL 0: SCLK idles low (mode 0 and 1)
    GPSPI2.user.ck_out_edge = (mode == 1); //CPHA: 0 = sample rising, 1 = sample falling
    GPSPI2.user.usr_miso = 1; //the only phase: read
    GPSPI2.user.doutdin = 0; //half duplex
    GPSPI2.ctrl.fread_dual = dual; //read two bits per clock (Q and D)
    GPSPI2.ms_dlen.ms_data_bitlen = (dual? 32:16)-1; //16 SCLKs either way

    //6. Use CSO only.
    GPSPI2.misc.cs0_dis = 0;
    GPSPI2.misc.cs1_dis = 1;
    GPSPI2.misc.cs2_dis = 1;
    GPSPI2.misc.cs3_dis = 1;
    GPSPI2.misc.cs4_dis = 1;
    GPSPI2.misc.cs5_dis = 1;

    //7. Copy the configratuion form the APB clock domain into the SPI clock domain.
    GPSPI2.cmd.update = 1;
    while(GPSPI2.cmd.update) {}
}