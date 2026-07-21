# Development Progress

Updated: 2026-07-17

## Current Target

- ESP32-S3 N16R8.
- 4.26 inch 480 x 800 black/white high-refresh E-Ink panel.
- SSD1677 driver IC over SPI.
- Reader-first product scope with seven modules: reading, SD files, weather, calendar, English, settings, and about.

## Completed

- Merged `feature/reader-simulator` into `main`.
- Re-centered the codebase on the current 480 x 800 black/white SSD1677 target.
- Updated `gfx` dimensions to 480 x 800.
- Reduced logical colors to white and black.
- Updated EPD packing to a single 48,000-byte `bw` plane.
- Removed the interactive entertainment module from the shared app state, home navigation, icons, and UI rendering.
- Kept the reader flow from the simulator branch:
  - SD-backed TXT/EPUB library in `src/app/reader_library.c`: keeps metadata/page offsets and reads each rendered page from the card.
  - Bookshelf selection.
  - Per-book page progress.
  - Per-page reader text loaded from the selected SD file when it is rendered.
  - Plain text can be split automatically when no explicit form-feed page breaks are present.
  - Desktop simulator startup indexes TXT and EPUB files under `assets/books/realbook`; no built-in book-text fallback is provided.
  - Recent book marker.
  - Reader menu.
  - Catalog overlay.
  - Bookmark state.
  - Font size and line spacing settings.
- Kept supporting mock modules:
  - Weather city/cache flow.
  - Calendar month/detail flow.
  - English front/back learning flow.
  - Settings state updates.
- Updated ESP platform files:
  - `src/platform/esp_board_config.h` names the SSD1677 target and 480 x 800 panel constants.
  - `src/platform/ssd1677.c` implements controller setup, full RAM writes, OTP full refresh, windowed differential partial refresh, BUSY synchronization, power-off, and deep sleep.
  - `src/platform/esp_display.c` connects the portable command driver to ESP-IDF GPIO/SPI.
  - `src/platform/esp_sd.c` mounts a FAT SPI SD card on the shared E-Ink bus, using independent chip-select pins.
  - Firmware loads up to three sorted UTF-8 `.txt` files from `/sdcard/books` before initializing bookshelf page counts.
  - Portrait UI pixels are rotated into the SSD1677 panel's native 800 x 480 RAM order.
  - The 384,000-byte UI framebuffer is allocated from ESP32-S3 Octal PSRAM rather than the main task stack.
  - `src/platform/epd_frame.c` packs black pixels into one 1bpp plane.
- Retargeted all 8 UI pages to true 480×800 portrait layout:
  - Defined shared layout constants (`PAGE_MARGIN_X`, `CONTENT_WIDTH`, `BODY_TOP`, `BODY_BOTTOM`, `BODY_HEIGHT`) in `src/ui/pages.c` replacing scattered magic numbers.
  - Home: 3 rows × 2 columns tile grid filling content area (was 2×3).
  - Bookshelf: tall card rows (~224px each) with book metadata + progress bar (was 40px compact rows).
  - Reader: body text area expanded from 154px to ~680px (4.4×), added bottom reading progress strip; menu and catalog overlays resized and centered for the larger panel.
  - Weather: large temperature hero block, three full-width forecast cards, full-width air quality bar.
  - Calendar: enlarged weekday header and day cells (~80px row height), detail box pushed below grid.
  - English: large word card (240px), stats and answer-state dots spread vertically.
  - Settings: six rows with ~110px each, selected row uses full-width inverted bar.
  - About: title and info lines distributed across full content height with larger fonts.
- Updated `tests/test_runner.c` pixel-coordinate assertions for the new layout and added `test_reader_body_fills_expanded_content_area`.
- Added portable app persistence:
  - `src/app/app_persistence.c` captures durable reader/settings state.
  - Versioned text encode/decode for per-book current pages, bookmarks, recent book, font size, line spacing, WiFi, city, and power-saving state.
  - Restore clamps saved values to current book page counts and setting ranges.
  - PPM and SDL simulators load `out/app_state.txt`, save after state changes, and save again on exit.
  - ESP32 firmware initializes NVS flash, restores `reader/app_state` on boot, and saves after button events.
  - Tests cover round trip, clamping, malformed payload rejection, and file save/load.
- Added ESP32 button debounce:
  - `src/platform/input_debounce.c` provides a portable stable-sample debounce state machine.
  - `src/platform/esp_input.c` scans BACK/POWER/UP/HOME/DOWN in a dedicated 20ms FreeRTOS task, uses 40ms debounce, and queues events during blocking E-Ink refresh; BACK is wired to GPIO42.
  - `src/platform/esp_time_sync.c` stores Wi-Fi credentials in NVS, connects as a station, synchronizes China Standard Time from NTP, and the settings page provides button-only SSID/password entry.
  - Tests cover bounce suppression, re-arm after release, and ESP wiring.
- Added POWER long-press handling:
  - `input_debounce_update_hold` distinguishes short press on release from long press while held.
  - ESP32 POWER long press uses a 1200ms threshold.
  - `main_esp.c` saves NVS state and calls `esp_display_sleep` for POWER long press.
  - `README.md`.
  - `requires01.md`.
  - `docs/OPERATION_MANUAL.md`.
  - `docs/superpowers/CURRENT_TARGET.md`.

## Verification

Latest successful checks on 2026-07-17:

```bash
pio run -e esp32-n16r8
```

Build result:

```text
SUCCESS
RAM: 102,464 / 327,680 bytes (31.3%)
Flash: 995,905 / 5,242,880 bytes (19.0%)
```

The standalone host smoke test also passes for portrait-to-native frame packing,
SSD1677 initialization, previous/current RAM writes, full refresh, BUSY waits,
power-off, and deep sleep. The command sequence, BUSY polarity, and GPIO wiring
were aligned with the successfully run `ping_test` project.

## In Progress

- Real-device validation of SSD1677 orientation, BUSY polarity, full refresh, repeated updates, sleep, and wake.
- Hardware-validate the implemented home-selection partial refresh after the EPD BUSY/wiring fault is resolved.
- Real-device validation of SD mount, shared-bus arbitration, and TXT loading; GBK detection remains future work.
- Real weather/network integration.

## Useful Commands

Build and run SDL2 simulator:

```bash
make reader_sim_sdl
./reader_sim_sdl
```

Build and run PPM simulator:

```bash
make reader_sim
./reader_sim
```

Run tests:

```bash
make test
```
