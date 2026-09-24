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

## 2026-09-23

Wrote the SPI side for the ADCs: GP-SPI2 set up straight from its
registers, no driver. Clock on, pins on the IO MUX, 80 MHz module clock
divided down, mode 1, 16 clocks per frame, single or dual line. The
fastest legal clock is 26.67 MHz (80 MHz / 3), since /2 would be 40 and
the ADS7883 tops out at 32 at 3.3 V.

Speed test is written too: times 10,000 frames on the cycle counter for
five settings and holds each for 2 s so the analyzer can catch it.
Nothing plugged in yet; the analyzer run is next.

## 2026-09-24

Ran the speed test on board 1. JTAG flashing kept failing in OpenOCD,
so flashed over UART on COM7 instead.

First run tripped the task watchdog: the 2 s hold loop spun and IDLE0
never ran. It yields every 500 frames now. The result lines also went
missing until I added a 10 ms delay after unmasking interrupts, plus
fflush.

| SCLK | Mode | Lines | Cycles/frame | ns/frame | kSa/s |
|---|---|---|---|---|---|
| 10 MHz | 1 | single | 525 | 2188 | 457 |
| 10 MHz | 1 | dual | 525 | 2188 | 457 |
| 26.67 MHz | 0 | single | 261 | 1088 | 919 |
| 26.67 MHz | 1 | single | 261 | 1088 | 919 |
| 26.67 MHz | 1 | dual | 261 | 1088 | 919 |

At 26.67 MHz the 16 clocks take 600 ns, so about 490 ns of each frame
is overhead from the CPU starting each frame. DMA fixes that later.
Dual line costs nothing, so it's Plan B: one SCLK and CS for both ADCs,
CH1 on GPIO13, CH2 on GPIO11.

![Speed test output on the monitor](screenshots_videos/spi_speed_test_terminal.png)

Analyzer on board 1, 24 MHz sample rate:

| Analyzer | GPIO | Signal |
|---|---|---|
| D0 | 12 | SCLK |
| D1 | 10 | CS |

![Analyzer wired to board 1](screenshots_videos/spi_speed_test_connections.JPG)

![Five frames back to back](screenshots_videos/spi_peed_test_logic_analyzer_5_frames.png)

10 MHz: CS period 2208 ns, matches the 2188 from the cycle counter.
16 clocks per frame.

![10 MHz block, 16 clocks inside one CS low](screenshots_videos/spi_speed_test_logic_analyzer_10_mhz.png)

26.67 MHz: CS period 1084 ns, matches 1088. The clock itself aliases at
24 MHz sampling, so the CS period is the proof here, not the edges.

![26.67 MHz block, CS period](screenshots_videos/spi_speed_test_logic_analyzer_26_67_mhz.png)
