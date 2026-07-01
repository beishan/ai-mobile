#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/app_state.h"
#include "app/app_persistence.h"
#include "app/reader_library.h"
#include "font/font.h"
#include "gfx/gfx.h"
#include "platform/epd_frame.h"
#include "platform/input_debounce.h"
#include "platform/sdl_display.h"
#include "platform/sim_display.h"
#include "ui/icons.h"
#include "ui/pages.h"

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ_INT(expected, actual) ASSERT_TRUE((expected) == (actual))
#define TEST_STATUS_BAR_HEIGHT 24

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

static int count_color_in_region(const gfx_framebuffer_t *fb, gfx_color_t color, int x, int y, int w, int h) {
    int count = 0;
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if (gfx_get_pixel(fb, px, py) == color) {
                count++;
            }
        }
    }
    return count;
}

static int framebuffers_equal(const gfx_framebuffer_t *a, const gfx_framebuffer_t *b) {
    for (int y = 0; y < gfx_height(a); y++) {
        for (int x = 0; x < gfx_width(a); x++) {
            if (gfx_get_pixel(a, x, y) != gfx_get_pixel(b, x, y)) {
                return 0;
            }
        }
    }
    return 1;
}

static int has_solid_black_block(const gfx_framebuffer_t *fb, int x, int y, int w, int h, int block_size) {
    for (int py = y; py <= y + h - block_size; py++) {
        for (int px = x; px <= x + w - block_size; px++) {
            if (count_color_in_region(fb, GFX_BLACK, px, py, block_size, block_size) == block_size * block_size) {
                return 1;
            }
        }
    }
    return 0;
}

static int file_contains(const char *path, const char *needle) {
    FILE *file = fopen(path, "rb");
    char buffer[4096];
    size_t needle_len = strlen(needle);
    size_t carry = 0;
    if (file == NULL) {
        return 0;
    }
    while (!feof(file)) {
        size_t read = fread(buffer + carry, 1, sizeof(buffer) - 1 - carry, file);
        size_t total = carry + read;
        buffer[total] = '\0';
        if (strstr(buffer, needle) != NULL) {
            fclose(file);
            return 1;
        }
        if (needle_len > 1 && total >= needle_len - 1) {
            carry = needle_len - 1;
            memmove(buffer, buffer + total - carry, carry);
        } else {
            carry = total;
        }
    }
    fclose(file);
    return 0;
}

static void local_now_for_test(struct tm *out) {
    time_t now = time(NULL);
    ASSERT_TRUE(out != NULL);
    ASSERT_TRUE(localtime_r(&now, out) != NULL);
}

static void test_framebuffer_has_ssd677_size(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    ASSERT_EQ_INT(480, gfx_width(&fb));
    ASSERT_EQ_INT(800, gfx_height(&fb));
}

static void test_set_pixel_clips_out_of_bounds(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);
    gfx_set_pixel(&fb, -1, -1, GFX_BLACK);
    gfx_set_pixel(&fb, 480, 800, GFX_BLACK);
    gfx_set_pixel(&fb, 20, 30, GFX_BLACK);
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 0, 0));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 20, 30));
}

static void test_rectangles_clip_and_place_black_pixels(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);
    gfx_fill_rect(&fb, 478, 798, 10, 10, GFX_BLACK);
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 479, 799));
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 477, 797));
}

static void test_display_commit_writes_480x800_ppm(void) {
    gfx_framebuffer_t fb;
    sim_display_t display;
    char header[32] = {0};
    FILE *file;

    gfx_init(&fb);
    gfx_set_pixel(&fb, 0, 0, GFX_BLACK);
    sim_display_init(&display, "out/test_frame.ppm");
    ASSERT_EQ_INT(0, sim_display_commit(&display, &fb));
    ASSERT_EQ_INT(1, sim_display_refresh_count(&display));

    file = fopen("out/test_frame.ppm", "rb");
    ASSERT_TRUE(file != NULL);
    ASSERT_TRUE(fread(header, 1, 15, file) > 0);
    fclose(file);
    ASSERT_TRUE(strncmp(header, "P6\n480 800\n255\n", 15) == 0);
}

static void test_epd_frame_pack_is_single_bw_plane(void) {
    gfx_framebuffer_t fb;
    epd_frame_t frame;

    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);
    gfx_set_pixel(&fb, 0, 0, GFX_BLACK);
    gfx_set_pixel(&fb, 7, 0, GFX_BLACK);
    gfx_set_pixel(&fb, 8, 0, GFX_BLACK);

    ASSERT_EQ_INT(0, epd_frame_pack(&fb, &frame));
    ASSERT_EQ_INT(48000, (int)sizeof(frame.bw));
    ASSERT_EQ_INT(0x7e, frame.bw[0]);
    ASSERT_EQ_INT(0x7f, frame.bw[1]);
}

static void test_target_documents_reference_ssd677_and_no_game_module(void) {
    ASSERT_TRUE(file_contains("requires01.md", "4.26"));
    ASSERT_TRUE(file_contains("requires01.md", "480×800"));
    ASSERT_TRUE(file_contains("requires01.md", "SSD677"));
    ASSERT_TRUE(file_contains("requires01.md", "黑白显示规范"));
    ASSERT_TRUE(!file_contains("requires01.md", "游戏模块"));
    ASSERT_TRUE(!file_contains("requires01.md", "贪吃蛇"));
    ASSERT_TRUE(file_contains("README.md", "480"));
    ASSERT_TRUE(file_contains("README.md", "800"));
    ASSERT_TRUE(file_contains("README.md", "SSD677"));
}

static void test_esp_project_targets_ssd677_bw_panel(void) {
    ASSERT_TRUE(file_contains("platformio.ini", "esp32-n16r8"));
    ASSERT_TRUE(file_contains("src/platform/esp_board_config.h", "SSD677"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "SSD677"));
    ASSERT_TRUE(file_contains("src/platform/epd_frame.h", "bw"));
    ASSERT_TRUE(!file_contains("src/platform/epd_frame.h", "red"));
}

static void test_home_has_six_modules_and_no_game(void) {
    app_page_t expected[] = {
        APP_PAGE_BOOKSHELF,
        APP_PAGE_WEATHER,
        APP_PAGE_CALENDAR,
        APP_PAGE_ENGLISH,
        APP_PAGE_SETTINGS,
        APP_PAGE_ABOUT
    };
    for (int i = 0; i < 6; i++) {
        app_state_t app;
        app_init(&app);
        app.home_selection = i;
        app_handle_button(&app, APP_BUTTON_HOME);
        ASSERT_EQ_INT(expected[i], app.page);
    }
    ASSERT_TRUE(strcmp("unknown", app_page_name((app_page_t)99)) == 0);
}

static void test_settings_page_scrolls_with_up_down_buttons(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_SETTINGS;

    ASSERT_EQ_INT(0, app.settings_scroll);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_TRUE(app.settings_scroll > 0);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(0, app.settings_scroll);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(0, app.settings_scroll);
}

static void test_weather_page_scrolls_while_changing_city(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_WEATHER;

    ASSERT_EQ_INT(0, app.weather_city_index);
    ASSERT_EQ_INT(0, app.weather_scroll);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.weather_city_index);
    ASSERT_TRUE(app.weather_scroll > 0);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(0, app.weather_city_index);
    ASSERT_EQ_INT(0, app.weather_scroll);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(2, app.weather_city_index);
    ASSERT_EQ_INT(0, app.weather_scroll);
}

static void test_home_selection_uses_outline_frame_and_larger_icon(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_HOME;
    app.home_selection = 0;
    ui_render_page(&fb, &app, &font);

    /* tile 0 sits at (PAGE_MARGIN_X=24, BODY_TOP=40); rounded outline frame
     * around it with a large icon centered inside. */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 16, 40));   /* outside left of frame */
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 24, 80));   /* left edge of frame */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 120, 52));  /* inside frame, above icon */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 80, 64, 72, 72) > 220);
    font_free(&font);
}

static void test_home_icons_use_modern_line_style_without_large_solid_blocks(void) {
    ui_icon_kind_t icons[] = {
        UI_ICON_READER,
        UI_ICON_WEATHER,
        UI_ICON_CALENDAR,
        UI_ICON_ENGLISH,
        UI_ICON_SETTINGS,
        UI_ICON_ABOUT
    };
    for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); i++) {
        gfx_framebuffer_t fb;
        gfx_init(&fb);
        ui_draw_icon(&fb, icons[i], 10, 10, 0);
        ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 10, 10, 64, 64) > 120);
        ASSERT_TRUE(!has_solid_black_block(&fb, 10, 10, 64, 64, 14));
    }
}

static void test_home_status_bar_content_is_vertically_centered(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_HOME;
    ui_render_page(&fb, &app, &font);

    ASSERT_TRUE(count_color_in_region(&fb, GFX_WHITE, 8, 4, 180, 1) == 0);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_WHITE, 8, 16, 180, 1) > 0);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_WHITE, 0, 1, GFX_WIDTH, 2) == 0);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_WHITE, 0, 21, GFX_WIDTH, 2) == 0);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_WHITE, 8, 6, 180, 12) > 20);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_WHITE, GFX_WIDTH - 90, 6, 80, 12) > 20);
    font_free(&font);
}

static void assert_uses_home_status_bar(gfx_framebuffer_t *fb) {
    ASSERT_TRUE(count_color_in_region(fb, GFX_BLACK, 0, 0, GFX_WIDTH, TEST_STATUS_BAR_HEIGHT) > 10000);
    ASSERT_TRUE(count_color_in_region(fb, GFX_WHITE, 8, 4, 180, 16) > 20);
    ASSERT_TRUE(count_color_in_region(fb, GFX_WHITE, GFX_WIDTH - 92, 4, 84, 16) > 20);
}

static void assert_uses_bookshelf_status_bar(gfx_framebuffer_t *fb) {
    ASSERT_TRUE(count_color_in_region(fb, GFX_BLACK, 0, 0, GFX_WIDTH, TEST_STATUS_BAR_HEIGHT) < 1500);
    ASSERT_TRUE(count_color_in_region(fb, GFX_BLACK, 18, 10, 60, 20) > 25);
    ASSERT_TRUE(count_color_in_region(fb, GFX_BLACK, GFX_WIDTH - 94, 8, 86, 20) > 45);
}

static void test_non_reader_pages_use_home_status_bar_style(void) {
    app_page_t pages[] = {
        APP_PAGE_HOME,
        APP_PAGE_WEATHER,
        APP_PAGE_ENGLISH,
        APP_PAGE_SETTINGS,
        APP_PAGE_ABOUT
    };
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    for (size_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
        gfx_framebuffer_t fb;
        app_state_t app;
        gfx_init(&fb);
        app_init(&app);
        app.page = pages[i];
        ui_render_page(&fb, &app, &font);
        assert_uses_home_status_bar(&fb);
    }
    font_free(&font);
}

static void test_reader_keeps_reading_status_bar_style(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_READER;
    ui_render_page(&fb, &app, &font);

    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 0, 0, GFX_WIDTH, TEST_STATUS_BAR_HEIGHT) < GFX_WIDTH * TEST_STATUS_BAR_HEIGHT / 3);
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 0, 40));
    font_free(&font);
}

static void test_bookshelf_and_reader_keep_progress(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_BOOKSHELF;
    app.bookshelf_selection = 1;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_READER, app.page);
    ASSERT_EQ_INT(1, app.current_book);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.book_current_pages[1]);
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_BOOKSHELF, app.page);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.reader_page);
}

static void test_reader_menu_opens_reader_settings_and_applies(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;

    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_READER_SETTINGS, app.page);
    ASSERT_EQ_INT(0, app.reader_menu_open);

    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_READER, app.page);
}

static void test_reader_menu_opens_full_catalog_page_and_selects_chapter(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;

    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_READER_CATALOG, app.page);
    ASSERT_EQ_INT(0, app.reader_menu_open);
    ASSERT_EQ_INT(0, app.reader_catalog_open);

    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_READER, app.page);
    ASSERT_EQ_INT(reader_library_chapter_page(app.current_book, 1), app.reader_page);
}

static void test_reader_settings_font_row_cycles_external_font_choice(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER_SETTINGS;
    app.reader_settings_selection = 1;

    ASSERT_EQ_INT(0, app.reader_font_index);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.reader_font_index);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(2, app.reader_font_index);
}

static void test_bookshelf_matches_design2_grid_skeleton(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_BOOKSHELF;
    app.bookshelf_selection = -1;
    ui_render_page(&fb, &app, &font);

    assert_uses_bookshelf_status_bar(&fb);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 20, 42, 440, 12) < 120);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 150, 91, 28, 1) < 3);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 306, 91, 28, 1) < 3);

    /* Three cover columns and three rows move up after removing the toolbar. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 24, 58, 124, 170) > 350);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 180, 58, 124, 170) > 350);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 336, 58, 124, 170) > 350);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 24, 282, 124, 170) > 350);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 180, 282, 124, 170) > 350);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 336, 282, 124, 170) > 350);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 24, 505, 124, 160) > 350);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 180, 505, 124, 160) > 350);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 336, 505, 124, 160) > 350);

    /* Bottom pagination/sort strip is fixed like the design. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 0, 768, GFX_WIDTH, 1) > 430);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 18, 779, 96, 16) > 40);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 214, 777, 52, 18) > 35);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 370, 779, 90, 16) > 45);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 198, 777, 12, 18) < 3);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 270, 777, 12, 18) < 3);
    font_free(&font);
}

static void test_reader_settings_matches_design2_skeleton(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_READER_SETTINGS;
    app.reader_margin_index = 1;
    app.reader_indent_enabled = 1;
    app.reader_bold_enabled = 0;
    app.reader_refresh_mode = 0;
    ui_render_page(&fb, &app, &font);

    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 18, 10, 60, 20) > 25);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 190, 50, 120, 42) > 100);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 18, 48, 26, 38) < 10);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 22, 89, 436, 640) > 1200);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 38, 103, 28, 30) > 35);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 132, 172, 246, 2) > 180);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 262, 164, 18, 18) > 120);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 132, 222, 312, 30) > 140);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 210, 362, 64, 92) > 130);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 390, 490, 44, 24) > 500);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 392, 558, 44, 24) > 120);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 392, 558, 18, 18) > 30);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 32, 744, 194, 40) > 180);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 254, 744, 194, 40) > 1200);
    font_free(&font);
}

static void test_reader_catalog_matches_design2_skeleton(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_READER_CATALOG;
    app.reader_catalog_selection = 1;
    app.reader_page = reader_library_chapter_page(app.current_book, 1);
    ui_render_page(&fb, &app, &font);

    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 0, 0, 20, 80) < 5);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 210, 24, 70, 42) < 70);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 0, 66, GFX_WIDTH, 2) < 40);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 40, 34, 360, 260) > 500);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 14, 62, 452, 52) > 220);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 0, 735, GFX_WIDTH, 65) < 20);
    font_free(&font);
}

static void test_bookshelf_selection_uses_rounded_outline_frame(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_BOOKSHELF;
    app.bookshelf_selection = 1;  /* Select second book (card 1) */
    ui_render_page(&fb, &app, &font);

    /* Card 1 is at col=1, row=0 (x=180, y=58, w=124, h=170 cover). */
    /* Outside rounded corner is white */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 176, 54));  /* outside top-left corner */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 308, 54));  /* outside top-right corner */
    /* Top border is black (straight part of top edge) */
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 220, 56));  /* inside top border */
    /* Cover area has decorative content (large character) */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 180, 58, 124, 170) > 350);  /* cover has content */
    /* Card 0 has no selection frame */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 20, 54));   /* outside card 0 */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 152, 54));  /* outside card 0 */
    font_free(&font);
}

static void test_reader_library_exposes_books_and_pages(void) {
    const reader_book_t *book = reader_library_book(0);
    const char *page0 = reader_library_page_text(0, 0);
    const char *page1 = reader_library_page_text(0, 1);
    const char *missing_book = reader_library_page_text(99, 0);
    const char *missing_page = reader_library_page_text(0, 99);

    ASSERT_EQ_INT(APP_BOOK_COUNT, reader_library_book_count());
    ASSERT_TRUE(book != NULL);
    ASSERT_TRUE(strcmp("三体", book->title) == 0);
    ASSERT_TRUE(strcmp("刘慈欣", book->author) == 0);
    ASSERT_TRUE(strcmp("1.2MB", book->size_label) == 0);
    ASSERT_TRUE(strcmp("TXT", book->file_type) == 0);
    ASSERT_TRUE(reader_library_page_count(0) >= 5);
    ASSERT_TRUE(page0 != NULL);
    ASSERT_TRUE(page1 != NULL);
    ASSERT_TRUE(strstr(page0, "第三章") != NULL);
    ASSERT_TRUE(strcmp(page0, page1) != 0);
    ASSERT_TRUE(missing_book != NULL);
    ASSERT_TRUE(missing_page != NULL);
    ASSERT_TRUE(strcmp("", missing_book) == 0);
    ASSERT_TRUE(strcmp("", missing_page) == 0);
}

static void test_app_init_uses_reader_library_page_counts(void) {
    app_state_t app;
    app_init(&app);
    for (int i = 0; i < APP_BOOK_COUNT; i++) {
        ASSERT_EQ_INT(reader_library_page_count(i), app.book_pages[i]);
    }
}

static void test_reader_library_builds_pages_from_source_text(void) {
    ASSERT_TRUE(reader_library_source_text(0) != NULL);
    ASSERT_TRUE(strstr(reader_library_source_text(0), "\f") != NULL);
    ASSERT_EQ_INT(5, reader_library_page_count(0));
    ASSERT_TRUE(strstr(reader_library_page_text(0, 0), "\f") == NULL);
    ASSERT_TRUE(strstr(reader_library_page_text(0, 1), "叶文洁") != NULL);
    ASSERT_TRUE(strstr(reader_library_page_text(0, 4), "天色微亮") != NULL);
}

static void test_reader_library_loads_source_text_from_file(void) {
    app_state_t app;
    ASSERT_EQ_INT(0, reader_library_load_book_file(0, "assets/books/santi.txt"));
    ASSERT_EQ_INT(3, reader_library_page_count(0));
    ASSERT_TRUE(strstr(reader_library_source_text(0), "assets/books/santi.txt") != NULL);
    ASSERT_TRUE(strstr(reader_library_page_text(0, 0), "来自文件的第一页") != NULL);
    ASSERT_TRUE(strstr(reader_library_page_text(0, 1), "第二页来自") != NULL);
    ASSERT_TRUE(strstr(reader_library_page_text(0, 2), "第三页用于验证") != NULL);
    app_init(&app);
    ASSERT_EQ_INT(3, app.book_pages[0]);
    ASSERT_EQ_INT(-1, reader_library_load_book_file(0, "assets/books/missing.txt"));
}

static void test_reader_library_auto_paginates_plain_text_file(void) {
    ASSERT_EQ_INT(0, reader_library_load_book_file(1, "assets/books/auto_page.txt"));
    ASSERT_TRUE(strstr(reader_library_source_text(1), "\f") == NULL);
    ASSERT_TRUE(reader_library_page_count(1) > 1);
    ASSERT_TRUE(strlen(reader_library_page_text(1, 0)) < 1024);
    ASSERT_TRUE(strlen(reader_library_page_text(1, 1)) < 1024);
    ASSERT_TRUE(strstr(reader_library_page_text(1, 0), "自动分页第一页开始") != NULL);
    ASSERT_TRUE(strlen(reader_library_page_text(1, 1)) > 0);
    ASSERT_TRUE(strcmp(reader_library_page_text(1, 0), reader_library_page_text(1, 1)) != 0);
    ASSERT_TRUE((reader_library_page_text(1, 1)[0] & 0xc0) != 0x80);
}

static void test_reader_library_loads_external_real_books(void) {
    app_state_t app;
    const reader_book_t *first_book;
    const reader_book_t *second_book;

    ASSERT_TRUE(reader_library_load_external_books() >= 2);
    first_book = reader_library_book(0);
    second_book = reader_library_book(1);
    ASSERT_TRUE(first_book != NULL);
    ASSERT_TRUE(second_book != NULL);
    ASSERT_TRUE(strstr(first_book->title, "全民转职") != NULL);
    ASSERT_TRUE(strstr(second_book->title, "混沌天帝诀") != NULL);
    ASSERT_TRUE(strcmp("TXT", first_book->file_type) == 0);
    ASSERT_TRUE(strstr(reader_library_source_text(0), "全民转职") != NULL);
    ASSERT_TRUE(strstr(reader_library_source_text(0), "转职修仙者") != NULL);
    ASSERT_TRUE(strstr(reader_library_source_text(1), "第1章 背叛") != NULL);
    ASSERT_TRUE(strstr(reader_library_source_text(1), "楚剑秋") != NULL);
    ASSERT_TRUE(strstr(reader_library_page_text(0, 0), "笔趣阁789提供下载") != NULL);
    ASSERT_TRUE(reader_library_page_count(0) >= 2);
    ASSERT_TRUE(reader_library_page_count(1) >= 2);

    app_init(&app);
    ASSERT_EQ_INT(reader_library_page_count(0), app.book_pages[0]);
    ASSERT_EQ_INT(reader_library_page_count(1), app.book_pages[1]);
    ASSERT_EQ_INT(reader_library_page_count(2), app.book_pages[2]);
}

static void test_bookshelf_reflects_loaded_realbook_metadata(void) {
    gfx_framebuffer_t before_fb;
    gfx_framebuffer_t after_fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&before_fb);
    app_init(&app);
    app.page = APP_PAGE_BOOKSHELF;
    ui_render_page(&before_fb, &app, &font);

    ASSERT_TRUE(reader_library_load_external_books() >= 2);
    gfx_init(&after_fb);
    app_init(&app);
    app.page = APP_PAGE_BOOKSHELF;
    ui_render_page(&after_fb, &app, &font);

    ASSERT_TRUE(!framebuffers_equal(&before_fb, &after_fb));
    ASSERT_TRUE(count_color_in_region(&after_fb, GFX_BLACK, 24, 230, 124, 28) > 40);
    ASSERT_TRUE(count_color_in_region(&after_fb, GFX_BLACK, 180, 230, 124, 28) > 40);
    font_free(&font);
}

static void test_entrypoints_load_external_books_on_startup(void) {
    ASSERT_TRUE(file_contains("src/main.c", "reader_library_load_external_books"));
    ASSERT_TRUE(file_contains("src/main_sdl.c", "reader_library_load_external_books"));
}

static void test_app_persistence_round_trips_reader_progress_and_settings(void) {
    app_state_t app;
    app_state_t restored;
    app_persisted_state_t snapshot;
    app_persisted_state_t decoded;
    char encoded[APP_PERSISTENCE_TEXT_MAX];

    app_init(&app);
    app.current_book = 1;
    app.recent_book = 1;
    app.book_current_pages[0] = 1;
    app.book_current_pages[1] = 1;
    app.book_current_pages[2] = 1;
    app.book_bookmark_pages[0] = -1;
    app.book_bookmark_pages[1] = 1;
    app.book_bookmark_pages[2] = 1;
    app.reader_page = 1;
    app.font_size_index = 4;
    app.line_spacing_index = 3;
    app.wifi_connected = 0;
    app.weather_city_index = 2;
    app.power_saving_enabled = 0;

    app_persistence_capture(&app, &snapshot);
    ASSERT_EQ_INT(0, app_persistence_encode(&snapshot, encoded, sizeof(encoded)));
    ASSERT_EQ_INT(0, app_persistence_decode(encoded, &decoded));

    app_init(&restored);
    app_persistence_apply(&restored, &decoded);

    ASSERT_EQ_INT(1, restored.current_book);
    ASSERT_EQ_INT(1, restored.recent_book);
    ASSERT_EQ_INT(1, restored.book_current_pages[0]);
    ASSERT_EQ_INT(1, restored.book_current_pages[1]);
    ASSERT_EQ_INT(1, restored.book_current_pages[2]);
    ASSERT_EQ_INT(-1, restored.book_bookmark_pages[0]);
    ASSERT_EQ_INT(1, restored.book_bookmark_pages[1]);
    ASSERT_EQ_INT(1, restored.book_bookmark_pages[2]);
    ASSERT_EQ_INT(1, restored.reader_page);
    ASSERT_EQ_INT(4, restored.font_size_index);
    ASSERT_EQ_INT(3, restored.line_spacing_index);
    ASSERT_EQ_INT(0, restored.wifi_connected);
    ASSERT_EQ_INT(2, restored.weather_city_index);
    ASSERT_EQ_INT(0, restored.power_saving_enabled);
}

static void test_app_persistence_clamps_restored_values_to_current_limits(void) {
    app_state_t app;
    app_persisted_state_t snapshot = {
        .version = APP_PERSISTENCE_VERSION,
        .current_book = 99,
        .recent_book = 99,
        .book_current_pages = {99, -5, 99},
        .book_bookmark_pages = {99, -5, 99},
        .font_size_index = 99,
        .line_spacing_index = -5,
        .wifi_connected = 7,
        .weather_city_index = -5,
        .power_saving_enabled = -2
    };

    app_init(&app);
    app_persistence_apply(&app, &snapshot);

    ASSERT_EQ_INT(APP_BOOK_COUNT - 1, app.current_book);
    ASSERT_EQ_INT(APP_BOOK_COUNT - 1, app.recent_book);
    ASSERT_EQ_INT(app.book_pages[0] - 1, app.book_current_pages[0]);
    ASSERT_EQ_INT(0, app.book_current_pages[1]);
    ASSERT_EQ_INT(app.book_pages[2] - 1, app.book_current_pages[2]);
    ASSERT_EQ_INT(app.book_pages[0] - 1, app.book_bookmark_pages[0]);
    ASSERT_EQ_INT(-1, app.book_bookmark_pages[1]);
    ASSERT_EQ_INT(app.book_pages[2] - 1, app.book_bookmark_pages[2]);
    ASSERT_EQ_INT(app.book_current_pages[APP_BOOK_COUNT - 1], app.reader_page);
    ASSERT_EQ_INT(4, app.font_size_index);
    ASSERT_EQ_INT(0, app.line_spacing_index);
    ASSERT_EQ_INT(1, app.wifi_connected);
    ASSERT_EQ_INT(0, app.weather_city_index);
    ASSERT_EQ_INT(0, app.power_saving_enabled);
}

static void test_app_persistence_rejects_malformed_payloads(void) {
    app_persisted_state_t decoded;
    ASSERT_EQ_INT(-1, app_persistence_decode("not a persisted app state", &decoded));
    ASSERT_EQ_INT(-1, app_persistence_decode("AIPERSIST 99\n", &decoded));
}

static void test_app_persistence_saves_and_loads_text_file(void) {
    app_persisted_state_t snapshot = {
        .version = APP_PERSISTENCE_VERSION,
        .current_book = 2,
        .recent_book = 2,
        .book_current_pages = {1, 0, 2},
        .book_bookmark_pages = {-1, 0, 2},
        .font_size_index = 3,
        .line_spacing_index = 1,
        .wifi_connected = 1,
        .weather_city_index = 2,
        .power_saving_enabled = 0
    };
    app_persisted_state_t loaded;

    ASSERT_EQ_INT(0, app_persistence_save_file("out/test_app_state.txt", &snapshot));
    ASSERT_EQ_INT(0, app_persistence_load_file("out/test_app_state.txt", &loaded));
    ASSERT_EQ_INT(2, loaded.current_book);
    ASSERT_EQ_INT(2, loaded.recent_book);
    ASSERT_EQ_INT(1, loaded.book_current_pages[0]);
    ASSERT_EQ_INT(2, loaded.book_current_pages[2]);
    ASSERT_EQ_INT(-1, loaded.book_bookmark_pages[0]);
    ASSERT_EQ_INT(2, loaded.book_bookmark_pages[2]);
    ASSERT_EQ_INT(3, loaded.font_size_index);
    ASSERT_EQ_INT(1, loaded.line_spacing_index);
    ASSERT_EQ_INT(1, loaded.wifi_connected);
    ASSERT_EQ_INT(2, loaded.weather_city_index);
    ASSERT_EQ_INT(0, loaded.power_saving_enabled);
    ASSERT_EQ_INT(-1, app_persistence_load_file("out/missing_app_state.txt", &loaded));
}

static void test_app_persistence_saves_and_loads_app_state_file(void) {
    app_state_t app;
    app_state_t restored;

    app_init(&app);
    app.current_book = 1;
    app.recent_book = 1;
    app.book_current_pages[1] = 1;
    app.book_bookmark_pages[1] = 1;
    app.reader_page = 1;
    app.font_size_index = 4;
    app.line_spacing_index = 3;
    app.wifi_connected = 0;
    app.weather_city_index = 2;
    app.power_saving_enabled = 0;

    ASSERT_EQ_INT(0, app_persistence_save_app_file("out/test_live_app_state.txt", &app));

    app_init(&restored);
    ASSERT_EQ_INT(0, app_persistence_load_app_file("out/test_live_app_state.txt", &restored));
    ASSERT_EQ_INT(1, restored.current_book);
    ASSERT_EQ_INT(1, restored.recent_book);
    ASSERT_EQ_INT(1, restored.book_current_pages[1]);
    ASSERT_EQ_INT(1, restored.book_bookmark_pages[1]);
    ASSERT_EQ_INT(1, restored.reader_page);
    ASSERT_EQ_INT(4, restored.font_size_index);
    ASSERT_EQ_INT(3, restored.line_spacing_index);
    ASSERT_EQ_INT(0, restored.wifi_connected);
    ASSERT_EQ_INT(2, restored.weather_city_index);
    ASSERT_EQ_INT(0, restored.power_saving_enabled);
}

static void test_app_persistence_nvs_backend_is_stubbed_on_host(void) {
    app_state_t app;
    app_init(&app);
    ASSERT_EQ_INT(-1, app_persistence_save_nvs("reader", "app_state", &app));
    ASSERT_EQ_INT(-1, app_persistence_load_nvs("reader", "app_state", &app));
}

static void test_esp_firmware_wires_app_persistence_to_nvs(void) {
    ASSERT_TRUE(file_contains("src/main_esp.c", "nvs_flash_init"));
    ASSERT_TRUE(file_contains("src/main_esp.c", "app_persistence_load_nvs"));
    ASSERT_TRUE(file_contains("src/main_esp.c", "app_persistence_save_nvs"));
    ASSERT_TRUE(file_contains("src/CMakeLists.txt", "nvs_flash"));
}

static void test_esp_input_wires_button_debounce(void) {
    ASSERT_TRUE(file_contains("src/platform/esp_board_config.h", "ESP_BUTTON_DEBOUNCE_MS 60"));
    ASSERT_TRUE(file_contains("src/platform/esp_board_config.h", "ESP_BUTTON_LONG_PRESS_MS 1200"));
    ASSERT_TRUE(file_contains("src/platform/esp_input.c", "input_debounce_update"));
    ASSERT_TRUE(file_contains("src/platform/esp_input.c", "APP_BUTTON_POWER_LONG"));
    ASSERT_TRUE(file_contains("src/main_esp.c", "esp_display_sleep"));
    ASSERT_TRUE(file_contains("src/CMakeLists.txt", "platform/input_debounce.c"));
}

static void test_sdl_key_mapping_for_core_buttons(void) {
    ASSERT_EQ_INT(APP_BUTTON_UP, sdl_display_button_from_key(SDLK_UP));
    ASSERT_EQ_INT(APP_BUTTON_DOWN, sdl_display_button_from_key(SDLK_s));
    ASSERT_EQ_INT(APP_BUTTON_HOME, sdl_display_button_from_key(SDLK_RETURN));
    ASSERT_EQ_INT(APP_BUTTON_POWER, sdl_display_button_from_key(SDLK_BACKSPACE));
}

static void test_input_debounce_emits_once_after_stable_press(void) {
    input_debounce_t debounce;
    input_debounce_init(&debounce, 3);

    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 0));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(1, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 1));
}

static void test_input_debounce_rearms_after_stable_release(void) {
    input_debounce_t debounce;
    input_debounce_init(&debounce, 2);

    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(1, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 0));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 0));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 0));
    ASSERT_EQ_INT(0, input_debounce_update(&debounce, 1));
    ASSERT_EQ_INT(1, input_debounce_update(&debounce, 1));
}

static void test_input_debounce_short_press_emits_on_release(void) {
    input_debounce_t debounce;
    input_debounce_init(&debounce, 2);

    ASSERT_EQ_INT(INPUT_DEBOUNCE_NONE, input_debounce_update_hold(&debounce, 1, 5));
    ASSERT_EQ_INT(INPUT_DEBOUNCE_NONE, input_debounce_update_hold(&debounce, 1, 5));
    ASSERT_EQ_INT(INPUT_DEBOUNCE_NONE, input_debounce_update_hold(&debounce, 0, 5));
    ASSERT_EQ_INT(INPUT_DEBOUNCE_SHORT_PRESS, input_debounce_update_hold(&debounce, 0, 5));
}

static void test_input_debounce_long_press_emits_once_while_held(void) {
    input_debounce_t debounce;
    input_debounce_init(&debounce, 2);

    ASSERT_EQ_INT(INPUT_DEBOUNCE_NONE, input_debounce_update_hold(&debounce, 1, 3));
    ASSERT_EQ_INT(INPUT_DEBOUNCE_NONE, input_debounce_update_hold(&debounce, 1, 3));
    ASSERT_EQ_INT(INPUT_DEBOUNCE_LONG_PRESS, input_debounce_update_hold(&debounce, 1, 3));
    ASSERT_EQ_INT(INPUT_DEBOUNCE_NONE, input_debounce_update_hold(&debounce, 1, 3));
    ASSERT_EQ_INT(INPUT_DEBOUNCE_NONE, input_debounce_update_hold(&debounce, 0, 3));
    ASSERT_EQ_INT(INPUT_DEBOUNCE_NONE, input_debounce_update_hold(&debounce, 0, 3));
}

static void test_icons_draw_black_pixels_only(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    ui_draw_icon(&fb, UI_ICON_READER, 10, 10, 0);
    ui_draw_icon(&fb, UI_ICON_WEATHER, 70, 10, 0);
    ui_draw_icon(&fb, UI_ICON_CALENDAR, 130, 10, 0);
    ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 100);
}

static void test_primary_pages_render_nonblank(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;
    app_page_t pages[] = {
        APP_PAGE_HOME,
        APP_PAGE_BOOKSHELF,
        APP_PAGE_READER,
        APP_PAGE_READER_CATALOG,
        APP_PAGE_WEATHER,
        APP_PAGE_CALENDAR,
        APP_PAGE_ENGLISH,
        APP_PAGE_SETTINGS,
        APP_PAGE_ABOUT
    };
    ASSERT_EQ_INT(1, font_load_default(&font));
    app_init(&app);
    for (size_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
        gfx_init(&fb);
        app.page = pages[i];
        ui_render_page(&fb, &app, &font);
        ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 50);
    }
    font_free(&font);
}

static void test_reader_body_renders_text_in_content_area(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_READER;
    app.reader_page = 0;
    ui_render_page(&fb, &app, &font);

    /* The reader renders body text in the expanded content area (24, 48, 432, 696). */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 40, 60, 400, 600) > 0);
    font_free(&font);
}

static void test_reader_main_matches_design_skeleton(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_READER;
    app.current_book = 0;
    app.reader_page = 0;
    ui_render_page(&fb, &app, &font);

    /* Reader design uses the same white status bar and separator as settings. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 0, 0, GFX_WIDTH, 40) > 80);
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 0, 40));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 479, 40));

    /* Reader chrome keeps only status bar and body content; no duplicate book/chapter headings. */
    ASSERT_TRUE(!file_contains("src/ui/pages.c", "breadcrumb"));
    ASSERT_TRUE(!file_contains("src/ui/pages.c", "font_draw_text_builtin(36, fb, 28, 132"));

    /* Body text starts near the top of the reading area. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 58, 70, 360, 130) > 160);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 0, 120, 24, 620) < 20);

    /* Bottom progress rule plus left and right footer text. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 20, 770, 440, 1) > 400);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 28, 779, 58, 17) > 55);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 285, 783, 170, 14) > 55);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 28, 764, 72, 5) < 20);
    font_free(&font);
}

static void test_settings_page_matches_design_skeleton(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_SETTINGS;
    app.wifi_connected = 1;
    app.weather_city_index = 2;
    app.power_saving_enabled = 1;
    ui_render_page(&fb, &app, &font);

    assert_uses_home_status_bar(&fb);

    /* Large title starts near x=30,y=80 and leaves the right side open. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 30, 78, 190, 48) > 180);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 300, 78, 140, 48) < 40);

    /* First group box uses looser row height and spacing. */
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 34, 184));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 447, 184));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 34, 295));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 447, 295));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 34, 240));
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 42, 195, 46, 40) > 45);

    /* Power-saving row has a filled pill switch with a white knob, like the design. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 370, 560, 46, 24) > 500);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_WHITE, 394, 562, 20, 20) > 160);

    /* Lower settings continue beyond the first viewport for scrolling. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 27, 724, 428, 56) > 120);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 30, 790, 180, 40) < 40);
    font_free(&font);
}

static void test_settings_render_uses_scroll_offset_below_status_bar(void) {
    gfx_framebuffer_t top_fb;
    gfx_framebuffer_t scrolled_fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&top_fb);
    gfx_init(&scrolled_fb);
    app_init(&app);
    app.page = APP_PAGE_SETTINGS;
    app.settings_scroll = 0;
    ui_render_page(&top_fb, &app, &font);

    app.settings_scroll = 120;
    ui_render_page(&scrolled_fb, &app, &font);

    assert_uses_home_status_bar(&scrolled_fb);
    ASSERT_TRUE(!framebuffers_equal(&top_fb, &scrolled_fb));
    ASSERT_TRUE(count_color_in_region(&top_fb, GFX_BLACK, 30, 78, 190, 48) > 120);
    ASSERT_TRUE(count_color_in_region(&scrolled_fb, GFX_BLACK, 30, 690, 190, 80) > 120);
    font_free(&font);
}

static void assert_font_contains_text(const font_face_t *face, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    while (cursor != NULL && *cursor != '\0') {
        uint32_t cp;
        ASSERT_TRUE(font_decode_utf8(&cursor, &cp));
        ASSERT_TRUE(font_find_glyph(face, cp) != NULL);
    }
}

static void test_settings_builtin_font_contains_all_labels(void) {
    const char *labels[] = {
        "09:41  晴 26C86%",
        "系统设置",
        "网络与连接",
        "Wi-Fi",
        "已连接 Reader_5G",
        "未连接",
        "蓝牙",
        "已关闭",
        "系统与时间",
        "天气城市",
        "杭州市",
        "时间同步",
        "已同步 09:41",
        "电源与性能",
        "电池节能模式",
        "存储空间",
        "已使用 12.6GB / 32GB",
        "内容与服务",
        "字典管理",
        "已安装 3 个字典",
        "关于与更新",
        "关于设备",
        "型号 Reader X",
        "软件更新",
        "当前版本 1.2.0",
        "20%"
    };
    const font_face_t *faces[] = {
        font_get_face(FONT_SIZE_14),
        font_get_face(FONT_SIZE_18),
        font_get_face(FONT_SIZE_20),
        font_get_face(FONT_SIZE_24)
    };

    for (size_t f = 0; f < sizeof(faces) / sizeof(faces[0]); f++) {
        for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
            assert_font_contains_text(faces[f], labels[i]);
        }
    }
}

static void test_reader_settings_builtin_font_contains_all_labels(void) {
    const char *labels[] = {
        "阅读设置",
        "排版设置",
        "字号",
        "大黑正圆更纱唐美",
        "行距",
        "紧凑适中宽松",
        "页边距",
        "窄适中宽自定义",
        "段首缩进",
        "每段首行自动缩进两个字符",
        "加粗",
        "启用后正文将以加粗字体显示",
        "翻页动画",
        "仿真",
        "刷新模式",
        "普通快速极速",
        "恢复默认",
        "应用"
    };
    const font_face_t *faces[] = {
        font_get_face(FONT_SIZE_14),
        font_get_face(FONT_SIZE_16),
        font_get_face(FONT_SIZE_18),
        font_get_face(FONT_SIZE_20),
        font_get_face(FONT_SIZE_24)
    };

    for (size_t f = 0; f < sizeof(faces) / sizeof(faces[0]); f++) {
        for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
            assert_font_contains_text(faces[f], labels[i]);
        }
    }
}

static void test_reader_catalog_builtin_font_contains_all_labels(void) {
    const char *labels[] = {
        "目录",
        "共3部",
        "本章进度  16%",
        "全民转职：修仙者废？看我一剑开仙门！"
    };
    const font_face_t *faces[] = {
        font_get_face(FONT_SIZE_16),
        font_get_face(FONT_SIZE_18),
        font_get_face(FONT_SIZE_20),
        font_get_face(FONT_SIZE_24)
    };

    for (size_t f = 0; f < sizeof(faces) / sizeof(faces[0]); f++) {
        for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
            assert_font_contains_text(faces[f], labels[i]);
        }
    }
}

static void test_bookshelf_builtin_font_contains_realbook_labels(void) {
    const char *labels[] = {
        "全民转职：修仙者废？",
        "混沌天帝诀",
        "本地书籍"
    };
    const font_face_t *faces[] = {
        font_get_face(FONT_SIZE_14),
        font_get_face(FONT_SIZE_18),
        font_get_face(FONT_SIZE_20),
        font_get_face(FONT_SIZE_24)
    };

    for (size_t f = 0; f < sizeof(faces) / sizeof(faces[0]); f++) {
        for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
            assert_font_contains_text(faces[f], labels[i]);
        }
    }
}

static void test_weather_builtin_font_contains_all_labels(void) {
    const char *labels[] = {
        "北京市上海市广州市",
        "晴多云小雨",
        "更新于 10:30",
        "空气质量",
        "良",
        "湿度 32%   |   北风 2级",
        "东风南风",
        "5日预报",
        "05/20",
        "今天明天周四周五周六",
        "26C / 14C",
        "今日建议",
        "穿衣出行紫外线",
        "薄外套适宜中等",
        "早晚微凉 建议",
        "搭配薄外套",
        "天气良好 适宜",
        "外出请做好防晒",
        "措施",
        "— 1 / 1 —"
    };
    const font_face_t *faces[] = {
        font_get_face(FONT_SIZE_14),
        font_get_face(FONT_SIZE_18),
        font_get_face(FONT_SIZE_20),
        font_get_face(FONT_SIZE_24)
    };

    for (size_t f = 0; f < sizeof(faces) / sizeof(faces[0]); f++) {
        for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
            assert_font_contains_text(faces[f], labels[i]);
        }
    }
}

static void test_calendar_builtin_font_contains_all_labels(void) {
    const char *labels[] = {
        "日历",
        "2026年5月",
        "一二三四五六日",
        "劳动节青年节立夏小满儿童节芒种",
        "初十十一十二十三十四十五十六十七十八十九二十廿一廿二廿三廿四廿五廿六廿七廿八廿九三十",
        "阳历 2026年5月21日 星期四",
        "农历 四月初四 丙午年 癸巳月 乙卯日",
        "宜读书学习写作订计划旅行散步",
        "忌搬家动土嫁娶安葬诉讼争吵",
        "今日事件",
        "09:00–10:30 读书计划：《时间简史》第三章",
        "19:30–20:30 英语复习：Unit 5 词汇与语法",
        "左右滑动切换月份"
    };
    const font_face_t *faces[] = {
        font_get_face(FONT_SIZE_12),
        font_get_face(FONT_SIZE_14),
        font_get_face(FONT_SIZE_16),
        font_get_face(FONT_SIZE_18),
        font_get_face(FONT_SIZE_20),
        font_get_face(FONT_SIZE_24)
    };

    for (size_t f = 0; f < sizeof(faces) / sizeof(faces[0]); f++) {
        for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
            assert_font_contains_text(faces[f], labels[i]);
        }
    }
}

static void test_calendar_page_matches_design2_skeleton(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_CALENDAR;
    app.calendar_month_offset = 0;
    app.calendar_selected_day = 21;
    ui_render_page(&fb, &app, &font);

    /* Calendar keeps a white status bar and removes the old title/navigation row. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 0, 0, GFX_WIDTH, TEST_STATUS_BAR_HEIGHT) < 1500);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 24, 48, 14, 28) < 8);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 426, 70, 24, 8) < 5);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 0, 84, GFX_WIDTH, 1) < 40);

    /* Month selector, weekday labels, and full 6-row grid. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 190, 47, 120, 26) > 120);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 20, 92, 430, 18) > 180);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 12, 112, 456, 1) > 430);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 12, 406, 456, 1) > 430);

    /* Date detail card, almanac rows, and event list extend to the bottom. */
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 24, 424));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 455, 424));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 24, 755));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 455, 755));
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 28, 444, 420, 88) > 600);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 28, 548, 420, 42) > 260);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 28, 604, 420, 70) > 500);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 150, 716, 210, 20) < 80);
    font_free(&font);
}

static void test_system_time_labels_are_not_hardcoded(void) {
    ASSERT_TRUE(!file_contains("src/ui/pages.c", "\"14:35"));
    ASSERT_TRUE(!file_contains("src/ui/pages.c", "\"09:41"));
    ASSERT_TRUE(!file_contains("src/ui/pages.c", "\"更新于 10:30\""));
    ASSERT_TRUE(!file_contains("src/ui/pages.c", "font_draw_text_builtin(18, fb, 24, 14, \"10:30\""));
    ASSERT_TRUE(!file_contains("src/ui/pages.c", "\"2026年5月21日 星期四\""));
}

static void test_calendar_page_selects_current_system_date(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;
    struct tm now_tm;
    struct tm first_tm;
    int first_weekday;
    int day_index;
    int selected_x;
    int selected_y;

    local_now_for_test(&now_tm);
    first_tm = now_tm;
    first_tm.tm_mday = 1;
    first_tm.tm_hour = 12;
    first_tm.tm_min = 0;
    first_tm.tm_sec = 0;
    first_tm.tm_isdst = -1;
    ASSERT_TRUE(mktime(&first_tm) != (time_t)-1);
    first_weekday = (first_tm.tm_wday + 6) % 7;
    day_index = first_weekday + now_tm.tm_mday - 1;
    selected_x = 12 + (day_index % 7) * 65;
    selected_y = 112 + (day_index / 7) * 49;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_CALENDAR;
    app.calendar_month_offset = 0;
    app.calendar_selected_day = 21;
    ui_render_page(&fb, &app, &font);

    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, selected_x, selected_y, 65, 49) > 2500);
    font_free(&font);
}

static void test_weather_page_matches_design2_skeleton(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_WEATHER;
    app.weather_city_index = 0;
    app.weather_type = 0;
    ui_render_page(&fb, &app, &font);

    assert_uses_home_status_bar(&fb);
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 0, TEST_STATUS_BAR_HEIGHT + 2));
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, GFX_WIDTH - 1, TEST_STATUS_BAR_HEIGHT + 2));

    /* Hero area: city and weather copy on the left, large icon/temperature on the right. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 36, 78, 150, 120) > 120);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 300, 80, 120, 170) > 220);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 300, 220, 130, 90) > 200);

    /* Forecast list starts below the hero divider and has row separators. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 15, 350, 450, 1) > 400);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 30, 370, 420, 300) > 500);
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 18, 448, 444, 1) > 390);

    /* Suggestion cards start below the first viewport and are reached by scrolling. */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 20, 740, 440, 50) > 100);
    font_free(&font);
}

static void test_weather_render_uses_scroll_offset_below_status_bar(void) {
    gfx_framebuffer_t top_fb;
    gfx_framebuffer_t scrolled_fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&top_fb);
    gfx_init(&scrolled_fb);
    app_init(&app);
    app.page = APP_PAGE_WEATHER;
    app.weather_scroll = 0;
    ui_render_page(&top_fb, &app, &font);

    app.weather_scroll = 120;
    ui_render_page(&scrolled_fb, &app, &font);

    assert_uses_home_status_bar(&scrolled_fb);
    ASSERT_TRUE(!framebuffers_equal(&top_fb, &scrolled_fb));
    ASSERT_TRUE(count_color_in_region(&top_fb, GFX_BLACK, 36, 78, 150, 120) > 120);
    ASSERT_TRUE(count_color_in_region(&scrolled_fb, GFX_BLACK, 36, 78, 150, 120) > 120);
    ASSERT_TRUE(count_color_in_region(&scrolled_fb, GFX_BLACK, 20, 620, 440, 130) > 700);
    ASSERT_TRUE(count_color_in_region(&scrolled_fb, GFX_BLACK, 30, 744, 420, 34) > 360);
    font_free(&font);
}

static void test_system_pages_ignore_loaded_external_fonts(void) {
    app_page_t pages[] = {
        APP_PAGE_HOME,
        APP_PAGE_WEATHER,
        APP_PAGE_CALENDAR,
        APP_PAGE_ENGLISH,
        APP_PAGE_SETTINGS,
        APP_PAGE_ABOUT
    };

    for (size_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
        gfx_framebuffer_t builtin_fb;
        gfx_framebuffer_t external_fb;
        app_state_t app;
        font_t font;

        font_manager_free_all();
        ASSERT_EQ_INT(1, font_load_default(&font));
        gfx_init(&builtin_fb);
        app_init(&app);
        app.page = pages[i];
        app.wifi_connected = 1;
        app.weather_city_index = 2;
        app.power_saving_enabled = 1;
        ui_render_page(&builtin_fb, &app, &font);

        ASSERT_TRUE(font_manager_load_dir("assets/fonts/external") > 0);
        gfx_init(&external_fb);
        ui_render_page(&external_fb, &app, &font);

        ASSERT_TRUE(framebuffers_equal(&builtin_fb, &external_fb));
        font_free(&font);
    }
    font_manager_free_all();
}

static void test_reader_body_uses_loaded_external_fonts(void) {
    gfx_framebuffer_t builtin_fb;
    gfx_framebuffer_t external_fb;
    app_state_t app;
    font_t font;

    font_manager_free_all();
    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&builtin_fb);
    app_init(&app);
    app.page = APP_PAGE_READER;
    app.reader_page = 0;
    ui_render_page(&builtin_fb, &app, &font);

    ASSERT_TRUE(font_manager_load_dir("assets/fonts/external") > 0);
    gfx_init(&external_fb);
    ui_render_page(&external_fb, &app, &font);

    ASSERT_TRUE(!framebuffers_equal(&builtin_fb, &external_fb));
    ASSERT_TRUE(count_color_in_region(&external_fb, GFX_BLACK, 58, 205, 360, 260) > 0);
    font_free(&font);
    font_manager_free_all();
}

static void test_reader_body_changes_when_external_font_choice_changes(void) {
    gfx_framebuffer_t font0_fb;
    gfx_framebuffer_t font1_fb;
    app_state_t app;
    font_t font;

    font_manager_free_all();
    ASSERT_EQ_INT(1, font_load_default(&font));
    ASSERT_TRUE(font_manager_load_dir("assets/fonts/external") > 0);

    app_init(&app);
    app.page = APP_PAGE_READER;
    app.reader_page = 0;
    app.reader_font_index = 0;
    gfx_init(&font0_fb);
    ui_render_page(&font0_fb, &app, &font);

    app.reader_font_index = 1;
    gfx_init(&font1_fb);
    ui_render_page(&font1_fb, &app, &font);

    ASSERT_TRUE(!framebuffers_equal(&font0_fb, &font1_fb));
    font_free(&font);
    font_manager_free_all();
}

int main(void) {
    test_framebuffer_has_ssd677_size();
    test_set_pixel_clips_out_of_bounds();
    test_rectangles_clip_and_place_black_pixels();
    test_display_commit_writes_480x800_ppm();
    test_epd_frame_pack_is_single_bw_plane();
    test_target_documents_reference_ssd677_and_no_game_module();
    test_esp_project_targets_ssd677_bw_panel();
    test_home_has_six_modules_and_no_game();
    test_settings_page_scrolls_with_up_down_buttons();
    test_weather_page_scrolls_while_changing_city();
    test_home_selection_uses_outline_frame_and_larger_icon();
    test_home_icons_use_modern_line_style_without_large_solid_blocks();
    test_home_status_bar_content_is_vertically_centered();
    test_non_reader_pages_use_home_status_bar_style();
    test_reader_keeps_reading_status_bar_style();
    test_bookshelf_and_reader_keep_progress();
    test_reader_menu_opens_reader_settings_and_applies();
    test_reader_menu_opens_full_catalog_page_and_selects_chapter();
    test_reader_settings_font_row_cycles_external_font_choice();
    test_bookshelf_matches_design2_grid_skeleton();
    test_reader_settings_matches_design2_skeleton();
    test_reader_catalog_matches_design2_skeleton();
    test_bookshelf_selection_uses_rounded_outline_frame();
    test_reader_library_exposes_books_and_pages();
    test_app_init_uses_reader_library_page_counts();
    test_reader_library_builds_pages_from_source_text();
    test_reader_library_loads_source_text_from_file();
    test_reader_library_auto_paginates_plain_text_file();
    test_bookshelf_reflects_loaded_realbook_metadata();
    test_reader_library_loads_external_real_books();
    test_entrypoints_load_external_books_on_startup();
    test_app_persistence_round_trips_reader_progress_and_settings();
    test_app_persistence_clamps_restored_values_to_current_limits();
    test_app_persistence_rejects_malformed_payloads();
    test_app_persistence_saves_and_loads_text_file();
    test_app_persistence_saves_and_loads_app_state_file();
    test_app_persistence_nvs_backend_is_stubbed_on_host();
    test_esp_firmware_wires_app_persistence_to_nvs();
    test_esp_input_wires_button_debounce();
    test_sdl_key_mapping_for_core_buttons();
    test_input_debounce_emits_once_after_stable_press();
    test_input_debounce_rearms_after_stable_release();
    test_input_debounce_short_press_emits_on_release();
    test_input_debounce_long_press_emits_once_while_held();
    test_icons_draw_black_pixels_only();
    test_primary_pages_render_nonblank();
    test_reader_body_renders_text_in_content_area();
    test_reader_main_matches_design_skeleton();
    test_settings_page_matches_design_skeleton();
    test_settings_render_uses_scroll_offset_below_status_bar();
    test_settings_builtin_font_contains_all_labels();
    test_reader_settings_builtin_font_contains_all_labels();
    test_reader_catalog_builtin_font_contains_all_labels();
    test_bookshelf_builtin_font_contains_realbook_labels();
    test_weather_builtin_font_contains_all_labels();
    test_calendar_builtin_font_contains_all_labels();
    test_weather_page_matches_design2_skeleton();
    test_calendar_page_matches_design2_skeleton();
    test_system_time_labels_are_not_hardcoded();
    test_calendar_page_selects_current_system_date();
    test_weather_render_uses_scroll_offset_below_status_bar();
    test_system_pages_ignore_loaded_external_fonts();
    test_reader_body_uses_loaded_external_fonts();
    test_reader_body_changes_when_external_font_choice_changes();
    puts("tests passed");
    return 0;
}
