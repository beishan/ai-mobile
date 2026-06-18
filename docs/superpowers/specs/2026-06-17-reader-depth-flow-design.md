# Reader Depth Flow Design

Date: 2026-06-17

## Purpose

Deepen the simulator reading module so bookshelf, reader, menu, catalog, and bookmark state work as one flow. This remains simulator-only mock state; SD card storage and persistent files are deferred.

## Requirements

- Bookshelf opens the selected book instead of always resetting to a single fixed book.
- Each mock book has its own title, author, page count, current page, chapter mapping, and bookmark page.
- Reading up/down changes the current book page and the bookshelf progress shown when returning.
- Reader title and page count reflect the selected book.
- Reader menu keeps the existing four items: `继续阅读 / 查看目录 / 添加书签 / 退出到书架`.
- Selecting `查看目录` opens a catalog overlay from the reader menu.
- Catalog overlay lists chapters for the selected book, supports up/down selection, HOME jumps to that chapter page, and POWER returns to the reader menu.
- Selecting `添加书签` records the current page for the selected book and changes the menu text to `已加书签`.
- Normal reader page bottom remains blank: no operation hint text and no bottom progress bar.
- Timestamped snapshots continue to include reader body and reader menu pages.

## Architecture

Extend `app_state_t` with fixed-size arrays for mock book state. Keep all behavior in `src/app/app_state.c` and all rendering in `src/ui/pages.c`, matching the current project structure. Catalog is represented as a reader overlay rather than a new top-level page, so SDL2 and PPM simulators keep the same page enum.

## Testing

Add tests to `tests/test_runner.c` for:

- Opening different books preserves selected book and its current page.
- Reading down updates the selected book progress.
- Exiting reader returns to bookshelf with progress retained.
- Adding a bookmark records the current page for the selected book.
- Catalog opens from reader menu, moves selection, and jumps to chapter page.
- Reader bottom area stays blank in normal reading mode.

## Non-Goals

- SD/NVS persistence.
- Real TXT parsing or EPUB extraction.
- More than the compiled-in three mock books.
- A separate full catalog page outside the reader overlay.

## Acceptance Criteria

- `make test` passes.
- `make reader_sim` builds.
- `make reader_sim_sdl` builds.
- `SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke` exits successfully.
- `python3 tools/capture_snapshots.py --label reader-depth-flow` creates timestamped snapshots including reader and reader menu.
