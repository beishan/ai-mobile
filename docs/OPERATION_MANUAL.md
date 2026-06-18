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

Button wiring:

| Button | ESP32 pin | Note |
|--------|-----------|------|
| POWER | GPIO0 | Internal pull-up |
| UP | GPIO35 | External 10k pull-up to 3.3V |
| HOME | GPIO34 | External 10k pull-up to 3.3V |
| DOWN | GPIO39 | External 10k pull-up to 3.3V |

Current firmware status:

- `app_main` renders the shared UI into the 400 x 300 framebuffer.
- `esp_display` configures CS/DC/RST as outputs, BUSY as input, and initializes the SPI bus on SCK/SDA.
- `esp_display` provides reset, busy wait, command send, and data send primitives.
- `platform/epd_frame` packs each rendered frame into 15,000-byte black and red 1bpp planes.
- `esp_input` polls POWER/UP/HOME/DOWN and routes button events into `app_handle_button`; each event re-renders and presents a new frame.
- Frame presentation logs packed byte counts, black/red pixel counts, and checksums until the exact controller init/refresh sequence is added.
- Physical panel refresh still needs the exact E-Ink controller command sequence.
