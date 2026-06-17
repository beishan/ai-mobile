#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/gfx.h"
#include "platform/sim_display.h"
#include "app/app_state.h"
#include "ui/pages.h"

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

int main(void) {
    test_framebuffer_has_fixed_eink_size();
    test_set_pixel_clips_out_of_bounds();
    test_rectangles_clip_and_place_red_pixels();
    test_display_commit_writes_ppm_and_counts_refresh();
    test_home_selection_wraps_and_opens_weather();
    test_power_returns_function_page_to_home();
    test_reader_page_bounds();
    test_settings_toggles_power_saving();
    test_home_render_uses_black_and_red();
    test_each_primary_page_renders_nonblank();
    puts("tests passed");
    return 0;
}
