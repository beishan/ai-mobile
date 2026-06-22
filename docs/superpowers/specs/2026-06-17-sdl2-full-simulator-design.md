# SDL2 Full Reader Simulator Design

Date: 2026-06-17

## Purpose

Make SDL2 the primary desktop simulator for the ESP32 three-color E-Ink reader. The simulator should feel like a small device: a scaled 400 x 300 screen, four-button keyboard control, clear Chinese UI, distinguishable pixel icons on the home screen, and usable flows for every application module.

The existing PPM simulator remains as a deterministic output path for tests and frame export.

## Core Requirements

- `reader_sim_sdl` opens an SDL2 window and renders the same 400 x 300 framebuffer used by the existing simulator.
- The SDL2 window scales the framebuffer by an integer factor, default 3x.
- Chinese text remains readable through the repository bitmap font.
- Home screen app entries use image-like pixel icons, not text-only tiles.
- Icons are repository-contained code/data assets and visually distinguish reading, weather, calendar, games, English, settings, and about.
- SDL2 keyboard events map to the four physical buttons.
- All application modules have usable simulated details, even where backend data is mocked.

## Controls

- `Up` or `w`: up button.
- `Down` or `s`: down button.
- `Return`, `Space`, or `h`: home/select button.
- `Backspace`, `Escape`, or `p`: power/back button.
- Window close or `q`: quit.

## Module Scope

- Home: status bar with time/weather, signal-style WiFi icon, battery icon plus percentage, seven app icons in a four-column grid, red selected state.
- Reader: bookshelf, open book, Chinese reader pages, top-bar page count, previous/next page, selectable reader menu. The normal reading page bottom is blank and does not show operation hints or a bottom progress bar.
- Weather: Beijing mock weather, manual refresh count, three-day forecast, air quality.
- Calendar: current month grid, today marker, weekend red text, lunar summary, month switching.
- Games: game list plus playable snake; push-box and sudoku preview entries with progress/status.
- English: word card front/back, Chinese meaning/example, progress dots, word navigation.
- Settings: Chinese setting rows, selectable/toggleable values for font size, line spacing, WiFi, city, and power saving.
- About: version, chip, flash, PSRAM, SD card, and project information.

## Architecture

The simulator keeps the current portable core:

- `gfx`: framebuffer and drawing primitives.
- `font`: UTF-8 and bitmap Chinese font rendering.
- `app`: state and button dispatch.
- `ui`: page rendering.
- `platform/sim_display`: PPM output.

SDL2 adds:

- `platform/sdl_display`: SDL window, renderer, texture, framebuffer-to-RGB upload, event polling.
- `ui/icons`: small three-color pixel icon assets and drawing helpers.
- `src/main_sdl.c`: SDL2 event loop.

The UI renders once into `gfx_framebuffer_t`; SDL2 only displays that buffer. This avoids duplicating UI logic.

## Visual Design

The SDL2 window shows an E-Ink-like off-white background with black and red pixels. The home screen uses 48 x 48 icons:

- Reading: open book.
- Weather: sun/cloud.
- Calendar: date page.
- Games: small joystick or grid.
- English: `A` plus card.
- Settings: gear.
- About: info circle.

Icons use black as the main shape and red for accents such as bookmark, sun, selected date, joystick button, or information dot.

Reader visual rules:

- Body text uses wrapped 20 px Chinese font.
- Page count stays in the top title bar.
- Bottom operation hint text and bottom progress bar are not rendered.
- Reader menu uses a white panel, red top accent, and inverse red highlight for the selected row.

## Testing

Automated tests continue through `make test` and do not require a visible SDL window. They cover:

- Icon assets draw distinct black/red pixels.
- App state transitions for all primary modules.
- Existing Chinese font and page rendering behavior.

SDL2 is verified by:

- `make reader_sim_sdl` build target.
- A dummy-video smoke command where available: `SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke`.

## Acceptance Criteria

- `make test` passes.
- `make reader_sim` builds.
- `make reader_sim_sdl` builds.
- `SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke` exits successfully.
- Running `./reader_sim_sdl` opens a scaled SDL2 window.
- Home page shows seven distinguishable pixel icons with Chinese labels.
- Keyboard input navigates home and opens every module.
- Each module renders meaningful Chinese content and at least one useful interaction.
- README documents SDL2 build/run commands.
- Timestamped snapshot capture includes both `reader.png` and `reader_menu.png`.
