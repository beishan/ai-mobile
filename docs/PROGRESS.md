# Development Progress

Updated: 2026-06-23

## Current Target

- ESP32 N16R8.
- 4.26 inch 480 x 800 black/white high-refresh E-Ink panel.
- SSD677 driver IC over SPI.
- Reader-first product scope with six modules: reading, weather, calendar, English, settings, and about.

## Completed

- Merged `feature/reader-simulator` into `main`.
- Re-centered the codebase on the current 480 x 800 black/white SSD677 target.
- Updated `gfx` dimensions to 480 x 800.
- Reduced logical colors to white and black.
- Updated EPD packing to a single 48,000-byte `bw` plane.
- Removed the interactive entertainment module from the shared app state, home navigation, icons, and UI rendering.
- Kept the reader flow from the simulator branch:
  - Shared source-text content catalog in `src/app/reader_library.c`.
  - Bookshelf selection.
  - Per-book page progress.
  - Per-page reader text generated from source text into a small page cache.
  - Plain text can be split automatically when no explicit form-feed page breaks are present.
  - Desktop simulator startup loads `assets/books/santi.txt` into the first book when available.
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
  - `src/platform/esp_board_config.h` names the SSD677 target and 480 x 800 panel constants.
  - `src/platform/esp_display.c` logs SSD677 black/white packed frame statistics.
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
  - `README.md`.
  - `requires01.md`.
  - `docs/OPERATION_MANUAL.md`.
  - `docs/superpowers/CURRENT_TARGET.md`.

## Verification

Latest successful checks:

```bash
make -B test
make reader_sim
make reader_sim_sdl
```

Latest test result:

```text
tests passed
```

## In Progress

- Real SSD677 command sequence, LUT/waveform, update trigger, and sleep behavior.
- Persistent storage for reading progress, bookmarks, and settings.
- Real file ingestion for TXT, including UTF-8/GBK detection and pagination, behind the `reader_library` interface.
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
