#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_state.h"
#include "font/font.h"
#include "gfx/gfx.h"
#include "platform/epd_frame.h"
#include "platform/sdl_display.h"
#include "platform/sim_display.h"
#include "ui/icons.h"
#include "ui/pages.h"

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ_INT(expected, actual) ASSERT_TRUE((expected) == (actual))

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
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_BOOKSHELF, app.page);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.reader_page);
}

static void test_sdl_key_mapping_for_core_buttons(void) {
    ASSERT_EQ_INT(APP_BUTTON_UP, sdl_display_button_from_key(SDLK_UP));
    ASSERT_EQ_INT(APP_BUTTON_DOWN, sdl_display_button_from_key(SDLK_s));
    ASSERT_EQ_INT(APP_BUTTON_HOME, sdl_display_button_from_key(SDLK_RETURN));
    ASSERT_EQ_INT(APP_BUTTON_POWER, sdl_display_button_from_key(SDLK_BACKSPACE));
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

int main(void) {
    test_framebuffer_has_ssd677_size();
    test_set_pixel_clips_out_of_bounds();
    test_rectangles_clip_and_place_black_pixels();
    test_display_commit_writes_480x800_ppm();
    test_epd_frame_pack_is_single_bw_plane();
    test_target_documents_reference_ssd677_and_no_game_module();
    test_esp_project_targets_ssd677_bw_panel();
    test_home_has_six_modules_and_no_game();
    test_bookshelf_and_reader_keep_progress();
    test_sdl_key_mapping_for_core_buttons();
    test_icons_draw_black_pixels_only();
    test_primary_pages_render_nonblank();
    puts("tests passed");
    return 0;
}
