# AI Mobile E-Ink Reader

ESP32-S3 reader firmware and desktop simulators for a 4.26 inch 480 x 800 black/white high-refresh E-Ink panel.

Current hardware target:

- MCU: ESP32-S3 N16R8.
- Panel: 4.26 inch 480 x 800 black/white E-Ink.
- Driver IC: SSD1677.
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

Upload firmware:

```bash
pio run -e esp32-n16r8 -t upload
```

Monitor serial logs:

```bash
pio device monitor -e esp32-n16r8
```

Hardware status:

- `src/platform/esp_board_config.h` centralizes the SSD1677 SPI wiring and panel constants.
- `src/platform/epd_frame.c` packs the shared framebuffer into a single 48,000-byte black/white 1bpp plane.
- `src/platform/ssd1677.c` implements SSD1677 reset-time setup, RAM addressing, full-frame transfer, refresh, BUSY synchronization, power-off, and deep sleep.
- `src/platform/esp_display.c` connects the SSD1677 command driver to ESP-IDF GPIO/SPI and transfers the 180-degree rotated 48,000-byte frame for the current panel mounting.
- `src/platform/esp_input.c` scans BACK/POWER/UP/HOME/DOWN in a dedicated 20ms FreeRTOS task, applies 40ms debounce, and queues events while E-Ink refresh blocks the UI loop.
- `src/platform/esp_sd.c` mounts a FAT-formatted SPI SD card at `/sdcard`; startup indexes alphabetically sorted `.txt` and `.epub` files from `/sdcard/books`.
- POWER long press is detected after 1200ms and handled by the ESP32 main loop as a display sleep request.
- `src/app/reader_library.c` indexes SD TXT and EPUB files used by the bookshelf and reader. EPUB package metadata and spine-ordered XHTML are extracted into a hidden SD cache, then the shared paginator keeps page offsets (up to 2,048 reader pages per book).
- `src/app/app_persistence.c` captures durable app state into a versioned text payload, with simulator file save/load wired through `out/app_state.txt` and ESP32 NVS save/load wired through the `reader/app_state` key.
- The desktop simulators load TXT and EPUB files from `assets/books/realbook`; no built-in book-text fallback is provided.
- Text sources may use form-feed (`\f`) for explicit page breaks; plain text without page breaks is split automatically on UTF-8-safe boundaries.
- The 480 x 800 UI framebuffer is allocated from PSRAM instead of the ESP-IDF main-task stack.
- The SSD1677 driver uses OTP full refresh for page changes and differential partial refresh for all same-page interactions: home and bookshelf selection, file-list navigation, reader turns/menu/catalog/settings, weather, calendar, English cards, settings, and Wi-Fi editing. Directory reloads and page transitions retain full refresh. Real-panel validation is still pending because the current EPD BUSY signal does not become ready.
- Partial refreshes are counted globally by the display driver. After 12 consecutive successful partial updates, the next partial request is automatically promoted to a full refresh to clear ghosting; adjust `ESP_EPD_PARTIAL_REFRESH_LIMIT` in `esp_board_config.h` if the panel needs a different interval.
- TXT/EPUB pagination follows the selected font size/family, line spacing, margins, indentation, and bold width. Chapter-like lines are indexed for catalog navigation, progress/bookmarks are matched by stable book IDs instead of shelf position, and files exceeding 2,048 laid-out pages show an explicit truncation warning.
- Wi-Fi + NTP time sync is ready. Open Settings and press HOME to edit SSID/password on the device, then select “保存并重启校时”. Credentials are stored in NVS; the firmware uses `ntp.aliyun.com`, China Standard Time (`CST-8`), waits up to 12 seconds at boot, and partial-refreshes the status clock every minute.

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
