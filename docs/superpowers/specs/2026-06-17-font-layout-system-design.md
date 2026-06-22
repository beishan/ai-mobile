# Font Layout System Design

Date: 2026-06-17

## Purpose

Replace the current fixed 16 x 16 text renderer with a font and layout system that better matches a real 400 x 300 E-Ink device. Text should be compact, readable, aligned, and usable across status bars, icon labels, lists, cards, and reader pages.

## Requirements

- Keep repository-contained bitmap font assets.
- Do not depend on SDL_ttf at runtime.
- Generate multiple font sizes for the simulator: 12, 14, 16, and 20 px.
- Store per-glyph metrics: width, height, advance, bitmap offset.
- Use proportional advances for ASCII, digits, punctuation, and Latin text.
- Use compact but readable advances for Chinese glyphs.
- Support text measurement, left/right/center alignment, and bounded text drawing.
- Support automatic wrapping for Chinese text and mixed ASCII/Chinese strings.
- Support ellipsis for text that exceeds a single-line width.
- Keep `make test` as the main verification command.

## Architecture

The generator creates one header per size under `assets/fonts/`:

- `sim_zh12.h`
- `sim_zh14.h`
- `sim_zh16.h`
- `sim_zh18.h`
- `sim_zh20.h`
- `sim_zh22.h`
- `sim_zh24.h`

Each header contains glyph metrics and bitmap bytes. Runtime `font.c` exposes `font_face_t` instances for each size and layout helpers:

- `font_get_face(font_size_t size)`
- `font_measure_text(...)`
- `font_draw_text(...)`
- `font_draw_text_aligned(...)`
- `font_draw_text_box(...)`
- `font_draw_ellipsis(...)`

Existing pages choose font sizes by UI role:

- Status bar: 12 px.
- Icon labels: 14 px.
- Lists and normal UI: 16 px.
- Reader body: 20 px.

## Layout Rules

- Status bar content must fit within 400 px without clipping.
- Home icon labels are centered under icons.
- Right status icons and battery text are right aligned.
- Reader text wraps inside margins and does not overlap menus.
- Reader normal page reserves the bottom area as blank reading whitespace; operation hints and bottom progress bar are not rendered.
- List rows use consistent baseline spacing.
- Text drawing clips to caller-provided boxes.

## Acceptance Criteria

- `make test` passes.
- Status bar renders without clipping.
- `14:35` uses compact digit spacing.
- Chinese labels are centered under home icons.
- Reader body uses wrapped 20 px Chinese text.
- Reader body can switch between 16, 18, 20, 22, and 24 px settings-driven font sizes.
- Reader body line spacing follows the settings module's spacing selection.
- Reader bottom hint/progress area stays blank.
- SDL2 smoke still passes.
- PPM snapshot remains available for visual inspection.
- Timestamped snapshot folders under `snapshots/` are available for visual tracking.
