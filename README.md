# Oscil

**Two-channel "smart" oscilloscope and function generator**

ESP32-S3 · ESP-IDF · FreeRTOS · Supabase

![status](https://img.shields.io/badge/status-working%20prototype-green)
![hardware](https://img.shields.io/badge/hardware-Rev%20A-blue)
![platform](https://img.shields.io/badge/platform-ESP32--S3-informational)
![firmware](https://github.com/pajsimbulan/oscil/actions/workflows/firmware.yml/badge.svg)

> **Working prototype on breadboards.** Board 1 samples two channels and
> sends waveform frames to board 2 at 31.8 frames/s with zero link errors.
> Board 2 runs the 7" touchscreen scope, measurements and the generator
> controls; board 3 outputs sine, square, saw and triangle. Board 2 also
> joins Wi-Fi, updates itself over HTTPS with rollback, signs in to a
> Supabase account and saves scope photos to the cloud.

| Folder | What's in it |
|---|---|
| [`hardware/`](hardware/) | Rev A schematic, pin map, build log |
| [`firmware/`](firmware/) | One ESP-IDF project per board, plus shared code and host tests |
| [`software/`](software/) | Supabase schema, account function and isolation tests |

---

## Contents

- [Use cases / functionality](#use-cases--functionality)
- [Architecture](#architecture)
- [Hardware requirements](#hardware-requirements)
- [Firmware requirements](#firmware-requirements)
- [Software requirements](#software-requirements)
- [Known limits](#known-limits)
- [Design documents](#design-documents)

---

## Use cases / functionality

What a user can do with Oscil.

![Use cases and functionality](screenshots/use_case_functionality_svg.svg)

---

## Architecture

Three ESP32-S3 boards (acquisition, display, generator) linked over UART, with Supabase as the backend.

![Architecture](screenshots/architecture_screenshort_svg.svg)

---

## Hardware requirements

What the hardware must provide, and why the external ADC and dedicated display MCU were chosen.

![Hardware requirements](screenshots/hardware_requirements_screenshot_svg.svg)

---

## Firmware requirements

How the three ESP32-S3s acquire, display, generate, and communicate.

![Firmware requirements](screenshots/firmware_requirements_screenshot_svg.svg)

---

## Software requirements

The cloud side: accounts, screenshot metadata, and file storage on Supabase.

![Software requirements](screenshots/software_requirements_screenshot_svg.svg)

---


## Known limits

A breadboard build with three dev boards, made to learn and to show the
whole path from ADC to cloud. It is not a lab instrument.

- Sampling tops out at about 620 kSa/s per burst, 12 bits per channel.
- The link carries 31.8 frames/s; the screen draws fewer, about 5 to 20
  frames/s depending on the view.
- The generator output is 0 to 3.3 V only, with no gain or offset stage.
- Wi-Fi is 2.4 GHz only, and credentials are compiled in from a
  git-ignored file. There is no on-device Wi-Fi setup yet.
- Photos are saved only to the cloud and only while signed in.
- OTA rollback is in place; the deliberate failure tests are still to be
  recorded.

---

## Design documents

| Document | Description |
|---|---|
| [`design_v0.pdf`](design_v0.pdf) | Full design, all sections on one document |
| [`Oscil_bill_of_materials_bom.xlsx`](Oscil_bill_of_materials_bom.xlsx) | Bill of materials |

PNG versions of each diagram are in [`screenshots/`](screenshots/).

---

## Revision history

| Rev | Date | Change |
|---|---|---|
| 0.1 | 2026-09-16 | Initial design: use cases, requirements, architecture. Nothing built. |
| 0.2 | 2026-09-22 | Hardware Rev A schematic complete. Firmware started. |
| 0.3 | 2026-09-30 | ADC capture, trigger, board links, display, touch and generator running. |
| 0.4 | 2026-10-02 | Touch and knob controls, generator screen, saved settings, two-point calibration, 16 MB OTA partition table. |
| 0.5 | 2026-10-03 | Wi-Fi, HTTPS OTA with self-test, Supabase accounts and cloud photo saves. |
| 0.6 | 2026-10-04 | Photo gallery and viewer on the device, trace dragging, photo numbering fix. |

---

Built by [Paul Simbulan](https://paulsimbulan.com)