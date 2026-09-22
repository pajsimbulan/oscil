#pragma once
// Board 3, generator. Source of truth: hardware/docs/pin-map.md
// D0-D7 are contiguous in GPIO_OUT so all eight bits change in one write.

#define GEN_DAC_D0         4  // header 4, LSB
#define GEN_DAC_D1         5  // header 5
#define GEN_DAC_D2         6  // header 6
#define GEN_DAC_D3         7  // header 7
#define GEN_DAC_D4         8  // header 12
#define GEN_DAC_D5         9  // header 15
#define GEN_DAC_D6        10  // header 16
#define GEN_DAC_D7        11  // header 17, MSB
#define GEN_DAC_SHIFT      4  // D0's GPIO number
#define GEN_DAC_MASK   (0xFFu << GEN_DAC_SHIFT)

// LINK2 from board 2, receive only
#define GEN_LINK2_RX      44  // header 42
#define GEN_STATUS_RGB    48  // header 29, onboard WS2812