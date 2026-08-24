# AI Mobile E-Ink Reader

ESP32-S3 reader firmware and desktop simulators for selectable black/white E-Ink panels.

Current hardware target:

- MCU: ESP32-S3 N16R8.
- Panels: OSPTEK 4.26 inch 480 x 800 (default), or HINK-E037A03-A1 3.7 inch 240 x 416.
- Driver ICs: SSD1677 or UC8171/IL0324.
- Display interface: SPI.
- Framebuffer: one 1bpp black/white plane, 48,000 bytes per frame.

The project is now reader-first. The old game module has been removed from the shared app state, home screen, renderer, and simulator flow.

## Build

```bash
make reader_sim
make reader_sim_sdl
```

## Test

```bash
make test
```

## Run

SDL2 windowed simulator:

```bash
./reader_sim_sdl
```

PPM export simulator:

```bash
./reader_sim
```

Controls:

- `w` / Up: up or previous item.
- `s` / Down: down or next item.
- `h`, Space, or Enter: home/select.
- `b` or Esc: back.
- `p` or Backspace: power.
- `q`: quit.

The headless simulator writes the latest 480 x 800 frame to `out/frame.ppm`.

## Current Modules

- Home: six app entries, `阅读 / 文件 / 天气 / 日历 / 英语 / 设置`.
- Files: browses `/sdcard`, enters folders, returns to parent folders, and opens selected UTF-8 `.txt` files in the reader.
- Bookshelf: lists TXT and EPUB books indexed from the SD card, with per-book progress and bookmarks.
- Reader: SD-backed TXT/EPUB reader with page turning, catalog, bookmarks, font size, and line spacing settings.
- Persistence: portable snapshot codec for per-book progress, bookmarks, recent book, and reader/settings state; simulators load it from `out/app_state.txt`, and ESP32 firmware stores the same payload in NVS.
- Back: the dedicated BACK key returns to the parent page; POWER long press saves app state and requests display sleep.
- Weather: mock city switching, refresh state, WiFi/offline cache behavior.
- Calendar: month switching and selected-day detail strip.
- English: front/back word card, known/review counts, answer-state dots.
- Settings: font size, line spacing, WiFi, city, and power-saving state.
- About: ESP32-S3 N16R8 plus 4.26 inch 480 x 800 SSD1677 SPI panel target.

## ESP32 Firmware

The PlatformIO + ESP-IDF firmware reuses the same portable `gfx`, `font`, `app`, and `ui` modules as the simulators.

Build firmware:

```bash
pio run -e esp32-n16r8
```

Build the HINK-E037A03-A1 firmware:

```bash
pio run -e esp32-n16r8-hink-e037a03-a1
```

The HINK target keeps the 480 x 800 logical UI and scales its 1bpp output to
the native 240 x 416 portrait panel. Its physical frame is 12,480 bytes and
BUSY is active-low. Use the same GPIO-level SPI signals listed below when the
panel is connected through a compatible 24-pin EPD power/adapter circuit.
Do not plug the bare HINK FPC into the existing SSD1677 PCB until its FPC pinout
and high-voltage reference circuit have been checked against the exact panel
datasheet.

Upload firmware:

```bash
pio run -e esp32-n16r8 -t upload
```

Monitor serial logs:

```bash
pio device monitor -e esp32-n16r8
```

Hardware status:

- `src/platform/epd_panel.h` selects panel geometry, controller family, frame size, and BUSY polarity at compile time.
- `src/platform/esp_board_config.h` centralizes the shared SPI wiring and timing constants.
- `src/platform/epd_frame.c` packs the shared framebuffer into the selected panel's black/white 1bpp plane.
- `src/platform/ssd1677.c` implements SSD1677 reset-time setup, RAM addressing, full-frame transfer, refresh, BUSY synchronization, power-off, and deep sleep.
- `src/platform/il0324.c` implements the HINK 240 x 416 UC8171/IL0324 command sequence, full/partial transfer, refresh, and sleep.
- `src/platform/esp_display.c` connects the selected controller to ESP-IDF GPIO/SPI.
- `src/platform/esp_input.c` scans BACK/POWER/UP/HOME/DOWN in a dedicated 20ms FreeRTOS task, applies 40ms debounce, and queues events while E-Ink refresh blocks the UI loop.
- `src/platform/esp_sd.c` mounts a FAT-formatted SPI SD card at `/sdcard`; startup indexes alphabetically sorted `.txt` and `.epub` files from `/sdcard/books`.
- POWER long press is detected after 1200ms, synchronously saves reading state, sleeps the SSD1677, stops Wi-Fi/Web services, and enters GPIO-wakeable light sleep.
- `src/app/reader_library.c` indexes SD TXT and EPUB files used by the bookshelf and reader. EPUB package metadata and spine-ordered XHTML are extracted into a hidden SD cache, then the shared paginator keeps page offsets (up to 2,048 reader pages per book).
- `src/app/app_persistence.c` captures durable app state into a versioned text payload, with simulator file save/load wired through `out/app_state.txt` and ESP32 NVS save/load wired through the `reader/app_state` key.
- The desktop simulators load TXT and EPUB files from `assets/books/realbook`; no built-in book-text fallback is provided.
- Text sources may use form-feed (`\f`) for explicit page breaks; plain text without page breaks is split automatically on UTF-8-safe boundaries.
- The 480 x 800 UI framebuffer is allocated from PSRAM instead of the ESP-IDF main-task stack.
- The SSD1677 display layer keeps a 48,000-byte previous-frame cache in PSRAM and derives one byte-aligned dirty rectangle from actual framebuffer differences. Unchanged frames are skipped, small changes use one differential partial refresh, and generic UI changes covering at least 70% of the panel use a full refresh. Entering the reader and turning text pages explicitly use a partial refresh for the reader area; the cumulative ghosting limit still promotes an occasional update to a full refresh.
- Partial refreshes are counted globally by the display driver. A full refresh clears ghosting after either 24 consecutive partial updates or roughly 12 screenfuls of cumulative updated area. Reader pages therefore normally remain partial for about 12 page turns; adjust `ESP_EPD_PARTIAL_REFRESH_LIMIT` or `ESP_EPD_PARTIAL_AREA_SCREENS` in `esp_board_config.h` if the panel needs a different interval.
- TXT/EPUB pagination follows the selected font size/family, line spacing, margins, indentation, and bold width. Chapter-like lines are indexed for catalog navigation, progress/bookmarks are matched by stable book IDs instead of shelf position, and files exceeding 2,048 laid-out pages show an explicit truncation warning.
- Opening a newly indexed book exposes its first 16 pages immediately and continues full-book pagination on the other CPU core. Rendering a page synchronously caches the next ready page, and background progress never triggers an E-Ink refresh by itself, so page turns within the ready range remain responsive.
- Wi-Fi + NTP time sync is ready. Open Settings and press HOME to edit SSID/password on the device. With battery saving enabled, Wi-Fi turns off after boot-time sync/weather work and is restarted only for Wi-Fi settings, manual sync, or scheduled weather; the Web admin follows the same lifecycle.
- Battery saving enables 40–160MHz dynamic frequency scaling and automatic tickless light sleep. After 30 seconds without input and once background work is idle, the device enters GPIO-wakeable light sleep with a 10-minute maintenance wake. Non-interactive reader status clocks update every 10 minutes, or every 30 minutes below 15% battery; automatic weather refresh is disabled below 15%.

Current E-Ink wiring:

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

Current button wiring:

| Button | ESP32 pin | Note |
|--------|-----------|------|
| BACK | GPIO42 | Internal pull-up; button connects to GND |
| POWER | GPIO41 | External 10k pull-up to 3.3V |
| UP | GPIO38 | External 10k pull-up to 3.3V |
| HOME | GPIO39 | External 10k pull-up to 3.3V |
| DOWN | GPIO40 | External 10k pull-up to 3.3V |

Current SD expansion-board wiring:

| SD pin | ESP32-S3 pin | Note |
|--------|----------------|------|
| CS | GPIO13 | Dedicated SD chip select |
| MOSI | GPIO17 | Shared with E-Ink SDA/MOSI |
| CLK | GPIO18 | Shared with E-Ink SCK |
| MISO | GPIO21 | SD data back to ESP32-S3 |
| GND | GND | Common ground |
| VCC | 3V3 | Use a 3.3V-compatible module |

Format the card as FAT32 and place UTF-8 text files under `/books`. The SD module
normally provides the required SPI pull-ups; if yours does not, add external
10k pull-ups as required by the card/module design.

## Font Asset

The simulator uses tracked bitmap font assets in `assets/fonts/` so host output stays deterministic.

To regenerate them on macOS:

```bash
python3 tools/generate_font.py
```
