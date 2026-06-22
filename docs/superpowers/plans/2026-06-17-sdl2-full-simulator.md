# SDL2 Full Reader Simulator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an SDL2 windowed simulator with Chinese UI, distinguishable pixel app icons, and richer module interactions.

**Architecture:** Keep `gfx`, `font`, `app`, and `ui` portable. Add `platform/sdl_display` for SDL2 display/input, `ui/icons` for three-color pixel icons, and `main_sdl.c` for the event loop.

**Tech Stack:** C11, Make, SDL2 2.32.10, repository-contained bitmap font/icon data.

## Current Implementation Status - 2026-06-17 18:12

- `reader_sim_sdl` is implemented and verified with dummy-video smoke mode.
- Home page icons are drawn as distinguishable three-color pixel icons with no outer tile frames.
- Home layout is currently 4 columns and supports future multi-screen expansion.
- Status bar has compact Chinese/number text, weather after time, signal-style WiFi icon, and complete battery display.
- Reader page now keeps page count in the title bar and removes the bottom hint/progress area.
- Reader menu supports selectable actions, bookmark feedback, POWER cancel, and exit to bookshelf.
- Snapshot workflow captures timestamped visual sets including `reader.png` and `reader_menu.png`.

## Current Implementation Status - 2026-06-18 10:49

- Snapshot workflow now stores all generated PNGs in timestamped folders and includes running/game-over Snake states.
- Reader, weather, calendar, English, settings, and game pages use generated Chinese bitmap font assets.
- Game module now has a split list/running/game-over state flow for Snake.
- Snake runs as an E-Ink-friendly step game: UP/DOWN changes vertical direction and moves once, HOME advances once, red food adds score, and wall collision ends the run.
- Push-box and Sudoku remain selectable preview rows for later deeper implementation.

## Global Constraints

- `reader_sim_sdl` is the primary SDL2 simulator executable.
- Existing `reader_sim` and PPM output remain available.
- Home screen apps use image-like pixel icons.
- Chinese display remains mandatory.
- SDL2 automated smoke uses `SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke`.
- `make test` remains the no-window unit test command.

---

### Task 1: SDL2 Platform Layer

**Files:**
- Create: `src/platform/sdl_display.h`
- Create: `src/platform/sdl_display.c`
- Create: `src/main_sdl.c`
- Modify: `Makefile`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Produces: `sdl_display_t`, `sdl_display_init`, `sdl_display_present`, `sdl_display_poll_button`, `sdl_display_shutdown`
- Consumes: `gfx_framebuffer_t`, `app_button_t`

Steps:
- [x] Add a failing compile test by referencing `sdl_display.h` from tests.
- [x] Run `make test`; expect compile failure.
- [x] Implement SDL2 display and event mapping.
- [x] Add `reader_sim_sdl` target using `sdl2-config --cflags --libs`.
- [x] Run `make test`, `make reader_sim_sdl`, and `SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke`.
- [ ] Commit with `feat: add sdl2 simulator window`.

### Task 2: Pixel Icons

**Files:**
- Create: `src/ui/icons.h`
- Create: `src/ui/icons.c`
- Modify: `Makefile`
- Modify: `tests/test_runner.c`
- Modify: `src/ui/pages.c`

**Interfaces:**
- Produces: `ui_icon_kind_t`, `ui_draw_icon`
- Consumes: `gfx_fill_rect`, `gfx_draw_rect`

Steps:
- [x] Add tests that each icon draws black pixels and at least one icon draws red accent pixels.
- [x] Run `make test`; expect compile failure because `ui/icons.h` is missing.
- [x] Implement seven 48 x 48 icon drawing routines: reading, weather, calendar, games, English, settings, about.
- [x] Use icons on the home screen above Chinese labels.
- [x] Run `make test`.
- [ ] Commit with `feat: add home pixel app icons`.

### Task 3: Richer Module Details

**Files:**
- Modify: `src/app/app_state.h`
- Modify: `src/app/app_state.c`
- Modify: `src/ui/pages.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: existing button dispatch and render API.
- Produces: richer app states for reader menu, settings values, game list previews, and module details.

Steps:
- [x] Add tests for opening every home module, settings value cycling, calendar month switching, reader menu toggle, and snake movement.
- [x] Run `make test`; expect any missing behavior to fail.
- [x] Implement missing state fields and button handling.
- [x] Render reader menu, game list previews, richer about/settings/weather details.
- [x] Run `make test`.
- [ ] Commit with `feat: enrich simulator app modules`.

### Task 4: Docs and Verification

**Files:**
- Modify: `README.md`

Steps:
- [x] Document `make reader_sim_sdl`, SDL2 controls, smoke command, and icon behavior.
- [ ] Run `make clean`, `make test`, `make reader_sim`, `make reader_sim_sdl`, `SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke`.
- [ ] Commit with `docs: document sdl2 simulator`.
- [ ] Push branch.

## Self-Review

- Covers SDL2 primary simulator, icon assets, Chinese UI, richer module details, tests, docs, and smoke verification.
- Keeps hardware, networking, storage, and persistence deferred.
- Keeps PPM simulator available.
