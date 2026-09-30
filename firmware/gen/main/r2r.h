#pragma once
#include <stdint.h>
#include "soc/gpio_struct.h"
#include "oscil_pins_gen.h"

// Ownership rule for board 3: GPIO_OUT bank 0 (GPIO0-31) is written only by r2r_write()
// and the DDS timing pin, both inside the DDS interrupt (or before it starts).
// Nothing else on board 3 may drive a GPIO below 32.
static inline __attribute__((always_inline)) void r2r_write(uint8_t code)
{
    GPIO.out = (GPIO.out & ~GEN_DAC_MASK) | ((uint32_t)code << GEN_DAC_SHIFT);   // one store, 8 pins
}

void r2r_init(void);                 // D0-D7 as outputs, all low (code 0)