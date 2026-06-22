# Development Progress

Updated: 2026-06-18 11:32

## Current Branch

- Branch: `feature/reader-simulator`
- Workspace: `/Users/beibei/aiprojects/ai-mobile/.worktrees/reader-simulator`
- Latest pushed/committed work includes SDL2 simulator foundation and Chinese simulator improvements.
- Current working tree has additional uncommitted changes for font/layout, snapshots, and UI fixes.

## Completed

- Product requirements reviewed from `requires01.md`.
- UI mockup reviewed from `eink_ui_mockups.png`.
- Git repository initialized and pushed to `git@github.com:beishan/ai-mobile.git`.
- Desktop C simulator created:
  - `reader_sim` PPM export mode.
  - 400 x 300 framebuffer.
  - Black/white/red logical colors.
  - Button-driven app state.
- Chinese rendering added:
  - UTF-8 decoder.
  - Repository-contained bitmap font assets.
  - Chinese UI pages.
- SDL2 simulator added:
  - `reader_sim_sdl` windowed simulator.
  - SDL2 keyboard mapping.
  - Dummy-video smoke mode.
- ESP32 hardware firmware skeleton added:
  - `platformio.ini` with `esp32-n16r8` ESP-IDF environment.
  - `partitions_16mb.csv` for 16MB flash OTA layout.
  - `sdkconfig.defaults` for PSRAM/log/FATFS/TLS defaults.
  - ESP-IDF `CMakeLists.txt` files that reuse portable `gfx`, `font`, `app`, and `ui` sources.
  - `src/main_esp.c` boots, initializes shared app state, renders the first home frame, and presents it.
  - E-Ink wiring is centralized in `src/platform/esp_board_config.h`: BUSY=GPIO4, RST=GPIO16, DC=GPIO17, CS=GPIO5, SCK=GPIO18, SDA=GPIO23.
  - `src/platform/esp_display.c` configures E-Ink GPIO/SPI and exposes reset, busy wait, command send, and data send primitives.
  - Frame presentation logs framebuffer black/red pixel counts until the exact controller init/refresh sequence is wired.
  - `docs/OPERATION_MANUAL.md` records build/upload/monitor commands and wiring.
- Home page improved:
  - Weather moved into status bar.
  - WiFi signal icon added.
  - Battery icon plus percentage added.
  - Red status strip removed.
  - 4-column icon grid.
  - Outer tile frames removed.
  - Pixel app icons added.
- Font/layout system improved:
  - Multi-size generated fonts: 12, 14, 16, 18, 20, 22, 24 px.
  - Glyph metrics added.
  - ASCII/digit spacing made compact.
  - Status bar text clipping fixed.
  - Home labels centered.
  - Reader body uses wrapped 20 px text.
  - Reader body now follows settings-controlled font size.
  - Reader body now follows settings-controlled line spacing.
  - Other pages migrated to role-based font sizes.
  - Bookshelf, weather, calendar, English, settings, games, and about pages now use role-based font sizing and alignment helpers.
- Snapshot workflow added:
  - `tools/capture_snapshots.py`
  - Timestamped snapshot folders under `snapshots/`
  - `snapshots/latest` symlink to newest snapshot folder.
  - Snapshot set now includes reader body and reader menu pages.
- Reader flow improved:
  - HOME opens a selectable reader menu.
  - UP/DOWN move through menu items.
  - POWER closes the reader menu without leaving the reader.
  - Reader menu can add a bookmark and exit back to bookshelf.
  - Reader menu renders selected rows with clear inverse highlighting.
  - Reader footer hint text and bottom progress bar removed for a cleaner reading page.
  - Bookshelf progress now follows each mock book's current reader page.
  - Reader opens the selected bookshelf book and restores that book's page.
  - Reader catalog overlay added through `查看目录`.
  - Catalog selection can jump to chapter start pages.
  - Bookmarks are stored per mock book and shown in menu/bookshelf state.
  - Bookshelf tracks the most recently opened book and shows a red `最近` marker.
  - Bookshelf snapshots now include recent-book and bookmarked-book variants.
  - Font assets regenerated to include new catalog and reading text glyphs.
- Settings flow improved:
  - Font size setting changes reader body font size.
  - Line spacing setting changes reader body vertical rhythm.
  - Snapshot workflow includes large-font and loose-line-spacing reader variants.
- Weather flow improved:
  - UP/DOWN switches mock weather city.
  - HOME refreshes weather when WiFi is connected.
  - Offline refresh marks cached/stale weather state and shows cache age.
  - Weather snapshots now include city-switch and offline-cache variants.
  - Font assets regenerated to include new weather city/cache glyphs.
- Calendar flow improved:
  - Month view UP/DOWN switches month.
  - HOME opens/closes selected day detail strip.
  - Detail view UP/DOWN moves selected day by one week.
  - Calendar renders current day, selected day, weekend coloring, lunar/solar-term mock details.
  - Calendar snapshots now include next-month and detail variants.
  - Font assets regenerated to include calendar detail glyphs.
- English flow improved:
  - Front side UP/DOWN switches words and HOME flips to meaning.
  - Back side UP marks unknown/review and DOWN marks known.
  - Answering advances to the next word and resets to the front side.
  - English page renders known/review counts and answer-state progress dots.
  - English snapshots now include back-side, known, and review variants.
  - Font assets regenerated to include English flow glyphs.
- Game flow improved:
  - Game page now separates list, running Snake, and game-over states.
  - HOME starts Snake from the selected Snake row.
  - Running Snake uses E-Ink-friendly step controls: UP/DOWN changes vertical direction and moves once, HOME advances once.
  - Red food adds score and relocates deterministically.
  - Wall collision stops the run and shows game-over feedback.
  - Game snapshots now include list, running Snake, and game-over Snake variants.
  - Font assets regenerated to include new game flow glyphs.
- Settings flow improved:
  - City row now cycles Beijing, Shanghai, and Guangzhou.
  - Settings city selection updates the same `weather_city_index` used by the weather page.
  - Changing city from settings marks weather as cached/stale until refreshed.
  - Snapshot workflow now includes `settings_city.png` and `settings_wifi_off.png`.
- Requirements and planning docs synchronized:
  - `requires01.md`
  - `README.md`
  - `docs/superpowers/specs/*`
  - `docs/superpowers/plans/*`

## Current Snapshot Set

Latest generated snapshot folder:

```text
snapshots/20260618-112013-bookshelf-flow/
```

Contents:

```text
about.png
bookshelf.png
bookshelf_bookmark.png
bookshelf_recent.png
calendar.png
calendar_detail.png
calendar_next_month.png
english.png
english_back.png
english_known.png
english_review.png
game.png
game_snake_over.png
game_snake_running.png
home.png
reader.png
reader_catalog.png
reader_font_large.png
reader_line_loose.png
reader_menu.png
settings.png
settings_city.png
settings_wifi_off.png
weather.png
weather_city.png
weather_offline.png
```

Generate a new timestamped set:

```bash
python3 tools/capture_snapshots.py --label <label>
```

## Verification

Latest successful verification commands:

```bash
python3 tools/generate_font.py
make clean
make test
make reader_sim
make reader_sim_sdl
SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke
pio run -e esp32-n16r8
python3 tools/capture_snapshots.py --label bookshelf-flow
```

Latest `make test` result:

```text
tests passed
```

## In Progress

- SDL2 visual QA using timestamped snapshots.
- Home/app icon visual refinement.
- Module depth beyond first-level simulated pages.
- Game module depth beyond Snake preview/run loop.
- Real ESP display driver now has GPIO/SPI setup, controller primitives, and black/red 1bpp frame packing.
- Real ESP input now polls POWER/UP/HOME/DOWN and routes events into the shared app state; each event re-renders and presents a frame.
- Real ESP display driver still needs exact E-Ink controller model and refresh command/waveform sequence before physical refresh.

## Known Issues / Follow-Up

- Some icons are still code-drawn pixel approximations and can be refined further.
- Reading, weather, calendar, English, settings, and games are still simulated with mock data.
- Push-box and Sudoku are preview/list entries, not complete games yet.
- Persistence, real SD card storage, WiFi, and weather API are still pending.
- ESP firmware currently renders into the shared framebuffer, initializes GPIO/SPI, packs black/red frame planes, polls four hardware buttons, and exposes command/data primitives; it does not yet physically refresh a panel.

## Next Steps

1. Confirm the E-Ink panel controller model and refresh command/waveform sequence.
2. Replace the current `esp_display` packed-frame logging with SPI panel initialization and full refresh using the centralized pin map.
3. Add NVS persistence for settings, recent book, and reading positions.
4. Continue module depth:
   - Reader persistence to SD/NVS after hardware/storage skeleton exists.
   - Weather detail layout.
   - Calendar memo persistence after storage skeleton exists.
   - English word library/settings persistence after storage skeleton exists.
   - More settings value editing and mock sub-pages.

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

Generate font assets:

```bash
python3 tools/generate_font.py
```

Generate visual snapshots:

```bash
python3 tools/capture_snapshots.py --label review
```
