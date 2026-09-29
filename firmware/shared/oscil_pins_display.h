#pragma once
// Board 2, display and hub. Source of truth: hardware/docs/pin-map.md
// All 27 usable GPIOs are taken. There is no spare.

// RGB565 bus to the 7" panel, top 5/6/5 bits of each colour
#define DISP_LCD_R3        1  // header 41
#define DISP_LCD_R4        2  // header 40
#define DISP_LCD_R5        4  // header 4
#define DISP_LCD_R6        5  // header 5
#define DISP_LCD_R7        6  // header 6
#define DISP_LCD_G2        8  // header 12
#define DISP_LCD_G3        9  // header 15
#define DISP_LCD_G4       10  // header 16
#define DISP_LCD_G5       11  // header 17
#define DISP_LCD_G6       12  // header 18
#define DISP_LCD_G7       13  // header 19
#define DISP_LCD_B3        7  // header 7
#define DISP_LCD_B4       15  // header 8
#define DISP_LCD_B5       16  // header 9
#define DISP_LCD_B6       17  // header 10
#define DISP_LCD_B7       18  // header 11
#define DISP_LCD_DCLK     14  // header 20
#define DISP_LCD_HSYNC    21  // header 27
#define DISP_LCD_VSYNC    38  // header 35
#define DISP_LCD_DE       39  // header 36

// GT911 touch, I2C with 4.7k pull-ups
#define DISP_TOUCH_SDA    40  // header 37
#define DISP_TOUCH_SCL    41  // header 38
#define DISP_TOUCH_INT    42  // header 39, driven at reset to pick the address
#define DISP_TOUCH_RESET  48  // header 29, shares the onboard WS2812

// Links, UART1 to board 1, TX-only UART to board 3
#define DISP_LINK1_TX     43  // header 43
#define DISP_LINK1_RX     44  // header 42
#define DISP_LINK2_TX     47  // header 28