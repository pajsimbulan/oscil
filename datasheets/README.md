# Datasheets

The PDFs live here locally but aren't committed (see `.gitignore`).
Each link below was checked against the local copy: same document, same
revision, unless the notes say otherwise. Page numbers in the schematic
notes and build logs refer to these revisions.

| Part | Used for | Revision used | Link |
|---|---|---|---|
| ADS7883 | 12-bit 3 MSPS ADC, one per channel | SLAS594, July 2008 | [TI product page](https://www.ti.com/product/ADS7883) · [PDF](https://www.ti.com/lit/ds/symlink/ads7883.pdf) |
| MCP6292 | Dual op-amp: VREF buffer, AFE buffers, generator | DS20001812G | [Microchip product page](https://www.microchip.com/en-us/product/MCP6292) · [PDF](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6291-Family-Data-Sheet-DS20001812G.pdf) |
| MCP1700-3302E | 3.3 V LDO for the analog rail (+3V3_A) | DS20001826F | [Microchip product page](https://www.microchip.com/en-us/product/MCP1700) · [PDF](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP1700-Data-Sheet-20001826F.pdf) |
| BAV99 (SMC Diode Solutions) | Input clamp diodes | N0595 Rev. B | [SMC product page](https://www.smc-diodes.com/Products/Discrete/Diodes/BAV99.html) · [PDF, Rev. B](https://www.smc-diodes.com/propdf/BAV99%20N0595%20REV.B.pdf) |
| ESP32-S3 | MCU on all three boards | Datasheet v2.2 | [Espressif PDF](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) |
| ESP32-S3 TRM | Register-level reference (GPIO, IO MUX, SPI, GDMA) | TRM v1.8 | [Espressif PDF](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf) |
| ESP32-S3-WROOM-1 (N16R8) | Module on the dev boards | Datasheet v1.8 | [Espressif PDF](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf) |
| ESP32-S3-DevKitC-1 | Reference for the dev board header layout | esp-dev-kits docs | [Espressif docs](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html) |
| WS2812B (Worldsemi) | Onboard status LED on GPIO48 | "Datasheet and Specifications" edition | [PDF (SparkFun mirror)](https://cdn.sparkfun.com/assets/e/6/1/f/4/WS2812B-LED-datasheet.pdf) |
| AFY800480B0-7.0N12NTM-C | 7" 800x480 RGB panel | Rev. E | [Orient Display PDF](https://www.orientdisplay.com/pdf/AFY800480B0-7.0N12NTM-C.pdf) · [DigiKey](https://www.digikey.com/en/products/detail/orient-display/AFY800480B0-7-0N12NTM-C/12089250) |
| GT911 datasheet | Touch controller on the panel | Rev. 09, March 2015 | [PDF (ElectroDragon mirror)](https://w2.electrodragon.com/Chip-dat/goodix-dat/GT911-dat/GT911-DS.pdf) |
| GT911 programming guide | Touch controller registers | Programming guide | [Orient Display PDF](https://www.orientdisplay.com/pdf/GT911.pdf) |

Notes:

- The dev boards are Lonely Binary ESP32-S3 N16R8 boards. The DevKitC-1 page
  is only a reference; header pin numbers in `hardware/docs/pin-map.md` are
  for the Lonely Binary board.
- BAV99: SMC has since released Rev. D. The design was checked against Rev. B.
- WS2812B and GT911: neither Worldsemi nor Goodix hosts a stable public link,
  so these point to well-known mirrors of the same documents.
- The EC11 encoders came with the seller's spec sheet only.
