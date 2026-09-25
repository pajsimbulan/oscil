# Pin map

Rev A, matches the schematic. Header pin numbers are for the Lonely Binary
ESP32-S3 N16R8 dev board.

## Board 1, acquisition

| Signal | GPIO | Header pin | Note |
|---|---|---|---|
| CH1_SCLK | 12 | 18 | SPI2 IO_MUX |
| CH1_SDO | 13 | 19 | SPI2 IO_MUX |
| CH1_CS | 10 | 16 | SPI2 IO_MUX, 10k pull-up |
| CH2_SCLK | 15 | 8 | SPI3, GPIO matrix |
| CH2_SDO | 16 | 9 | SPI3, GPIO matrix |
| CH2_CS | 17 | 10 | SPI3, 10k pull-up |
| ENC1_A / B / SW | 4 / 5 / 6 | 4 / 5 / 6 | |
| ENC2_A / B / SW | 7 / 8 / 9 | 7 / 12 / 15 | |
| ENC3_A / B / SW | 18 / 21 / 38 | 11 / 27 / 35 | |
| BTN_RUN / SINGLE / GEN | 39 / 40 / 41 | 36 / 37 / 38 | |
| LED_RUN / TRIG / ARM | 42 / 2 / 1 | 39 / 40 / 41 | 330R each |
| LINK1_TX / RX | 43 / 44 | 43 / 42 | UART1 |

Spare: 11, 14, 47. GPIO48 is the onboard WS2812.

## Board 2, display

| Signal | GPIO | Header pin |
|---|---|---|
| LCD_R3–R7 | 1, 2, 4, 5, 6 | 41, 40, 4, 5, 6 |
| LCD_G2–G7 | 7, 8, 9, 10, 11, 12 | 7, 12, 15, 16, 17, 18 |
| LCD_B3–B7 | 13, 14, 15, 16, 17 | 19, 20, 8, 9, 10 |
| LCD_DCLK / HSYNC / VSYNC / DE | 18 / 21 / 38 / 39 | 11 / 27 / 35 / 36 |
| TOUCH_SDA / SCL / INT | 40 / 41 / 42 | 37 / 38 / 39 |
| TOUCH_RESET | 48 | 29 |
| LINK1_TX / RX | 43 / 44 | 43 / 42 |
| LINK2_TX | 47 | 28 |

Spare: none. All 27 usable GPIOs are taken.

## Board 3, generator

| Signal | GPIO | Header pin |
|---|---|---|
| DAC_D0–D3 | 4, 5, 6, 7 | 4, 5, 6, 7 |
| DAC_D4–D7 | 8, 9, 10, 11 | 12, 15, 16, 17 |
| LINK2_RX | 44 | 42 |

Spare: 1, 2, 12–18, 21, 38–43, 47, 48.

## Reserved on every board

| GPIO | Why |
|---|---|
| 26–32 | SPI flash |
| 33–37 | Octal PSRAM (N16R8) |
| 19–20 | Native USB |
| 0, 3, 45, 46 | Strapping |

## Notes

LINK2 is TX only. Board 2 has no pin left for a receive line, and board 3
has nothing to report back.

GPIO43/44 are UART0's default pins. The links run on UART1 and the console
on USB Serial/JTAG (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`), otherwise the
boot log goes down the link cable.

GPIO39–42 are the external JTAG pins. Using them rules out an external JTAG
probe; the built-in USB JTAG still works.

DAC_D0–D7 sit on GPIO4–11, contiguous in the GPIO_OUT register, so all eight
bits update in one write.