# Chinese Full Reader Simulator Design

Date: 2026-06-17

## Purpose

Extend the desktop simulator from a structural prototype into a Chinese, full-feature UI prototype that closely represents the final 400 x 300 three-color E-Ink reader experience.

This phase keeps the project on the host simulator. It does not start ESP-IDF hardware work yet. The simulator becomes the behavior and visual reference for later firmware implementation.

## Core Requirement

Chinese display is mandatory. The simulator must render readable Chinese UI text and sample reading content. ASCII-only labels or block-only glyph substitutes are not acceptable for Chinese UI.

The visual output should match the final device constraints:

- 400 x 300 pixel canvas.
- Three logical colors only: white, black, and red.
- E-Ink-like restrained layout with dense, readable information.
- Red reserved for selection, warning, today marker, progress emphasis, and game targets.
- Full-refresh behavior represented by frame commits and refresh count.

## Scope

This phase includes:

- A bitmap font pipeline/resource that supports the Chinese characters used by simulator screens and sample reading text.
- A text renderer that can draw UTF-8 Chinese and ASCII into the three-color framebuffer.
- Chinese versions of all existing simulator pages: home, bookshelf, reader, weather, calendar, English learning, settings, snake, and about.
- More complete simulator interactions for the major modules.
- Mock data that looks like final product data, including Chinese book names, weather, calendar summaries, settings labels, and reading text.
- Tests for UTF-8 text rendering, state transitions, and selected module behaviors.
- Documentation for building, running, and inspecting the generated frame.

## Non-Goals

This phase does not include:

- ESP-IDF or PlatformIO project creation.
- Real E-Ink SPI driver implementation.
- Real SD card, SPIFFS, NVS, WiFi, SNTP, or HTTP weather calls.
- Full GBK detection.
- Full EPUB extraction.
- Production-grade Chinese typography.
- Real lunar calendar algorithm.
- Persisting state across runs.

## Font Strategy

The simulator will use a repository-contained bitmap font resource instead of system fonts. This makes output deterministic and keeps the project portable.

The first implementation will include a compact generated bitmap font for the exact glyph set needed by current simulator screens and sample data. The glyph resource will live under `assets/fonts/` and be loaded by the simulator at startup.

The font layer will expose a small API:

- `font_load(...)`
- `font_free(...)`
- `font_measure_text(...)`
- `font_draw_text(...)`

The renderer will decode UTF-8 to Unicode code points, look up glyph bitmaps, and draw missing glyphs as a visible replacement box. ASCII remains supported.

Later firmware can replace the file-backed loader with SPIFFS or compiled binary font data while keeping the same text-rendering API shape.

## UI Design

The current page renderers will be converted from English labels to Chinese layouts:

- Home: status bar with `14:35`, `晴 26°C 北京`, `WiFi`, and `78%`; grid labels `阅读 / 天气 / 日历 / 游戏 / 英语 / 设置 / 关于`.
- Bookshelf: Chinese titles such as `三体`, `百年孤独`, `活着`; author, size, and progress.
- Reader: Chinese title/chapter bar and real sample Chinese paragraphs with page progress.
- Weather: `北京`, `26°C`, `晴转多云`, humidity/wind, three-day forecast, and air quality.
- Calendar: `2025年6月`, weekday headers, today marker, weekend red text, and lunar summary.
- English: Chinese module labels with English word, phonetic text, Chinese meaning, and example sentence.
- Settings: Chinese setting labels and values.
- Snake: Chinese title, score, red food, and operation hint.
- About: firmware/version/system information in Chinese.

The simulator should remain visually close to the mockup but can improve spacing, hierarchy, and content density where needed.

## Interaction Scope

This phase makes the simulator feel like a usable device:

- Home selection opens every module.
- Power returns from modules to home.
- Bookshelf selection opens the reader.
- Reader supports previous/next page and progress updates.
- Weather supports a manual mock refresh.
- Calendar supports previous/next month offset.
- English card flips between front and back, and moves through sample words.
- Settings moves through rows and toggles simple values.
- Snake moves one turn per button event and updates the board.
- About remains a static information page.

Push-box and Sudoku are not implemented as full games in this phase. The game module may keep snake as the interactive game and list the other games as disabled or preview rows.

## Data Model

Mock data remains compiled in or loaded from simple local assets:

- Chinese book metadata.
- Sample Chinese reader pages.
- Weather mock values.
- Calendar month display data.
- English word list with Chinese meanings.
- Settings rows and values.
- Snake board state.

State still resets on each simulator run. Persistence is deferred until the firmware/storage phase.

## Error Handling

- If the font resource cannot load, `reader_sim` exits with an actionable error message.
- Missing glyphs render as replacement boxes and do not crash rendering.
- Invalid UTF-8 bytes render as replacement boxes.
- Drawing operations remain clipped to the framebuffer.
- Failed frame output returns a non-zero process status.

## Testing

Tests should cover:

- UTF-8 decoding for ASCII, common Chinese characters, and invalid byte sequences.
- Font lookup for included Chinese glyphs.
- Rendering Chinese text places black or red pixels in expected regions.
- Home page renders Chinese labels and red selected state.
- Reader next/previous page stays within bounds.
- Weather manual refresh increments mock timestamp/count.
- English card flip changes rendered state.
- Settings toggle changes value.
- Snake movement changes board position.
- PPM output still writes a valid frame.

The project continues to use `make test` as the single verification command.

## Acceptance Criteria

The phase is complete when:

- `make test` passes.
- `make reader_sim` builds `reader_sim`.
- Running `./reader_sim` produces `out/frame.ppm`.
- The generated frame contains readable Chinese text.
- All primary pages render Chinese UI text.
- Main interactions listed in this spec work through `w`, `s`, `h`/Enter, and `p`.
- Font assets required for Chinese rendering are tracked in the repository.
- README documents the Chinese simulator and frame inspection flow.

## Deferred Decisions

- The firmware font format can be optimized later.
- GBK input, TXT parsing, EPUB parsing, real weather, real calendar algorithms, and persistence are deferred.
- A windowed preview is deferred; PPM remains the deterministic output format for this phase.
