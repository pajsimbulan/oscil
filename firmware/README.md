# Firmware

One ESP-IDF project per board. Each builds on its own.

| Board | Folder | Job |
|---|---|---|
| 1 | [`acq/`](acq/) | Two ADS7883s over SPI, trigger, front panel, LINK1 |
| 2 | [`display/`](display/) | 7" RGB panel, touch, UI, Wi-Fi, LINK1 and LINK2 |
| 3 | [`gen/`](gen/) | DDS into an 8-bit R-2R ladder, LINK2 |

![firmware](https://github.com/pajsimbulan/oscil/actions/workflows/firmware.yml/badge.svg)

Shared code lives in [`shared/`](shared/).
Pin numbers come from [`hardware/docs/pin-map.md`](../hardware/docs/pin-map.md).

---

## Build

From the ESP-IDF terminal, in a board folder:

    idf.py set-target esp32s3
    idf.py build


---

## Progress

| # | What | Status |
|---|---|---|
| 00 | Three projects, shared component, pin headers, sdkconfig defaults | Done |
| 01 | Host tests for the AFE math, CI on every push | Done |
| 02 | All three boards flash and report 16 MB flash, 8 MB PSRAM, 1 kHz tick | Done |
| 03 | Register-level GPIO, bit-banged WS2812 heartbeat on all three | Done |
| 04 | Pin walk: every GPIO checked against the schematic on the analyzer | Done |
| 05 | SPI speed test: GP-SPI2 from registers, frame rate measured | Done |
| 06 | Analog rail: MCP1700 makes +3V3_A (3.314 V) | Done |
| 07 | 2.2 V reference: divider + U2A buffer, VREF_2V2 = 2.214 V | Done |
| 08 | CH1 and CH2 front ends: offset (1.66 V) and open input (VREF) checked | Done |
| 09 | Front panel: 3 LEDs, 3 buttons debounced by a 1 kHz scan, events on a queue | Done |
| 10 | Encoders: 3 EC11s, table-driven quadrature decoder, one event per click | Done |
| 11 | First ADC reads: both ADS7883s clean at 10 MHz, CH1 calibrated to 0.001 V with IN grounded | Done |
| 12 | Noise floor at 26.67 MHz: 3.9 codes rms (12.6 mV at the BNC), SPI mode chosen from measurement | Done |
| 13 | Both channels from one dual-line SPI frame, bit de-interleave host-tested | Done |
| 14 | DMA burst capture: 3200 samples per burst, rate set by a hardware counter, checked against an on-board 1 kHz reference, up to about 620 kSa/s | Done |
| 15 | Edge trigger with hysteresis and sub-sample position, host-tested | Done |
| 16 | Min/max decimation to 800 display columns, single-sample spikes kept, host-tested | Done |
| 17 | Acquisition task on core 1: capture, trigger, min/max frame, RUN/STOP/SINGLE, 70 frames/s | Done |
| 18 | Board link protocol: COBS framing, CRC-16, resync and loss count, host-tested | Done |
| 19 | UART driver from the registers: 15,600 frames at 2 Mbaud in loopback, zero errors | Done |
| 20 | Board 1 to board 2 over LINK1: 31.8 frames/s of waveform data, panel events, settings round trip | Done |
| 21 | Display backlight from an XL6009 boost: 40 mA at 23 V, set and measured by hand | Done |
| 22 | RGB panel lit from a PSRAM framebuffer at 16 MHz: colour bars, bit bands, border | Done |

---

## Build log

Everything, including the mistakes, with photos and analyzer captures: [docs/build_log.md](docs/build_log.md)