# Firmware

One ESP-IDF project per board. Each builds on its own.

| Board | Folder | Job |
|---|---|---|
| 1 | [`acq/`](acq/) | Two ADS7883s over SPI, trigger, front panel, LINK1 |
| 2 | [`display/`](display/) | 7" RGB panel, touch, UI, Wi-Fi, OTA, LINK1 and LINK2 |
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
| 23 | 8-bit R-2R ladder, all eight bits in one GPIO_OUT store: every bit weight within 1% | Done |
| 24 | DDS in a 100 kHz register-level timer interrupt: sine, square, saw, triangle, host-tested | Done |
| 25 | LVGL 9.3 on the panel: double framebuffer, bounce buffers, rendering on core 1 | Done |
| 26 | GT911 touch over I2C at 0x5D, feeding LVGL as an input device | Done |
| 27 | Live scope view: board 1 frames drawn on board 2, link 31.8 frames/s error-free under display load | In progress (8 fps, trace cleanup) |
| 28 | Measurements on board 1 from the full record: min, max, pp, avg, true RMS, frequency, duty, host-tested | Done |
| 29 | Controls: knobs, buttons and touch through one action path, board 2 owns the settings, RUN/STOP/SINGLE | Done |
| 30 | Generator screen: shapes, keypad frequency, amplitude, offset, duty, live preview from the same DDS code | Done |
| 31 | LINK2: board 2 broadcasts the generator state on change and every 500 ms, board 3 applies it, no acknowledgement needed | Done |
| 32 | Settings saved to NVS with debounce and CRC; two-point calibration per channel, stored on board 1 and sent to board 2 | Done |
| 33 | 16 MB partition table on all boards: two 4 MB OTA slots, 7.9 MB storage, app rollback on | Done |
| 34 | Wi-Fi station on board 2: event-driven, exponential backoff reconnect, SNTP, credentials kept out of git | Done |
| 35 | SYS panel and HTTPS OTA: one-time NVS request, reboot without LCD for download, new-image self-test | In progress (0.8.0 to 0.8.1 passed; rollback tests pending) |
| 36 | Supabase HTTPS client: sign-in, session saved in NVS, refresh after reboot, wrong-password rejection | Done |

---

## Updates

Board 2 updates over Wi-Fi from SYS. The release asset is
`oscil_display.bin`, built as `display/build/display.bin`; `OTA_URL`
in the ignored `display/main/secrets.h` points to it. Upload the new
binary to the latest GitHub Release before pressing Update.

Update saves a one-time request to NVS and reboots. The next boot
downloads with the LCD, touch and UI disabled, then boots the new
image. The screen is blank during the download. A failed attempt
returns to normal startup instead of retrying on every boot.

Keep board 1 running: a new image confirms itself only after the LCD
and touch start and a recent acquisition frame arrives. Otherwise it
rolls back. The 0.8.0 to 0.8.1 update and confirmation passed; deliberate
self-test failure and missing-LINK1 rollback tests are still pending.

[Update mode and download log](docs/screenshots_videos/ota_update_mode_download.png),
[0.8.1 confirmed with LINK1 clean](docs/screenshots_videos/ota_0_8_1_confirmed_link_clean.png),
and [video: update from the touchscreen](docs/screenshots_videos/ota_update_0_8_0_to_0_8_1.MP4).

---

## Accounts

Board 2 signs in through Supabase over HTTPS. The refresh token and user
identity are saved in NVS; the access token stays in RAM. After reboot,
the saved refresh token obtains a new access token without the password.

Device tests passed for sign-in, session recovery after reset and rejection
of a wrong password. The touchscreen account screen is still to add.
Backend setup and isolation tests live in [`software/`](../software/).

[Sign-in](docs/screenshots_videos/account_sign_in_success.png),
[session recovery](docs/screenshots_videos/account_session_restored.png),
and [wrong-password rejection](docs/screenshots_videos/account_wrong_password_rejected.png).

---

## Build log

Everything, including the mistakes, with photos and analyzer captures: [docs/build_log.md](docs/build_log.md)