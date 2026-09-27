#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "soc/spi_struct.h" //GPSPI2: the GP-SPI2 register block
#include "soc/spi_reg.h" //SPI_USR

//SCLCK = 80/Mhz / div.  div 3 = 26.67 MHz (fastest legal for the ADS7883 part),  div 8 = 10 MHz.
//spi2 is the one with dma
void spi2_adc_init(int div, int mode, bool dual);

//one frame: CS low, 16 SCLKs, CS high, returns the raw recikeved word if yo look at ADS7883 datasheet
static inline __attribute__((always_inline)) uint32_t spi2_adc_frame(void)  {
    GPSPI2.cmd.val = SPI_USR; //start. CS falls here: this is the sampling instant
    while(GPSPI2.cmd.usr) {} // hardware clears USR wghen CS has risen again
    return GPSPI2.data_buf[0]; //W0: first byte recieved is in bits 7..0
}

// SLAS594 p.9:  two leading zeros,  and then data  D11..D0  two trailing zeros, MSB first
static inline uint16_t ads7883_decode(uint16_t word) {
    return (word >> 2) & 0x0FFF;
}


// Single-line frame: W0 holds the first byte received in bits 7..0, the second in 15..8.
static inline uint16_t spi2_adc_single_code(uint32_t raw) {
    uint16_t word = (uint16_t)(((raw & 0xFFu) << 8) | ((raw >> 8) & 0xFFu)); // swap bytes
    return ads7883_decode(word);
}