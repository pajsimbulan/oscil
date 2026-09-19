# Datasheets

Reference documents for the parts used in Oscil.

The PDFs themselves are **not tracked in this repo** — they belong to their
manufacturers. Download them into this folder using the links below; the
`.gitignore` here keeps them out of version control.

All links verified 2026-09-18.

| Part | Document | What it's used for |
|---|---|---|
| ADS7883 | [SLAS594 (TI)](https://www.ti.com/lit/ds/symlink/ads7883.pdf) | p.11 source impedance limit (<200 Ω); Fig. 25 supply/reference decoupling (1 µF + 10 nF) |
| MCP1700 | [DS20001826F (Microchip)](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP1700-Data-Sheet-20001826F.pdf) | Analog 3.3 V rail. Dropout, 250 mA limit, required input/output capacitors |
| MCP6292 | [MCP6291 Family, DS20001812G (Microchip)](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6291-Family-Data-Sheet-DS20001812G.pdf) | AFE buffer and reference buffer. 10 MHz GBWP, rail-to-rail, supply range |
| BAV99 | [Product data sheet (Nexperia)](https://assets.nexperia.com/documents/data-sheet/BAV99.pdf) | Input clamp. Forward voltage, reverse leakage, switching speed |
| BAV99 series | [BAV99_SER (Nexperia)](https://assets.nexperia.com/documents/data-sheet/BAV99_SER.pdf) | Pin roles: 1 = anode D1, 2 = cathode D2, 3 = common node |
| ESP32-S3-WROOM-1 | [Module datasheet (Espressif)](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf) | Pin definitions (Table 3-1), strapping pins, reserved GPIOs on R8 PSRAM parts |
| GT911 | [Datasheet Rev.09](https://www.fortec-integrated.de/fileadmin/pdf/produkte/Touchcontroller/DDGroup/GT911_Datasheet.pdf) | I²C address strap: RESET/INT state at power-on selects 0xBA/0xBB or 0x28/0x29 |
| GT911 | [Programming Guide](https://www.orientdisplay.com/pdf/GT911.pdf) | Register map, coordinate read sequence |
| XL6009 | [Datasheet Rev 1.1](https://rigpix.com/components/xl6009.pdf) | Backlight boost. FB pin regulates to 1.25 V through the external divider |

## Notes

- **XL6009**: XLSEMI has no stable official download. The link above is a
  third-party mirror of Rev 1.1. Treat it accordingly.
- **Espressif** moved their documentation to `documentation.espressif.com`.
  Old `www.espressif.com/sites/default/files/...` links no longer resolve.
- **GT911**: the I²C address strap is in the *datasheet*, not the programming
  guide. The programming guide covers registers only.
