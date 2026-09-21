# Build log

## 2026-09-16

Created the KiCad project. Pushed the design docs and requirements.

## 2026-09-18

Six hierarchical sheets on the root page: power, both AFE channels,
acquisition, display, generator. 2000x1500 mils each, two rows of three.
Empty so far.

Added hardware/datasheets/ — PDFs untracked, README of links instead.
Two of the links I had were dead.

Made the project symbol library and drew the ADS7883. Pin numbers checked
against SLAS594 p.5.

Took me a while to work out that the pin X/Y is where the pin meets the
body, not the end of the stub.

## 2026-09-19

Drew the dev board symbol. 44 pins, numbered by header position with the
board in front of me.

Didn't use KiCad's built-in WROOM-1 symbol,  that's the bare module and the
header order is completely different.

GPIO35-37 are broken out on the header but the octal PSRAM uses them.
Noted on the symbol.

Drew the LCD 40-pin symbol. Was working off the 5.0 inch datasheet at first
and caught it, wrong touch controller, wrong backlight, and three pins the
7.0 inch part actually uses are NC on the 5.0.

Renamed pins 35 and 36 from SCL/SDA to SPI_SCK/SPI_MOSI. They're SPI, and
the GT911 touch I2C on the same sheet is already using those names. 

Drew the GT911 touch symbol. Six pins, same datasheet page as the LCD
connector.

## 2026-09-20

Power sheet. USB input, 0R rail split, MCP1700 to +3V3_A.

No ferrite bead in the BOM so fitted 0R instead.

2.2 V reference. 10k/20k off +3V3_A, buffered with half the MCP6292.
Tied off the spare half.

ERC needed a second PWR_FLAG after R1. The resistor splits the net so
the flag on +5V doesn't reach the other side.

VREF_2V2 as a power symbol fails ERC. Made it a global label.

BNC input on ch1. Shell to GND, centre pin is the signal.

Wrote the safety note on the sheet. The shell is system ground, which
is USB ground, which is mains earth. Not isolated.

Divider on ch1. 750k/250k off the BNC, bottom of the stack goes to
VREF_2V2 instead of GND, which is what shifts the signal into the
ADC window.

Trimmers go across each resistor, not to ground. R4*C5 = R5*C6.

BAV99 clamp on the divider junction. Pin 3 is the middle tap, the two
diodes are in series, not a common-cathode pair.

R4 is doing two jobs. It's the attenuator and it's what limits fault
current into the diodes.

Buffer on ch1. MCP6292 as a follower off the divider junction.

The divider is 188k out. The ADS7883 wants under 200 ohm, so the
buffer isn't optional.

Filter and sheet exit on ch1. 150R with two 2.2nF C0G in parallel,
241kHz corner. C0G because X7R shifts with temperature and DC bias.

Tied off U3B as a grounded follower. No-connects would have left the
inputs floating, which on a CMOS part means the output sits on a rail.

One ERC error left, CH1_ANALOG has nowhere to go until the acquisition
sheet exists.

Copied ch1 to ch2. Designators auto-incremented, but the notes carry
part numbers so those had to be retyped.

# Pin map

First pass. Verify against the Lonely Binary header diagram before wiring.

## Board 1, acquisition

| Signal | GPIO |
|---|---|
| CH1_SCLK | 12 |
| CH1_SDO | 13 |
| CH1_CS | 10 |
| CH2_SCLK | 15 |
| CH2_SDO | 16 |
| CH2_CS | 17 |
| ENC1_A | 4 |
| ENC1_B | 5 |
| ENC1_SW | 6 |
| ENC2_A | 7 |
| ENC2_B | 8 |
| ENC2_SW | 9 |
| ENC3_A | 18 |
| ENC3_B | 21 |
| ENC3_SW | 38 |
| BTN_RUN | 39 |
| BTN_SINGLE | 40 |
| BTN_GEN | 41 |
| LED_RUN | 42 |
| LED_TRIG | 47 |
| LED_ARM | 48 |
| LINK1_TX | 43 |
| LINK1_RX | 44 |

Spare: 1, 2, 11, 14

## Board 2, display and hub

| Signal | GPIO |
|---|---|
| LCD_R3 | 1 |
| LCD_R4 | 2 |
| LCD_R5 | 4 |
| LCD_R6 | 5 |
| LCD_R7 | 6 |
| LCD_G2 | 7 |
| LCD_G3 | 8 |
| LCD_G4 | 9 |
| LCD_G5 | 10 |
| LCD_G6 | 11 |
| LCD_G7 | 12 |
| LCD_B3 | 13 |
| LCD_B4 | 14 |
| LCD_B5 | 15 |
| LCD_B6 | 16 |
| LCD_B7 | 17 |
| LCD_DCLK | 18 |
| LCD_HSYNC | 21 |
| LCD_VSYNC | 38 |
| LCD_DE | 39 |
| TOUCH_SDA | 40 |
| TOUCH_SCL | 41 |
| TOUCH_INT | 42 |
| TOUCH_RESET | 47 |
| LINK1_TX | 43 |
| LINK1_RX | 44 |
| LINK2_TX | 48 |

Spare: none

## Board 3, generator

| Signal | GPIO |
|---|---|
| DAC_D0 | 4 |
| DAC_D1 | 5 |
| DAC_D2 | 6 |
| DAC_D3 | 7 |
| DAC_D4 | 8 |
| DAC_D5 | 9 |
| DAC_D6 | 10 |
| DAC_D7 | 11 |
| OUT_EN | 12 |
| LINK2_RX | 44 |

Spare: 1, 2, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42, 43, 47, 48


GPIO48 drives the onboard RGB LED. On board 1 that is the ARM status LED.
On board 2 it carries LINK2_TX, so the LED flickers with generator traffic.

LINK2 is TX only. Board 2 sends generator settings and gets nothing back.
That is what makes board 2 fit in 27 pins.