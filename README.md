# AI Mobile E-Ink Reader

ESP32 reader firmware and desktop simulators for a 4.26 inch 480 x 800 black/white high-refresh E-Ink panel.

Current hardware target:

- MCU: ESP32 N16R8.
- Panel: 4.26 inch 480 x 800 black/white E-Ink.
- Driver IC: SSD677.
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
- `p`, Esc, or Backspace: power/back.
- `q`: quit.

The headless simulator writes the latest 480 x 800 frame to `out/frame.ppm`.

## Current Modules

- Home: six app entries, `阅读 / 天气 / 日历 / 英语 / 设置 / 关于`.
- Bookshelf: mock books with per-book progress, recent marker, and bookmark state.
- Reader: shared content catalog, source-text backed page cache, page turning, reader menu, catalog overlay, bookmark action, font size and line spacing settings.
- Persistence: portable snapshot codec for per-book progress, bookmarks, recent book, and reader/settings state; simulators load it from `out/app_state.txt`, and ESP32 firmware stores the same payload in NVS.
- Power: ESP32 POWER short press keeps the existing back behavior; POWER long press saves app state and requests display sleep.
- Weather: mock city switching, refresh state, WiFi/offline cache behavior.
- Calendar: month switching and selected-day detail strip.
- English: front/back word card, known/review counts, answer-state dots.
- Settings: font size, line spacing, WiFi, city, and power-saving state.
- About: ESP32 N16R8 plus 4.26 inch 480 x 800 SSD677 SPI panel target.

## ESP32 Firmware

The PlatformIO + ESP-IDF skeleton reuses the same portable `gfx`, `font`, `app`, and `ui` modules as the simulators.

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

- `src/platform/esp_board_config.h` centralizes the SSD677 SPI wiring and panel constants.
- `src/platform/epd_frame.c` packs the shared framebuffer into a single 48,000-byte black/white 1bpp plane.
- `src/platform/esp_display.c` initializes GPIO/SPI primitives and logs packed SSD677 black/white frame statistics.
- `src/platform/esp_input.c` polls POWER/UP/HOME/DOWN with 60ms debounce and routes events through the shared app state.
- POWER long press is detected after 1200ms and handled by the ESP32 main loop as a display sleep request.
- `src/app/reader_library.c` is the current source-text catalog used by both the bookshelf and reader; it builds page text from source strings and is the handoff point for later SD/TXT ingestion.
- `src/app/app_persistence.c` captures durable app state into a versioned text payload, with simulator file save/load wired through `out/app_state.txt` and ESP32 NVS save/load wired through the `reader/app_state` key.
- The desktop simulators try to load `assets/books/santi.txt` at startup and fall back to built-in source text if the file is missing.
- Text sources may use form-feed (`\f`) for explicit page breaks; plain text without page breaks is split automatically on UTF-8-safe boundaries.
- The exact SSD677 init, LUT/waveform, update, and sleep command sequence still needs to be filled in against the panel vendor datasheet.

Current E-Ink wiring:

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

Current button wiring:

| Button | ESP32 pin | Note |
|--------|-----------|------|
| POWER | GPIO0 | Internal pull-up |
| UP | GPIO35 | External 10k pull-up to 3.3V |
| HOME | GPIO34 | External 10k pull-up to 3.3V |
| DOWN | GPIO39 | External 10k pull-up to 3.3V |

## Font Asset

The simulator uses tracked bitmap font assets in `assets/fonts/` so host output stays deterministic.

To regenerate them on macOS:

```bash
python3 tools/generate_font.py
```
