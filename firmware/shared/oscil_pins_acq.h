#pragma once
// Board 1, acquisition. Source of truth: hardware/docs/pin-map.md
// Plain integers so host tests can include this without ESP-IDF.
// Comments give the Lonely Binary header pin.

// ADS7883 channel 1, SPI2 through IO_MUX
#define ACQ_CH1_SCLK      12  // header 18
#define ACQ_CH1_SDO       13  // header 19
#define ACQ_CH1_CS        10  // header 16, 10k pull-up

// ADS7883 channel 2, SPI3 through the GPIO matrix
#define ACQ_CH2_SCLK      15  // header 8
#define ACQ_CH2_SDO       16  // header 9
#define ACQ_CH2_CS        17  // header 10, 10k pull-up

// Encoders, active low, internal pull-ups
#define ACQ_ENC1_A         4  // header 4
#define ACQ_ENC1_B         5  // header 5
#define ACQ_ENC1_SW        6  // header 6
#define ACQ_ENC2_A         7  // header 7
#define ACQ_ENC2_B         8  // header 12
#define ACQ_ENC2_SW        9  // header 15
#define ACQ_ENC3_A        18  // header 11
#define ACQ_ENC3_B        21  // header 27
#define ACQ_ENC3_SW       38  // header 35

// Buttons, active low, internal pull-ups
#define ACQ_BTN_RUN       39  // header 36
#define ACQ_BTN_SINGLE    40  // header 37
#define ACQ_BTN_GEN       41  // header 38

// Panel LEDs, active high, 330R each
#define ACQ_LED_RUN       42  // header 39
#define ACQ_LED_TRIG      47  // header 28
#define ACQ_LED_ARM        1  // header 41
#define ACQ_STATUS_RGB    48  // header 29, onboard WS2812

// LINK1 to board 2, UART1 (console is on USB Serial/JTAG)
#define ACQ_LINK1_TX      43  // header 43
#define ACQ_LINK1_RX      44  // header 42