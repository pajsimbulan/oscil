# Firmware build log

## 2026-09-21

Started firmware. Three ESP-IDF projects, one per board, plus a shared
folder so pin numbers and the link protocol only exist once.

## 2026-09-22

sdkconfig.defaults in each: 16MB flash, octal PSRAM, 1000 Hz tick,
console on USB Serial/JTAG. The 2MB and 100 Hz defaults both bit me
in earlier lab work, so each build gets checked against sdkconfig.

Pin numbers from pin-map.md are in a header per board. Firmware
never uses a raw GPIO number.

AFE scaling in shared. The divider bottom sits on 2.2 V instead of
ground, so 0 V at the BNC reads 1.65 V at the ADC. Vbnc = 4 x (Vadc - 1.65).

First build couldn't find oscil_pins_acq.h. An include path would've
found the header but not compiled the .c, so shared is a real component
now. Also caught display and gen still named project(acq).

Host tests with Unity run the real oscil_afe.c on my PC. 4/4 pass.
They check the math, not the hardware. CI runs them on every push
that touches firmware, and the badge on the README shows the result.

Status LED heartbeat on all three boards. GPIO goes through registers
in oscil_gpio.h, no driver. The WS2812 is bit-banged off the CPU cycle
counter: 24 bits GRB, 0.4 or 0.85 us high in a 1.25 us bit, interrupts
masked for the ~30 us frame. Blink task pinned to core 0.
Acq green, display red, gen blue.

First build died on -Werror=misleading-indentation. A `while (...);`
wait loop with the next line one space off. Empty loops get `{ }` now.
First flash wasn't one either: only the monitor ran, and board 1 was
still booting an old lab.

![Before: three projects, one command each](screenshots_videos/heartbeat_test_before.png)

![After: all three boot, heartbeat on GPIO48](screenshots_videos/heartbeat_test_after_success.png)

[![Three boards blinking, click for video](screenshots_videos/heartbeat_3_mcus.JPG)](screenshots_videos/heartbeat_3_mcus.MP4)

Pin walk on board 1. The first pin in the list pulses once, the second
twice, and so on, then every pin is read back with the pull-up on.
26 pins, four batches of eight on the logic analyzer, every count
matched the schematic header. GPIO43 and 44 are the TX and RX pads
on the silkscreen, not numbered.

First read pass came back with random 0s. I was reading right after
turning on the 45k pull-up, and the pin had just been driven low, so it
hadn't charged yet. Pull-ups on for everything, 10 ms settle, then read:
all 1s every pass.

Each batch: analyzer GND to header 22, D0 to D7 on eight header pins,
1 MHz capture. Counting the pulses on a channel tells you which GPIO
is on that pin.

![Board 1 wired to the analyzer, batch 1](screenshots_videos/pin_walk_mcu1_batch1.JPG)

Batch 1:

| Ch | Header | GPIO | Signal | Pulses | Seen |
|---|---|---|---|---|---|
| D0 | 4 | 4 | ENC1_A | 1 | ✓ |
| D1 | 5 | 5 | ENC1_B | 2 | ✓ |
| D2 | 6 | 6 | ENC1_SW | 3 | ✓ |
| D3 | 7 | 7 | ENC2_A | 4 | ✓ |
| D4 | 12 | 8 | ENC2_B | 5 | ✓ |
| D5 | 15 | 9 | ENC2_SW | 6 | ✓ |
| D6 | 8 | 15 | CH2_SCLK | 7 | ✓ |
| D7 | 9 | 16 | CH2_SDO | 8 | ✓ |

![Batch 1: 1 to 8 pulses](screenshots_videos/pin_walk_board1_batch1_logic_analyzer.png)

Batch 2:

| Ch | Header | GPIO | Signal | Pulses | Seen |
|---|---|---|---|---|---|
| D0 | 10 | 17 | CH2_CS | 9 | ✓ |
| D1 | 16 | 10 | CH1_CS | 10 | ✓ |
| D2 | 11 | 18 | ENC3_A | 11 | ✓ |
| D3 | 18 | 12 | CH1_SCLK | 12 | ✓ |
| D4 | 19 | 13 | CH1_SDO | 13 | ✓ |
| D5 | 27 | 21 | ENC3_B | 14 | ✓ |
| D6 | 35 | 38 | ENC3_SW | 15 | ✓ |
| D7 | 36 | 39 | BTN_RUN | 16 | ✓ |

![Batch 2: 9 to 16 pulses](screenshots_videos/pin_walk_board1_batch2_logic_analyzer.png)

Batch 3:

| Ch | Header | GPIO | Signal | Pulses | Seen |
|---|---|---|---|---|---|
| D0 | 37 | 40 | BTN_SINGLE | 17 | ✓ |
| D1 | 38 | 41 | BTN_GEN | 18 | ✓ |
| D2 | 39 | 42 | LED_RUN | 19 | ✓ |
| D3 | 28 | 47 | LED_TRIG | 20 | ✓ |
| D4 | 41 | 1 | LED_ARM | 21 | ✓ |
| D5 | 43 (TX) | 43 | LINK1_TX | 22 | ✓ |
| D6 | 42 (RX) | 44 | LINK1_RX | 23 | ✓ |
| D7 | 40 | 2 | spare | 24 | ✓ |

![Batch 3: 17 to 24 pulses, TX and RX included](screenshots_videos/pin_walk_board1_batch3_logic_analyzer.png)

Batch 4:

| Ch | Header | GPIO | Signal | Pulses | Seen |
|---|---|---|---|---|---|
| D0 | 17 | 11 | spare | 25 | ✓ |
| D1 | 20 | 14 | spare | 26 | ✓ |

![Batch 4: the two spares, 25 and 26](screenshots_videos/pin_walk_board1_batch4_logic_analyzer.png)

Pin walk on board 2. Same 26-pin idea, same header order as board 1, so
the analyzer wiring for each batch didn't change between boards. Every
count matched. GPIO48 stays off the list: it's the status LED now and
the touch reset later, so it gets checked when the touch panel goes on.

Batch 1:

| Ch | Header | GPIO | Signal | Pulses | Seen |
|---|---|---|---|---|---|
| D0 | 4 | 4 | LCD_R5 | 1 | ✓ |
| D1 | 5 | 5 | LCD_R6 | 2 | ✓ |
| D2 | 6 | 6 | LCD_R7 | 3 | ✓ |
| D3 | 7 | 7 | LCD_G2 | 4 | ✓ |
| D4 | 12 | 8 | LCD_G3 | 5 | ✓ |
| D5 | 15 | 9 | LCD_G4 | 6 | ✓ |
| D6 | 8 | 15 | LCD_B5 | 7 | ✓ |
| D7 | 9 | 16 | LCD_B6 | 8 | ✓ |

![Board 2 batch 1: 1 to 8 pulses, red and green data lines](screenshots_videos/pin_walk_board2_batch1_logic_analyzer.png)

Batch 2:

| Ch | Header | GPIO | Signal | Pulses | Seen |
|---|---|---|---|---|---|
| D0 | 10 | 17 | LCD_B7 | 9 | ✓ |
| D1 | 16 | 10 | LCD_G5 | 10 | ✓ |
| D2 | 11 | 18 | LCD_DCLK | 11 | ✓ |
| D3 | 18 | 12 | LCD_G7 | 12 | ✓ |
| D4 | 19 | 13 | LCD_B3 | 13 | ✓ |
| D5 | 27 | 21 | LCD_HSYNC | 14 | ✓ |
| D6 | 35 | 38 | LCD_VSYNC | 15 | ✓ |
| D7 | 36 | 39 | LCD_DE | 16 | ✓ |

![Board 2 batch 2: 9 to 16 pulses, the rest of the RGB bus and its sync lines](screenshots_videos/pin_walk_board2_batch2_logic_analyzer.png)

Batch 3:

| Ch | Header | GPIO | Signal | Pulses | Seen |
|---|---|---|---|---|---|
| D0 | 37 | 40 | TOUCH_SDA | 17 | ✓ |
| D1 | 38 | 41 | TOUCH_SCL | 18 | ✓ |
| D2 | 39 | 42 | TOUCH_INT | 19 | ✓ |
| D3 | 28 | 47 | LINK2_TX | 20 | ✓ |
| D4 | 41 | 1 | LCD_R3 | 21 | ✓ |
| D5 | 43 (TX) | 43 | LINK1_TX | 22 | ✓ |
| D6 | 42 (RX) | 44 | LINK1_RX | 23 | ✓ |
| D7 | 40 | 2 | LCD_R4 | 24 | ✓ |

![Board 2 batch 3: 17 to 24 pulses, touch, both links, two red lines](screenshots_videos/pin_walk_board2_batch3_logic_analyzer.png)

Batch 4:

| Ch | Header | GPIO | Signal | Pulses | Seen |
|---|---|---|---|---|---|
| D0 | 17 | 11 | LCD_G6 | 25 | ✓ |
| D1 | 20 | 14 | LCD_B4 | 26 | ✓ |

![Board 2 batch 4: 25 and 26](screenshots_videos/pin_walk_board2_batch4_logic_analyzer.png)

Pin walk on board 3. Only nine pins: the eight R-2R bits and LINK2's
receive line. The DAC bits are the ones that have to be exact, since
all eight get written in one store to GPIO_OUT later. All eight matched,
and LINK2_RX on the RX pad showed its 9 pulses. Step 04 done on all
three boards, 61 pins, no mismatches.

| Ch | Header | GPIO | Signal | Pulses | Seen |
|---|---|---|---|---|---|
| D0 | 4 | 4 | DAC_D0 | 1 | ✓ |
| D1 | 5 | 5 | DAC_D1 | 2 | ✓ |
| D2 | 6 | 6 | DAC_D2 | 3 | ✓ |
| D3 | 7 | 7 | DAC_D3 | 4 | ✓ |
| D4 | 12 | 8 | DAC_D4 | 5 | ✓ |
| D5 | 15 | 9 | DAC_D5 | 6 | ✓ |
| D6 | 16 | 10 | DAC_D6 | 7 | ✓ |
| D7 | 17 | 11 | DAC_D7 | 8 | ✓ |
| D0 (batch 2) | 42 (RX) | 44 | LINK2_RX | 9 | ✓ |

![Board 3 batch 1: the eight DAC bits, 1 to 8 pulses](screenshots_videos/pin_walk_board3_batch1_logic_analyzer.png)

![Board 3 batch 2: LINK2_RX, 9 pulses](screenshots_videos/pin_walk_board3_batch2_logic_analyzer.png)
