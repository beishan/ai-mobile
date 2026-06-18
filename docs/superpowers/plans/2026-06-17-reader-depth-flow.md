# Reader Depth Flow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect bookshelf, reader progress, catalog jump, and bookmark state into one coherent simulator reading flow.

**Architecture:** Extend the existing `app_state_t` with fixed-size mock book arrays and a reader overlay mode for catalog. Keep button behavior in `src/app/app_state.c` and rendering in `src/ui/pages.c`. No new storage, networking, or page enum is required.

**Tech Stack:** C11, Make, repository-contained bitmap fonts, existing PPM and SDL2 simulators.

## Global Constraints

- Display remains exactly 400 x 300 pixels.
- Normal reader page bottom stays blank with no operation hint text and no bottom progress bar.
- Reader menu keeps `继续阅读 / 查看目录 / 添加书签 / 退出到书架`.
- Catalog is a reader overlay, not a new app page.
- `make test` remains the primary verification command.
- Snapshot output remains timestamped under `snapshots/YYYYMMDD-HHMMSS-label/`.

---

### Task 1: Mock Book State

**Files:**
- Modify: `src/app/app_state.h`
- Modify: `src/app/app_state.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Produces: `current_book`, `book_pages[3]`, `book_current_pages[3]`, `book_bookmark_pages[3]`, `reader_catalog_open`, `reader_catalog_selection`.

- [ ] **Step 1: Add failing tests**

Add tests that open the second book, turn a page, exit to bookshelf, and assert the current book/page are retained.

- [ ] **Step 2: Run red test**

Run `make test`; expected compile failure for missing state fields.

- [ ] **Step 3: Implement mock book state**

Initialize three mock books with page counts and per-book current pages. Opening a book sets `current_book` from `bookshelf_selection` and loads `reader_page` from `book_current_pages[current_book]`.

- [ ] **Step 4: Run green test**

Run `make test`; expected `tests passed`.

### Task 2: Reader Menu Catalog and Bookmark

**Files:**
- Modify: `src/app/app_state.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: fields from Task 1.
- Produces: catalog overlay behavior and per-book bookmark behavior.

- [ ] **Step 1: Add failing tests**

Add tests that select `查看目录`, navigate to chapter 2, HOME jump to page 2, and POWER from catalog returns to the reader menu. Add bookmark test for current book/page.

- [ ] **Step 2: Run red test**

Run `make test`; expected behavior assertions fail.

- [ ] **Step 3: Implement menu actions**

Menu item 1 opens catalog overlay. Menu item 2 writes `book_bookmark_pages[current_book] = reader_page`. Catalog UP/DOWN wraps through three chapters; HOME jumps to chapter start pages.

- [ ] **Step 4: Run green test**

Run `make test`; expected `tests passed`.

### Task 3: Render Bookshelf, Reader, Catalog

**Files:**
- Modify: `src/ui/pages.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: book state and catalog state from app.
- Produces: dynamic bookshelf progress, dynamic reader title/page count, catalog overlay.

- [ ] **Step 1: Add render smoke tests**

Add assertions that catalog overlay renders nonblank red/black content and normal reader bottom area remains blank.

- [ ] **Step 2: Implement render updates**

Render book progress from `book_current_pages/book_pages`, show selected book title in reader title bar, show `current/total`, show catalog overlay with selected row.

- [ ] **Step 3: Run verification**

Run `make test`, `make reader_sim`, `make reader_sim_sdl`, and `SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke`.

### Task 4: Docs and Snapshots

**Files:**
- Modify: `README.md`
- Modify: `docs/PROGRESS.md`
- Modify: `tools/capture_snapshots.py`

**Interfaces:**
- Consumes: implemented reader depth flow.
- Produces: timestamped visual tracking.

- [ ] **Step 1: Add catalog snapshot route**

Add `reader_catalog` capture path to `tools/capture_snapshots.py`.

- [ ] **Step 2: Generate snapshot set**

Run `python3 tools/capture_snapshots.py --label reader-depth-flow`.

- [ ] **Step 3: Update docs**

Record completed flow and latest snapshot folder in `docs/PROGRESS.md`.
