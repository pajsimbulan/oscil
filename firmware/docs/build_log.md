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