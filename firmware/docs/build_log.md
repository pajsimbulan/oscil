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
They check the math, not the hardware.
