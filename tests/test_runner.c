#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/gfx.h"
#include "platform/sim_display.h"
#include "app/app_state.h"
#include "ui/pages.h"
#include "font/font.h"

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ_INT(expected, actual) ASSERT_TRUE((expected) == (actual))

static int count_color(const gfx_framebuffer_t *fb, gfx_color_t color);

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
    font_t font;
    gfx_init(&fb);
    app_init(&app);
    ASSERT_EQ_INT(1, font_load_default(&font));
    ui_render_page(&fb, &app, &font);
    ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 100);
    ASSERT_TRUE(count_color(&fb, GFX_RED) > 100);
    font_free(&font);
}

static void test_each_primary_page_renders_nonblank(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;
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
    ASSERT_EQ_INT(1, font_load_default(&font));
    for (size_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
        gfx_init(&fb);
        app.page = pages[i];
        ui_render_page(&fb, &app, &font);
        ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 50);
    }
    font_free(&font);
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
    test_utf8_decoder_reads_ascii_and_chinese();
    test_utf8_decoder_replaces_invalid_bytes();
    test_default_font_has_chinese_glyphs();
    test_font_draw_text_places_pixels_for_chinese();
    test_weather_refresh_changes_counter();
    test_english_flip_changes_state();
    test_snake_movement_changes_position();
    test_home_render_uses_black_and_red();
    test_each_primary_page_renders_nonblank();
    puts("tests passed");
    return 0;
}
