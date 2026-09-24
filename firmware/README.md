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

| Step | What | Status |
|---|---|---|
| 00 | Three projects, shared component, pin headers, sdkconfig defaults | Done |
| 01 | Host tests for the AFE math, CI on every push | Done |
| 02 | All three boards flash and report 16 MB flash, 8 MB PSRAM, 1 kHz tick | Done |
| 03 | Register-level GPIO, bit-banged WS2812 heartbeat on all three | Done |
| 04 | Pin walk: every GPIO checked against the schematic on the analyzer | Done |
| 05 | SPI speed test: GP-SPI2 from registers, frame rate measured | In progress |

---

## Build log

Every step, including the mistakes, with photos and analyzer captures: [docs/build_log.md](docs/build_log.md)