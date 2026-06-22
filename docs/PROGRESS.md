# Development Progress

Updated: 2026-06-22

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
  - Bookshelf selection.
  - Per-book page progress.
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
- Updated current documentation:
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
- True 480 x 800 vertical layout refinement beyond the current functional port.
- Persistent storage for reading progress, bookmarks, and settings.
- Real file ingestion for TXT, including UTF-8/GBK detection and pagination.
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
