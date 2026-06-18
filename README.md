# AI Mobile E-Ink Reader

Desktop simulator for an ESP32 three-color E-Ink reader UI.

The simulator renders a 400 x 300 black/white/red frame with readable Chinese
UI text. It is the host-side prototype for the final device interface.

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

- `w`: up
- `s`: down
- `h` or Enter: home/select
- `p`: power/back
- `q`: quit

The latest simulated E-Ink frame is written to `out/frame.ppm`.

Reader behavior:

- `Up` / `Down`: previous or next page.
- `Home`: open the reader menu.
- Reader menu: `Up` / `Down` moves selection, `Home` executes, `Power` closes the menu.
- `查看目录` opens a catalog overlay; `Up` / `Down` selects a chapter, `Home` jumps, `Power` returns to the reader menu.
- `添加书签` records the current book page and changes the menu item to `已加书签`.
- Bookshelf progress is linked to each mock book's current reader page.
- Bookshelf marks the most recently opened book with a red `最近` label.
- Settings font size and line spacing immediately change reader body layout.
- The normal reader page keeps the bottom area blank; it does not show operation hints or a bottom progress bar.

Weather behavior:

- `Up` / `Down`: switch mock city.
- `Home`: refresh weather when WiFi is connected.
- If WiFi is disabled, refresh marks the page as cached/stale and shows the cache age in red.

Calendar behavior:

- Month view: `Up` / `Down` switches previous or next month.
- `Home` opens or closes the selected day detail strip.
- Detail view: `Up` / `Down` moves the selected day by one week.

Settings behavior:

- `Up` / `Down` moves through setting rows.
- Font size and line spacing update reader layout immediately.
- WiFi, city, and power-saving rows update shared app state.
- City selection cycles Beijing, Shanghai, and Guangzhou and is shared with weather.

English behavior:

- Front side: `Up` / `Down` switches words, `Home` flips to meaning.
- Back side: `Up` marks unknown/review, `Down` marks known, then advances to the next word.
- The word card shows known/review counts and answer-state progress dots.

Game behavior:

- Game list: `Up` / `Down` switches between Snake, Sokoban preview, and Sudoku preview.
- Snake: `Home` starts from the selected Snake row.
- Running Snake: `Up` / `Down` changes vertical direction and moves one step; `Home` moves one step in the current direction.
- Red food adds score; hitting the wall enters the game-over state.

## SDL2 Simulator

`reader_sim_sdl` is the primary interactive simulator. It opens a scaled SDL2
window showing the 400 x 300 three-color E-Ink framebuffer with readable
Chinese UI and pixel app icons on the home screen.

SDL2 controls:

- `Up` or `w`: up
- `Down` or `s`: down
- `Return`, Space, or `h`: home/select
- `Backspace`, `Esc`, or `p`: power/back
- `q` or window close: quit

Headless smoke test:

```bash
SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke
```

## ESP32 Hardware Firmware

The repository now includes a first PlatformIO + ESP-IDF firmware skeleton for
real ESP32 hardware. It reuses the same portable `gfx`, `font`, `app`, and `ui`
modules used by the simulators.

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

Current hardware status:

- `app_main` initializes the app state, framebuffer, built-in bitmap font, and renders the home page.
- The wiring is centralized in `src/platform/esp_board_config.h`.
- `src/platform/esp_display.c` configures the E-Ink GPIO pins and SPI bus using that wiring map.
- `esp_display` now provides reset, busy wait, command send, and data send primitives.
- `platform/epd_frame` packs the shared framebuffer into 15,000-byte black and red 1bpp planes.
- `src/platform/esp_input.c` polls the four active-low hardware buttons and routes them to the shared app state.
- Frame presentation logs packed byte counts, black/red pixel counts, and checksums until the exact controller init/refresh sequence is added.
- The exact E-Ink SPI controller, reset/busy timing, and LUT/waveform sequence still need to be filled in before it can physically refresh the panel.

Current E-Ink wiring:

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

Current button wiring:

| Button | ESP32 pin | Note |
|--------|-----------|------|
| POWER | GPIO0 | Internal pull-up |
| UP | GPIO35 | External 10k pull-up to 3.3V |
| HOME | GPIO34 | External 10k pull-up to 3.3V |
| DOWN | GPIO39 | External 10k pull-up to 3.3V |

## Snapshots

Capture timestamped PNG snapshots for visual review:

```bash
python3 tools/capture_snapshots.py --label layout-pass
```

Snapshots are written to:

```text
snapshots/YYYYMMDD-HHMMSS-label/
```

`snapshots/latest` points to the newest snapshot folder.

The default snapshot set includes home, bookshelf, bookshelf recent,
bookshelf bookmark, reader, large-font reader, loose-line-spacing reader,
reader menu, reader catalog, weather, calendar, calendar next month, calendar
detail, weather city, weather offline, game list, running Snake, game-over
Snake, English, English back, English known, English review, settings,
settings city, settings WiFi off, and about pages.

## Chinese Font Asset

The simulator uses tracked bitmap font assets:

```text
assets/fonts/sim_zh12.h
assets/fonts/sim_zh14.h
assets/fonts/sim_zh16.h
assets/fonts/sim_zh18.h
assets/fonts/sim_zh20.h
assets/fonts/sim_zh22.h
assets/fonts/sim_zh24.h
```

This keeps runtime output deterministic and avoids depending on system fonts
when `reader_sim` runs.

To regenerate the asset on macOS:

```bash
python3 tools/generate_font.py
```

The generator uses `/System/Library/Fonts/PingFang.ttc` and Pillow. The
generated asset should be committed after regeneration.
