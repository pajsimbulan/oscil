# Firmware

One ESP-IDF project per board. Each builds on its own.

| Board | Folder | Job |
|---|---|---|
| 1 | [`acq/`](acq/) | Two ADS7883s over SPI, trigger, front panel, LINK1 |
| 2 | [`display/`](display/) | 7" RGB panel, touch, UI, Wi-Fi, LINK1 and LINK2 |
| 3 | [`gen/`](gen/) | DDS into an 8-bit R-2R ladder, LINK2 |

Shared code lives in [`shared/`](shared/).
Pin numbers come from [`hardware/docs/pin-map.md`](../hardware/docs/pin-map.md).

---

## Build

From the ESP-IDF terminal, in a board folder:

    idf.py set-target esp32s3
    idf.py build


---

## Build log

Every step, including the mistakes: [docs/build_log.md](docs/build_log.md)