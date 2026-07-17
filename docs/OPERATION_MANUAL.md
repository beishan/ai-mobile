# Operation Manual

## Target

- Device: ESP32-S3 N16R8 reader.
- Display: 4.26 inch 480 x 800 black/white high-refresh E-Ink.
- Controller: SSD1677.
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
| DC | GPIO15 |
| CS | GPIO5 |
| SCK | GPIO18 |
| SDA/MOSI | GPIO17 |
| GND | GND |
| VCC | 3V |

Button wiring:

| Button | ESP32 pin | Note |
|--------|-----------|------|
| BACK | GPIO42 | Internal pull-up; button connects to GND |
| POWER | GPIO41 | External 10k pull-up to 3.3V |
| UP | GPIO38 | External 10k pull-up to 3.3V |
| HOME | GPIO39 | External 10k pull-up to 3.3V |
| DOWN | GPIO40 | External 10k pull-up to 3.3V |

SD expansion-board wiring (shared SPI bus):

| SD pin | ESP32-S3 pin | Note |
|--------|----------------|------|
| CS | GPIO13 | Independent from E-Ink CS |
| MOSI | GPIO17 | Shared with E-Ink SDA/MOSI |
| CLK | GPIO18 | Shared with E-Ink SCK |
| MISO | GPIO21 | Kept away from native USB GPIO19/20 |
| GND | GND | Common ground |
| VCC | 3V3 | The module must be 3.3V compatible |

Use a FAT32 card and put UTF-8 `.txt` books in `/books`. At startup the firmware
mounts the card at `/sdcard` and loads the first three files in alphabetic order.
An absent or invalid card leaves the bookshelf empty; the display and other
applications continue to start. Both E-Ink and SD chip-select lines must remain high
while the other device is using the shared bus.

## Current Firmware Status

- `app_main` renders the shared UI into the 480 x 800 framebuffer.
- To enable automatic time, open Settings and press HOME to enter the Wi-Fi configuration page. Select SSID or password with UP/DOWN, press HOME to edit, use UP/DOWN to choose each character, HOME to add it, POWER to delete, and BACK to finish that field. Select “保存并重启校时” to persist the credentials in NVS and reboot. The firmware queries `ntp.aliyun.com`, applies `CST-8`, and continues offline if NTP is not ready within 12 seconds. ESP32-S3 has no battery-backed RTC, so it must synchronize again after a power loss.
- The home page includes a file-browser entry. UP/DOWN selects an item, HOME enters a directory or opens a TXT file, and BACK returns to the parent directory or home page.
- `esp_sd` initializes the shared SPI bus and mounts the card before the first E-Ink transaction; `esp_display` then attaches the SSD1677 device to the same bus.
- `esp_display` provides reset, busy wait, command send, and data send primitives.
- `platform/epd_frame` packs each rendered frame into one 48,000-byte black/white 1bpp plane.
- `esp_input` scans BACK/POWER/UP/HOME/DOWN in a dedicated 20ms FreeRTOS task with 40ms debounce. An eight-event queue retains presses made while the E-Ink refresh path is blocking.
- POWER long press uses a 1200ms threshold. Firmware saves app state to NVS and requests display sleep.
- App state persistence is backed by NVS at namespace/key `reader/app_state`; firmware restores it during boot and saves it after button events.
- Frame presentation rotates the portrait framebuffer 180 degrees into the panel's native 800 x 480 RAM order, writes previous/current RAM, triggers the OTP full-refresh waveform, and waits for BUSY to clear.
- Long-press sleep powers the controller off and sends the SSD1677 deep-sleep command. The next presentation performs a hardware reset and reinitializes it.
- On the home page, UP/DOWN redraws only the old and new selection-tile windows. Bookshelf selection and in-folder file-list navigation likewise refresh only the affected cards/rows; entering a folder reloads the list and therefore keeps a full refresh.
- In the reader, UP/DOWN page turns redraw the text/progress area only; reader-menu changes, catalog selection, and in-place reader-setting edits also use their respective partial windows. Weather, calendar, English cards, settings, and Wi-Fi editing use bounded same-page partial windows. Entering a different page remains a full refresh.
- Before release, validate orientation, BUSY polarity, full refresh, home selection partial refresh, wake-after-sleep, and repeated refresh behavior on the exact GDEQ0426T82-compatible panel.
- The SSD1677 behavior and BUSY polarity follow the successfully run `ping_test`; GPIOs were remapped to pins exposed by the ESP32-S3 N16R8 board.
