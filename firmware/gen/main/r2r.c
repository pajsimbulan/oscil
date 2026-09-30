#include "oscil_gpio.h"
#include "r2r.h"

void r2r_init(void)
{
    for (int pin = GEN_DAC_D0; pin <= GEN_DAC_D7; pin++) oscil_gpio_output(pin);
}