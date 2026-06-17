# ESP32 E-Ink Reader Desktop Simulator Design

Date: 2026-06-17

## Purpose

Build the first development milestone as a desktop C simulator for the ESP32 three-color E-Ink reader. The simulator validates the UI structure, button navigation, page state machine, and refresh behavior before hardware drivers are available.

The simulator is not the final firmware. It is a host-runnable prototype whose core page logic should be portable to ESP-IDF later.

## Scope

The first simulator version includes:

- A `reader_sim` executable that runs on the local development machine.
- A 400 x 300 virtual display with three logical colors: white, black, and red.
- Four simulated buttons: power, up, home, and down.
- A page state machine covering the main screens from `requires01.md` and `eink_ui_mockups.png`.
- Basic drawing primitives matching the future EPD abstraction: clear, pixel, line, rectangle, filled rectangle, text, and display commit.
- Static or semi-interactive screens for home, bookshelf, reader, weather, calendar, English learning, settings, and snake.
- A refresh counter/log so slow full-refresh behavior is visible during development.
- Host-side tests for navigation and core rendering/state behavior.

## Non-Goals

This milestone does not include:

- ESP-IDF, PlatformIO, or hardware-specific drivers.
- Real E-Ink SPI communication.
- Real SD card, SPIFFS, NVS, WiFi, SNTP, or weather API integration.
- Full text shaping, GBK/UTF-8 detection, EPUB parsing, or persistent book indexes.
- Real-time game timing.
- Pixel-perfect reproduction of the mockup image.

## Architecture

The code is organized so firmware-facing logic stays separate from host-only simulation:

- `src/main.c`: simulator entry point and input loop.
- `src/app/app_state.*`: current page, selection indexes, mock data, and button dispatch.
- `src/ui/page_*.c`: page renderers and page-local input handlers.
- `src/gfx/gfx.*`: device-independent drawing API for a 400 x 300 three-color framebuffer.
- `src/platform/sim_display.*`: host display adapter that exports the framebuffer in a viewable form.
- `tests/`: host tests for state transitions, button behavior, and selected rendering outputs.

The future ESP32 port should replace `platform/sim_display` with an EPD driver while keeping most `app`, `ui`, and `gfx` logic intact.

## Interaction Model

The simulator accepts simple keyboard input:

- `w`: up button
- `s`: down button
- `enter` or `h`: home button
- `p`: power button
- `q`: quit simulator

Initial navigation behavior:

- Home screen selection moves through the seven functions.
- Home opens the selected function.
- Power returns from most function pages to home.
- Reader bookshelf opens the selected mock book.
- Reader page supports previous/next page with up/down.
- Weather home forces a mock refresh with home.
- Calendar switches months with up/down.
- English card flips with home and moves through mock words with up/down.
- Settings moves through rows with up/down and toggles simple values with home.
- Snake moves one step per button event and commits one full refresh per move.

## Rendering Rules

The display model uses a small enum for logical colors:

- White: background and empty space.
- Black: primary text, borders, icons, and normal values.
- Red: selected items, warnings, today's date, food/target game elements, and other emphasis.

The first version may use a built-in bitmap or block text renderer rather than high-quality Chinese font rendering. UI layout should preserve the proportions and information hierarchy of the mockup: 20 px status/title bars, restrained red usage, and dense but readable 400 x 300 screens.

Each committed frame is written to an output artifact such as a PPM image under `out/frame.ppm`, and the simulator prints the current page plus refresh count. This keeps the simulator dependency-light and easy to verify in automated tests.

## Data Model

Mock data is compiled into the simulator:

- Books: three sample books with title, author, size, and progress.
- Reader pages: several sample text pages and a progress value.
- Weather: Beijing, current temperature, condition, three-day forecast, air quality.
- Calendar: one current month view with highlighted today and lunar summary.
- English: a small word list with phonetic, meaning, and example fields.
- Settings: font size, font family, line spacing, WiFi state, city, and power-saving toggle.
- Snake: board, snake body, food, score, and game-over flag.

Persistence is deliberately skipped in this milestone. State resets on each run.

## Error Handling

The simulator should handle invalid input by ignoring it and keeping the current page. Rendering functions should clip drawing operations to the 400 x 300 framebuffer bounds. Failed file output should return a non-zero process status and print an actionable error.

## Testing

Tests are written before implementation for behavior that can run without a display window:

- Home selection wraps and opens the expected page.
- Power returns function pages to home.
- Reader page increments and decrements within bounds.
- Settings toggles the power-saving value.
- Drawing primitives clip safely and place red/black pixels in expected positions.
- Display commit writes a valid PPM header and increments refresh count.

The initial test runner can be a small C test executable built with `make test` or an equivalent script, avoiding external dependencies.

## Acceptance Criteria

The milestone is complete when:

- `./reader_sim` runs locally and accepts keyboard button input.
- At least the eight mockup-inspired screens can be rendered.
- Navigation between home and each function page works.
- `out/frame.ppm` updates after committed screen refreshes.
- Host tests pass from a single command.
- The code layout keeps simulation-specific output separate from reusable UI and state logic.

## Deferred Decisions

- The first output format is PPM to avoid external dependencies; PNG export is deferred.
- Text rendering will start with a simple built-in renderer; high-quality Chinese bitmap fonts are deferred to the firmware or a later simulator pass.
