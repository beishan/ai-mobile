# Operation Manual

## ESP32 Firmware Build

Build the real-device firmware:

```bash
pio run -e esp32-n16r8
```

Upload to the connected ESP32:

```bash
pio run -e esp32-n16r8 -t upload
```

Open the serial monitor:

```bash
pio device monitor -e esp32-n16r8
```

## E-Ink Wiring

The wiring map is centralized in `src/platform/esp_board_config.h`. Change that
file first if the hardware wiring changes, then update this table and
`requires01.md`.

| EPD pin | ESP32 pin |
|---------|-----------|
| BUSY | GPIO4 |
| RST | GPIO16 |
| DC | GPIO17 |
| CS | GPIO5 |
| SCK | GPIO18 |
| SDA | GPIO23 |
| GND | GND |
| VCC | 3V |

Current firmware status:

- `app_main` renders the shared UI into the 400 x 300 framebuffer.
- `esp_display` configures CS/DC/RST as outputs, BUSY as input, and initializes the SPI bus on SCK/SDA.
- `esp_display` provides reset, busy wait, command send, and data send primitives.
- Frame presentation logs framebuffer black/red pixel counts until the exact controller init/refresh sequence is added.
- Physical panel refresh still needs the exact E-Ink controller command sequence.
