#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "soc/spi_struct.h" //GPSPI2: the GP-SPI2 register block
#include "soc/spi_reg.h" //SPI_USR
#include "oscil_ads7883.h"  //frame decode helpers (host-testable)

// SPI mode for full speed (26.67 MHz), measured: modes 0 and 1 both clean,
// 1 has positive setup/hold margin on paper
#define ADC_MODE_FULL 1

//SCLCK = 80/Mhz / div.  div 3 = 26.67 MHz (fastest legal for the ADS7883 part),  div 8 = 10 MHz.
//spi2 is the one with dma
void spi2_adc_init(int div, int mode, bool dual);

//one frame: CS low, 16 SCLKs, CS high, returns the raw recikeved word if yo look at ADS7883 datasheet
static inline __attribute__((always_inline)) uint32_t spi2_adc_frame(void)  {
    GPSPI2.cmd.val = SPI_USR; //start. CS falls here: this is the sampling instant
    while(GPSPI2.cmd.usr) {} // hardware clears USR wghen CS has risen again
    return GPSPI2.data_buf[0]; //W0: first byte recieved is in bits 7..0
}

