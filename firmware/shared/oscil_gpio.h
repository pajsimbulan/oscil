#pragma once
// Bare-metal GPIO for the ESP32-S3.
// Source: TRM ch.6 (datasheets/esp32-s3_technical_reference_manual_en.pdf p.472-525).
// Same sequences as ESP-IDF v5.5 hal/esp32s3/include/hal/gpio_ll.h.
#include <stdint.h>
#include <stdbool.h>
#include "soc/soc.h"            // REG_SET_BIT, REG_CLR_BIT, REG_SET_FIELD
#include "soc/gpio_struct.h"    // GPIO: the GPIO register block (like GPIOF-> on the TM4C)
#include "soc/io_mux_reg.h"     // IO_MUX_GPIO0_REG, FUN_IE, FUN_PU, FUN_PD, MCU_SEL
#include "soc/gpio_sig_map.h"   // SIG_GPIO_OUT_IDX (256)

#define OSCIL_GPIO_INLINE static inline __attribute__((always_inline))

// Each pin has one 32-bit IO MUX register, 4 bytes apart (TRM p.513).
#define OSCIL_IOMUX_REG(pin)  (IO_MUX_GPIO0_REG + 4u * (uint32_t)(pin))

OSCIL_GPIO_INLINE void oscil_gpio_set(int pin)          // drive high
{
    if (pin < 32) GPIO.out_w1ts = 1u << pin;
    else          GPIO.out1_w1ts.val = 1u << (pin - 32);
}

OSCIL_GPIO_INLINE void oscil_gpio_clr(int pin)          // drive low
{
    if (pin < 32) GPIO.out_w1tc = 1u << pin;
    else          GPIO.out1_w1tc.val = 1u << (pin - 32);
}

OSCIL_GPIO_INLINE bool oscil_gpio_read(int pin)         // pad level, 0 or 1
{
    if (pin < 32) return (GPIO.in >> pin) & 1u;
    else          return (GPIO.in1.val >> (pin - 32)) & 1u;
}

OSCIL_GPIO_INLINE void oscil_gpio_output(int pin)       // push-pull output, starts low
{
    oscil_gpio_clr(pin);                                 // 1. output latch = 0 before we drive
    GPIO.pin[pin].pad_driver = 0;                        // 2. push-pull, not open-drain
    GPIO.func_out_sel_cfg[pin].val = SIG_GPIO_OUT_IDX;   // 3. matrix: plain GPIO (256)
    REG_SET_FIELD(OSCIL_IOMUX_REG(pin), MCU_SEL, 1);     // 4. pad function 1 = GPIO
    REG_CLR_BIT(OSCIL_IOMUX_REG(pin), FUN_PU | FUN_PD);  // 5. no pulls on an output
    if (pin < 32) GPIO.enable_w1ts = 1u << pin;          // 6. turn the driver on
    else          GPIO.enable1_w1ts.val = 1u << (pin - 32);
}

OSCIL_GPIO_INLINE void oscil_gpio_input(int pin, bool pullup)   // input, optional pull-up
{
    if (pin < 32) GPIO.enable_w1tc = 1u << pin;          // 1. stop driving first
    else          GPIO.enable1_w1tc.val = 1u << (pin - 32);
    REG_SET_FIELD(OSCIL_IOMUX_REG(pin), MCU_SEL, 1);     // 2. pad function 1 = GPIO
    REG_SET_BIT(OSCIL_IOMUX_REG(pin), FUN_IE);           // 3. input buffer on
    REG_CLR_BIT(OSCIL_IOMUX_REG(pin), FUN_PD);
    if (pullup) REG_SET_BIT(OSCIL_IOMUX_REG(pin), FUN_PU);   // 4. 45k pull-up
    else        REG_CLR_BIT(OSCIL_IOMUX_REG(pin), FUN_PU);
}