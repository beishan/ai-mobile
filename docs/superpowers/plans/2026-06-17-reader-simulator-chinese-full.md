# Chinese Full Reader Simulator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade `reader_sim` into a Chinese, full-feature desktop prototype that visually matches the final 400 x 300 three-color E-Ink reader more closely.

**Architecture:** Add a portable font layer between UTF-8 UI text and the existing framebuffer. Store generated bitmap glyph data in the repository, draw Chinese through `font_draw_text`, and keep page/state code hardware-agnostic for later ESP32 migration.

**Tech Stack:** C11, Make, standard C library, repository-contained bitmap font data, Python 3 + Pillow only for regenerating font assets.

## Global Constraints

- Chinese display is mandatory.
- ASCII-only labels or block-only glyph substitutes are not acceptable for Chinese UI.
- Display size is exactly 400 x 300 pixels.
- Logical colors are white, black, and red.
- Red is reserved for selection, warning, today marker, progress emphasis, and game targets.
- `make test` remains the single test command.
- `make reader_sim` builds the simulator executable.
- Running `./reader_sim` produces `out/frame.ppm`.
- No ESP-IDF, PlatformIO, SPI, SD, WiFi, SNTP, NVS, or HTTP weather integration in this phase.
- State resets on each simulator run.

---

## File Structure

- Create `src/font/font.h`: UTF-8 decoder, font structs, measurement, and drawing API.
- Create `src/font/font.c`: glyph lookup, UTF-8 decoding, text measurement, and glyph drawing.
- Create `assets/fonts/sim_zh16.h`: generated repository-contained 16 px bitmap glyph data.
- Create `tools/generate_font.py`: optional generator for rebuilding `sim_zh16.h` from a local Chinese font.
- Modify `src/ui/pages.c`: convert all visible UI labels/content to Chinese and use `font_draw_text`.
- Modify `src/ui/pages.h`: pass the loaded font to page rendering.
- Modify `src/main.c`: load font before rendering and exit clearly on failure.
- Modify `tests/test_runner.c`: add UTF-8/font/rendering behavior tests.
- Modify `Makefile`: compile `src/font/font.c` into tests and simulator.
- Modify `README.md`: document Chinese simulator output and font asset regeneration.

---

### Task 1: UTF-8 Decoder and Font API

**Files:**
- Create: `src/font/font.h`
- Create: `src/font/font.c`
- Modify: `tests/test_runner.c`
- Modify: `Makefile`

**Interfaces:**
- Produces: `font_t`, `font_glyph_t`, `font_load_default`, `font_free`, `font_decode_utf8`, `font_measure_text`, `font_draw_text`
- Consumes: `gfx_framebuffer_t`, `gfx_color_t`, `gfx_fill_rect`

- [ ] **Step 1: Write failing UTF-8 tests**

Add tests to `tests/test_runner.c`:

```c
#include "font/font.h"

static void test_utf8_decoder_reads_ascii_and_chinese(void) {
    const unsigned char *p = (const unsigned char *)"A阅";
    uint32_t cp = 0;
    ASSERT_EQ_INT(1, font_decode_utf8(&p, &cp));
    ASSERT_EQ_INT('A', (int)cp);
    ASSERT_EQ_INT(1, font_decode_utf8(&p, &cp));
    ASSERT_EQ_INT(0x9605, (int)cp);
}

static void test_utf8_decoder_replaces_invalid_bytes(void) {
    const unsigned char bytes[] = {0xff, 0x00};
    const unsigned char *p = bytes;
    uint32_t cp = 0;
    ASSERT_EQ_INT(1, font_decode_utf8(&p, &cp));
    ASSERT_EQ_INT(0xfffd, (int)cp);
}
```

Call both tests from `main`.

- [ ] **Step 2: Run red test**

Run:

```bash
make test
```

Expected: compile failure because `font/font.h` does not exist.

- [ ] **Step 3: Implement minimal font API**

Create `src/font/font.h` and `src/font/font.c`. Implement UTF-8 decoding for 1, 2, 3, and 4 byte sequences. Add stub font loading that returns a static `font_t` with no glyphs yet. Implement measurement and drawing with replacement boxes when glyphs are missing.

- [ ] **Step 4: Run green test**

Run:

```bash
make test
```

Expected: `tests passed`

- [ ] **Step 5: Commit**

```bash
git add Makefile src/font/font.h src/font/font.c tests/test_runner.c
git commit -m "feat: add utf8 font api"
```

---

### Task 2: Repository Bitmap Font Asset

**Files:**
- Create: `tools/generate_font.py`
- Create: `assets/fonts/sim_zh16.h`
- Modify: `src/font/font.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: `font_load_default`
- Produces: glyph lookup for Chinese codepoints including `阅`, `读`, `天`, `气`, `日`, `历`, `游`, `戏`, `英`, `语`, `设`, `置`, `关`, `于`, `三`, `体`, `北`, `京`

- [ ] **Step 1: Write failing glyph lookup/render test**

Add tests:

```c
static void test_default_font_has_chinese_glyphs(void) {
    font_t font;
    ASSERT_EQ_INT(1, font_load_default(&font));
    ASSERT_TRUE(font_find_glyph(&font, 0x9605) != NULL);
    font_free(&font);
}

static void test_font_draw_text_places_pixels_for_chinese(void) {
    gfx_framebuffer_t fb;
    font_t font;
    gfx_init(&fb);
    ASSERT_EQ_INT(1, font_load_default(&font));
    font_draw_text(&font, &fb, 10, 10, "阅读", GFX_BLACK);
    ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 20);
    font_free(&font);
}
```

- [ ] **Step 2: Run red test**

Run:

```bash
make test
```

Expected: tests fail because `font_find_glyph` or generated glyphs are not available.

- [ ] **Step 3: Generate and commit font asset**

Create `tools/generate_font.py` that renders the fixed glyph set using `/System/Library/Fonts/PingFang.ttc` at 16 px through Pillow and writes `assets/fonts/sim_zh16.h`. The generated header defines `sim_zh16_glyphs[]`, `sim_zh16_glyph_count`, `SIM_ZH16_WIDTH`, and `SIM_ZH16_HEIGHT`.

- [ ] **Step 4: Wire font asset into runtime**

Update `font_load_default` and `font_find_glyph` to expose generated glyphs. Missing glyphs still draw replacement boxes.

- [ ] **Step 5: Run green test**

Run:

```bash
make test
```

Expected: `tests passed`

- [ ] **Step 6: Commit**

```bash
git add tools/generate_font.py assets/fonts/sim_zh16.h src/font/font.c src/font/font.h tests/test_runner.c
git commit -m "feat: add chinese bitmap font asset"
```

---

### Task 3: Chinese Text Rendering Integration

**Files:**
- Modify: `src/ui/pages.h`
- Modify: `src/ui/pages.c`
- Modify: `src/main.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: `font_t`, `font_draw_text`
- Produces: `ui_render_page(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font)`

- [ ] **Step 1: Write failing Chinese home render test**

Update UI tests to load `font_t` and call the new render signature:

```c
static void test_home_render_uses_chinese_font_pixels(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;
    gfx_init(&fb);
    app_init(&app);
    ASSERT_EQ_INT(1, font_load_default(&font));
    ui_render_page(&fb, &app, &font);
    ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 500);
    ASSERT_TRUE(count_color(&fb, GFX_RED) > 100);
    font_free(&font);
}
```

- [ ] **Step 2: Run red test**

Run:

```bash
make test
```

Expected: compile failure because `ui_render_page` still has the old signature.

- [ ] **Step 3: Update render signatures and main**

Change `ui_render_page` to receive `const font_t *font`. Replace page text helper calls with `font_draw_text`. Load the default font in `main` and pass it to rendering.

- [ ] **Step 4: Run green test**

Run:

```bash
make test
```

Expected: `tests passed`

- [ ] **Step 5: Commit**

```bash
git add src/ui/pages.h src/ui/pages.c src/main.c tests/test_runner.c
git commit -m "feat: render simulator pages with chinese font"
```

---

### Task 4: Chinese Pages and Module Interactions

**Files:**
- Modify: `src/ui/pages.c`
- Modify: `src/app/app_state.h`
- Modify: `src/app/app_state.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: `font_draw_text`, existing app state
- Produces: Chinese mock pages with richer state for reader, weather, calendar, English, settings, and snake

- [ ] **Step 1: Write failing interaction tests**

Add tests:

```c
static void test_weather_refresh_changes_counter(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_WEATHER;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.weather_refreshes);
}

static void test_english_flip_changes_state(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_ENGLISH;
    ASSERT_EQ_INT(0, app.english_show_back);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.english_show_back);
}

static void test_snake_movement_changes_position(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_SNAKE;
    int y = app.snake_y;
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_TRUE(app.snake_y < y);
}
```

- [ ] **Step 2: Run red test**

Run:

```bash
make test
```

Expected: if current behavior already passes, add render-state assertions for Chinese pages before proceeding.

- [ ] **Step 3: Update page content**

Replace all visible English strings in page renderers with Chinese text from the design spec. Use Chinese book data, Chinese weather data, Chinese settings labels, Chinese reader paragraphs, and Chinese operation hints. Keep red usage limited to semantic highlights.

- [ ] **Step 4: Run green test**

Run:

```bash
make test
```

Expected: `tests passed`

- [ ] **Step 5: Commit**

```bash
git add src/ui/pages.c src/app/app_state.h src/app/app_state.c tests/test_runner.c
git commit -m "feat: add chinese simulator module flows"
```

---

### Task 5: Documentation and Final Verification

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: completed simulator
- Produces: Chinese simulator usage docs

- [ ] **Step 1: Update README**

Document that generated frames contain Chinese UI, that the font asset is tracked, and that `tools/generate_font.py` can regenerate `assets/fonts/sim_zh16.h` on macOS with Pillow.

- [ ] **Step 2: Run final verification**

Run:

```bash
make clean
make test
make reader_sim
printf 's\nh\np\nq\n' | ./reader_sim
```

Expected: tests pass, executable builds, simulator opens weather through Chinese Home selection, returns Home, exits, and writes `out/frame.ppm`.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: document chinese simulator"
```

- [ ] **Step 4: Push**

```bash
git push
```

Expected: branch `feature/reader-simulator` is updated on GitHub.

---

## Self-Review

- Spec coverage: covers Chinese rendering, repository font assets, page conversion, interaction scope, testing, README, and final PPM output.
- Scope: keeps hardware, networking, real storage, persistence, GBK, EPUB, real lunar calendar, and production typography deferred.
- Gap scan: all task steps name concrete files, APIs, commands, and expected results.
- Type consistency: `font_t`, `font_glyph_t`, `font_load_default`, `font_find_glyph`, `font_decode_utf8`, `font_measure_text`, `font_draw_text`, and the new `ui_render_page` signature are used consistently.
