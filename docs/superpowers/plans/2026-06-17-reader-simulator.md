# Reader Simulator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a dependency-light C desktop simulator for the 400 x 300 three-color ESP32 E-Ink reader UI.

**Architecture:** The simulator separates portable UI/state code from host-only display output. `gfx` owns the framebuffer and drawing primitives, `app` owns page state and button dispatch, `ui` renders pages, and `platform` writes the framebuffer to `out/frame.ppm`.

**Tech Stack:** C11, Make, standard C library, PPM image output, no external runtime dependencies.

## Global Constraints

- Display size is exactly 400 x 300 pixels.
- Logical colors are white, black, and red.
- The executable is named `reader_sim`.
- Output frame path is `out/frame.ppm`.
- Simulated buttons are power, up, home, and down.
- Tests must run from a single `make test` command.
- No ESP-IDF, PlatformIO, SPI, SD, WiFi, SNTP, NVS, or API integration in this milestone.
- Mock data is compiled in and resets on each run.

---

## File Structure

- Create `Makefile`: build `reader_sim`, build `tests/test_runner`, run tests, and clean build outputs.
- Create `src/gfx/gfx.h`: public framebuffer, color enum, and drawing API.
- Create `src/gfx/gfx.c`: clipping-safe drawing primitives and simple block text rendering.
- Create `src/platform/sim_display.h`: public display commit API.
- Create `src/platform/sim_display.c`: create `out/`, write `out/frame.ppm`, and count refreshes.
- Create `src/app/app_state.h`: page enum, button enum, app state struct, and app API.
- Create `src/app/app_state.c`: state initialization and button dispatch.
- Create `src/ui/pages.h`: render entry point for all pages.
- Create `src/ui/pages.c`: home, bookshelf, reader, weather, calendar, English, settings, and snake renderers.
- Create `src/main.c`: terminal input loop for the simulator.
- Create `tests/test_runner.c`: minimal C test harness for core behavior.

---

### Task 1: Build System and Framebuffer Primitives

**Files:**
- Create: `Makefile`
- Create: `src/gfx/gfx.h`
- Create: `src/gfx/gfx.c`
- Create: `tests/test_runner.c`

**Interfaces:**
- Produces: `gfx_color_t`, `gfx_framebuffer_t`, `gfx_init`, `gfx_clear`, `gfx_width`, `gfx_height`, `gfx_get_pixel`, `gfx_set_pixel`, `gfx_fill_rect`, `gfx_draw_rect`, `gfx_draw_text`
- Consumes: standard C library only

- [ ] **Step 1: Write failing framebuffer tests**

Add `tests/test_runner.c` with tests that expect the future API:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/gfx.h"

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ_INT(expected, actual) ASSERT_TRUE((expected) == (actual))

static void test_framebuffer_has_fixed_eink_size(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    ASSERT_EQ_INT(400, gfx_width(&fb));
    ASSERT_EQ_INT(300, gfx_height(&fb));
}

static void test_set_pixel_clips_out_of_bounds(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);
    gfx_set_pixel(&fb, -1, -1, GFX_RED);
    gfx_set_pixel(&fb, 400, 300, GFX_RED);
    gfx_set_pixel(&fb, 20, 30, GFX_BLACK);
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 0, 0));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 20, 30));
}

static void test_rectangles_clip_and_place_red_pixels(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);
    gfx_fill_rect(&fb, 398, 298, 10, 10, GFX_RED);
    ASSERT_EQ_INT(GFX_RED, gfx_get_pixel(&fb, 399, 299));
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 397, 297));
}

int main(void) {
    test_framebuffer_has_fixed_eink_size();
    test_set_pixel_clips_out_of_bounds();
    test_rectangles_clip_and_place_red_pixels();
    puts("tests passed");
    return 0;
}
```

- [ ] **Step 2: Add build targets and run red test**

Add `Makefile` with `make test` compiling `tests/test_runner.c`. Run:

```bash
make test
```

Expected: FAIL at compile time because `gfx/gfx.h` does not exist.

- [ ] **Step 3: Implement minimal framebuffer API**

Create `src/gfx/gfx.h` and `src/gfx/gfx.c` with a 400 x 300 in-memory pixel buffer, bounds-checked `get/set`, clipped fill rectangle, outline rectangle, and simple text that draws 5 x 7 block cells for visible ASCII characters.

- [ ] **Step 4: Run green test**

Run:

```bash
make test
```

Expected: `tests passed`

- [ ] **Step 5: Commit**

```bash
git add Makefile src/gfx/gfx.h src/gfx/gfx.c tests/test_runner.c
git commit -m "feat: add simulator framebuffer primitives"
```

---

### Task 2: PPM Display Commit

**Files:**
- Modify: `Makefile`
- Create: `src/platform/sim_display.h`
- Create: `src/platform/sim_display.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: `gfx_framebuffer_t` from Task 1
- Produces: `sim_display_t`, `sim_display_init`, `sim_display_commit`, `sim_display_refresh_count`

- [ ] **Step 1: Write failing PPM commit test**

Extend `tests/test_runner.c` with:

```c
#include "platform/sim_display.h"

static void test_display_commit_writes_ppm_and_counts_refresh(void) {
    gfx_framebuffer_t fb;
    sim_display_t display;
    char header[32] = {0};
    FILE *file;

    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);
    gfx_set_pixel(&fb, 0, 0, GFX_RED);
    sim_display_init(&display, "out/test_frame.ppm");

    ASSERT_EQ_INT(0, sim_display_refresh_count(&display));
    ASSERT_EQ_INT(0, sim_display_commit(&display, &fb));
    ASSERT_EQ_INT(1, sim_display_refresh_count(&display));

    file = fopen("out/test_frame.ppm", "rb");
    ASSERT_TRUE(file != NULL);
    ASSERT_TRUE(fread(header, 1, 15, file) > 0);
    fclose(file);
    ASSERT_TRUE(strncmp(header, "P6\n400 300\n255\n", 15) == 0);
}
```

- [ ] **Step 2: Run red test**

Run:

```bash
make test
```

Expected: FAIL at compile time because `platform/sim_display.h` does not exist.

- [ ] **Step 3: Implement PPM output**

Create the display adapter. `sim_display_commit` creates `out/` when needed, writes binary PPM `P6\n400 300\n255\n`, maps white to RGB 245/244/236, black to 35/35/32, red to 230/72/64, and increments refresh count only after a successful write.

- [ ] **Step 4: Run green test**

Run:

```bash
make test
```

Expected: `tests passed`

- [ ] **Step 5: Commit**

```bash
git add Makefile src/platform/sim_display.h src/platform/sim_display.c tests/test_runner.c
git commit -m "feat: add simulator ppm display output"
```

---

### Task 3: App State and Button Dispatch

**Files:**
- Create: `src/app/app_state.h`
- Create: `src/app/app_state.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: no simulator display output
- Produces: `app_page_t`, `app_button_t`, `app_state_t`, `app_init`, `app_handle_button`, `app_page_name`

- [ ] **Step 1: Write failing state transition tests**

Extend `tests/test_runner.c` with:

```c
#include "app/app_state.h"

static void test_home_selection_wraps_and_opens_weather(void) {
    app_state_t app;
    app_init(&app);
    ASSERT_EQ_INT(APP_PAGE_HOME, app.page);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.home_selection);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_WEATHER, app.page);
}

static void test_power_returns_function_page_to_home(void) {
    app_state_t app;
    app_init(&app);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_POWER);
    ASSERT_EQ_INT(APP_PAGE_HOME, app.page);
}

static void test_reader_page_bounds(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;
    app.reader_page = 0;
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(0, app.reader_page);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.reader_page);
}

static void test_settings_toggles_power_saving(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_SETTINGS;
    app.settings_selection = 5;
    ASSERT_EQ_INT(1, app.power_saving_enabled);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(0, app.power_saving_enabled);
}
```

- [ ] **Step 2: Run red test**

Run:

```bash
make test
```

Expected: FAIL at compile time because `app/app_state.h` does not exist.

- [ ] **Step 3: Implement app state**

Implement page enum values for home, bookshelf, reader, weather, calendar, English, settings, snake, and about. Implement button behavior from the design: home selection wraps through seven main items, home opens selected page, power returns to home, reader page clamps between 0 and 4, settings row 5 toggles power saving, and invalid input leaves state unchanged.

- [ ] **Step 4: Run green test**

Run:

```bash
make test
```

Expected: `tests passed`

- [ ] **Step 5: Commit**

```bash
git add src/app/app_state.h src/app/app_state.c tests/test_runner.c Makefile
git commit -m "feat: add reader simulator app state"
```

---

### Task 4: Page Rendering

**Files:**
- Create: `src/ui/pages.h`
- Create: `src/ui/pages.c`
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: `gfx_framebuffer_t`, `app_state_t`
- Produces: `ui_render_page(gfx_framebuffer_t *fb, const app_state_t *app)`

- [ ] **Step 1: Write failing rendering smoke tests**

Extend `tests/test_runner.c` with:

```c
#include "ui/pages.h"

static int count_color(const gfx_framebuffer_t *fb, gfx_color_t color) {
    int count = 0;
    for (int y = 0; y < gfx_height(fb); y++) {
        for (int x = 0; x < gfx_width(fb); x++) {
            if (gfx_get_pixel(fb, x, y) == color) {
                count++;
            }
        }
    }
    return count;
}

static void test_home_render_uses_black_and_red(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    gfx_init(&fb);
    app_init(&app);
    ui_render_page(&fb, &app);
    ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 100);
    ASSERT_TRUE(count_color(&fb, GFX_RED) > 100);
}

static void test_each_primary_page_renders_nonblank(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    app_page_t pages[] = {
        APP_PAGE_HOME,
        APP_PAGE_BOOKSHELF,
        APP_PAGE_READER,
        APP_PAGE_WEATHER,
        APP_PAGE_CALENDAR,
        APP_PAGE_ENGLISH,
        APP_PAGE_SETTINGS,
        APP_PAGE_SNAKE
    };
    app_init(&app);
    for (size_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
        gfx_init(&fb);
        app.page = pages[i];
        ui_render_page(&fb, &app);
        ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 50);
    }
}
```

- [ ] **Step 2: Run red test**

Run:

```bash
make test
```

Expected: FAIL at compile time because `ui/pages.h` does not exist.

- [ ] **Step 3: Implement page renderers**

Implement a shared status/title bar helper and render all eight pages with mockup-inspired layouts. Use red for selected home tile, selected bookshelf row, reader progress, low forecast temperature, today marker, English progress dots, settings active row, and snake food.

- [ ] **Step 4: Run green test**

Run:

```bash
make test
```

Expected: `tests passed`

- [ ] **Step 5: Commit**

```bash
git add src/ui/pages.h src/ui/pages.c tests/test_runner.c Makefile
git commit -m "feat: render simulator ui pages"
```

---

### Task 5: Interactive Simulator Executable

**Files:**
- Create: `src/main.c`
- Modify: `Makefile`

**Interfaces:**
- Consumes: `app_init`, `app_handle_button`, `ui_render_page`, `sim_display_commit`
- Produces: `reader_sim`

- [ ] **Step 1: Write failing build expectation**

Run:

```bash
make reader_sim
```

Expected: FAIL because `src/main.c` does not exist or `reader_sim` target is missing.

- [ ] **Step 2: Implement main input loop**

Create `src/main.c` that initializes app state, framebuffer, and display. It renders once at startup, then reads one line at a time from stdin. Map `w` to up, `s` to down, `h` and an empty line to home, `p` to power, and `q` to quit. After each valid button event, re-render, commit to `out/frame.ppm`, and print current page name plus refresh count.

- [ ] **Step 3: Build executable**

Run:

```bash
make reader_sim
```

Expected: `reader_sim` executable exists.

- [ ] **Step 4: Run tests**

Run:

```bash
make test
```

Expected: `tests passed`

- [ ] **Step 5: Smoke run**

Run:

```bash
./reader_sim
```

Type `s`, `h`, `p`, `q`.
Expected: the simulator reports page changes, refresh count increases, and `out/frame.ppm` exists.

- [ ] **Step 6: Commit**

```bash
git add src/main.c Makefile
git commit -m "feat: add interactive reader simulator"
```

---

### Task 6: Documentation and Final Verification

**Files:**
- Create: `README.md`

**Interfaces:**
- Consumes: completed simulator
- Produces: usage documentation

- [ ] **Step 1: Document local commands**

Create `README.md` with:

```markdown
# AI Mobile E-Ink Reader

Desktop simulator for an ESP32 three-color E-Ink reader UI.

## Build

```bash
make reader_sim
```

## Test

```bash
make test
```

## Run

```bash
./reader_sim
```

Controls:

- `w`: up
- `s`: down
- `h` or Enter: home/select
- `p`: power/back
- `q`: quit

The latest simulated E-Ink frame is written to `out/frame.ppm`.
```

- [ ] **Step 2: Run final verification**

Run:

```bash
make clean
make test
make reader_sim
./reader_sim
```

For the interactive command, type `q` after the initial render.
Expected: tests pass, executable builds, and `out/frame.ppm` is produced.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: document reader simulator"
```

- [ ] **Step 4: Push**

```bash
git push
```

Expected: local commits are pushed to `origin/main`.

---

## Self-Review

- Spec coverage: the plan covers framebuffer primitives, PPM output, state transitions, all eight requested screens, keyboard simulation for four buttons, tests, and documentation.
- Scope: the plan deliberately excludes ESP-IDF, hardware drivers, real storage, network APIs, persistence, EPUB/TXT parsing, and high-quality Chinese fonts.
- Placeholder scan: no TODO/TBD implementation gaps remain in the plan.
- Type consistency: task interfaces use stable names across tests and implementation steps.
