# Operation Manual

## Target

- Device: ESP32 N16R8 reader.
- Display: 4.26 inch 480 x 800 black/white high-refresh E-Ink.
- Controller: SSD677.
- Interface: SPI.

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

The wiring map is centralized in `src/platform/esp_board_config.h`. Change that file first if hardware wiring changes, then update this table and `requires01.md`.

| EPD pin | ESP32 pin |
|---------|-----------|
| BUSY | GPIO4 |
| RST | GPIO16 |
| DC | GPIO17 |
| CS | GPIO5 |
| SCK | GPIO18 |
| SDA/MOSI | GPIO23 |
| GND | GND |
| VCC | 3V |

Button wiring:

| Button | ESP32 pin | Note |
|--------|-----------|------|
| POWER | GPIO0 | Internal pull-up |
| UP | GPIO35 | External 10k pull-up to 3.3V |
| HOME | GPIO34 | External 10k pull-up to 3.3V |
| DOWN | GPIO39 | External 10k pull-up to 3.3V |

## Current Firmware Status

- `app_main` renders the shared UI into the 480 x 800 framebuffer.
- `esp_display` configures CS/DC/RST as outputs, BUSY as input, and initializes the SPI bus on SCK/SDA.
- `esp_display` provides reset, busy wait, command send, and data send primitives.
- `platform/epd_frame` packs each rendered frame into one 48,000-byte black/white 1bpp plane.
- `esp_input` polls POWER/UP/HOME/DOWN and routes button events into `app_handle_button`; each event re-renders and presents a new frame.
- Frame presentation logs packed SSD677 black/white byte counts, black pixel counts, and checksums until the exact controller command sequence is added.
- Physical panel refresh still needs the exact SSD677 init, window, update, waveform, and sleep sequence from the panel vendor datasheet.
