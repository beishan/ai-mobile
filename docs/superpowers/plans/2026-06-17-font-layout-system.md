# Font Layout System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add multi-size bitmap fonts with metrics and layout helpers for compact, realistic 400 x 300 E-Ink UI text.

**Architecture:** Regenerate font headers with glyph metrics and bitmaps. Refactor `font` runtime around `font_face_t`, then migrate page rendering to role-specific font sizes and alignment helpers.

**Tech Stack:** C11, Make, Python 3 + Pillow for generated assets, repository-contained bitmap font headers.

## Current Implementation Status - 2026-06-17 18:12

- Multi-size font assets and metrics are implemented for 12, 14, 16, 18, 20, 22, and 24 px.
- Status bar uses compact 12 px text to avoid clipped time/weather/battery percentage.
- Home icon labels use centered 14 px text under image-like icons.
- Reader body uses wrapped 20 px Chinese text.
- Reader body now follows settings-controlled font size and line spacing.
- Reader bottom operation hint text and progress bar have been removed; the lower reading area stays blank.
- Snapshot capture stores timestamped folders under `snapshots/` for visual regression tracking.

## Global Constraints

- Runtime must not depend on SDL_ttf.
- Generated font assets must be committed.
- `make test` must pass.
- SDL2 smoke must pass.

---

### Task 1: Font Metrics Assets

- Generate 12, 14, 16, and 20 px font headers.
- Include per-glyph `width`, `height`, `advance`, and `offset`.
- Test default font faces exist and ASCII advance is narrower than Chinese advance.

### Task 2: Runtime Layout API

- Replace fixed `font_t` assumptions with `font_face_t`.
- Add aligned drawing, box drawing, and ellipsis helpers.
- Test measurement, center alignment, right alignment, and wrapping behavior.

### Task 3: UI Migration

- Migrate status bar to 12 px font.
- Migrate home icon labels to centered 14 px.
- Migrate normal pages to 16 px.
- Migrate reader body to wrapped 20 px.
- Verify home/status snapshot no longer clips or overlaps.

### Task 4: Verification

- Run `make test`.
- Run `make reader_sim`.
- Run `make reader_sim_sdl`.
- Run `SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke`.
- Export `out/frame.png` for visual inspection.

## Self-Review

This plan focuses on font and layout only. It preserves SDL2, PPM, Chinese UI, and existing module flows.
