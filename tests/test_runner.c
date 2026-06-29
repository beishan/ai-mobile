#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_bookshelf_renders_as_single_line_list_without_item_frames(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;

    ASSERT_EQ_INT(1, font_load_default(&font));
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_BOOKSHELF;
    app.bookshelf_selection = -1;
    ui_render_page(&fb, &app, &font);

    /* Grid layout: 3 columns x N rows. Card 0 at (x=24, y=50), card size w=128, h=220 (cover only) */
    /* No selection frame on any card */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 22, 48));   /* outside top-left of first card cover */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 154, 48));  /* outside top-right of first card cover */
    /* Content (large decorative character) is present in first card */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 24, 50, 128, 220) > 50);  /* cover area has content */
    ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 50);
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

    /* Grid layout: Card 1 is at col=1, row=0 (x=176, y=50, w=128, h=220 cover) */
    /* Selected card has thick rounded border (offset by -2, size +4): x=174, y=48, w=132, h=224, r=12 */
    /* Outside rounded corner is white */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 172, 46));  /* outside top-left corner */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 308, 46));  /* outside top-right corner */
    /* Top border is black (straight part of top edge) */
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 220, 48));  /* inside top border */
    /* Cover area has decorative content (large character) */
    ASSERT_TRUE(count_color_in_region(&fb, GFX_BLACK, 176, 50, 128, 220) > 50);  /* cover has content */
    /* Card 0 has no selection frame */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 22, 48));   /* outside card 0 */
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 154, 48));  /* outside card 0 */
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

static void test_app_persistence_round_trips_reader_progress_and_settings(void) {
    app_state_t app;
    app_state_t restored;
    app_persisted_state_t snapshot;
    app_persisted_state_t decoded;
    char encoded[APP_PERSISTENCE_TEXT_MAX];

    app_init(&app);
    app.current_book = 1;
    app.recent_book = 1;
    app.book_current_pages[0] = 2;
    app.book_current_pages[1] = 1;
    app.book_current_pages[2] = 2;
    app.book_bookmark_pages[0] = -1;
    app.book_bookmark_pages[1] = 1;
    app.book_bookmark_pages[2] = 2;
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
    ASSERT_EQ_INT(2, restored.book_current_pages[0]);
    ASSERT_EQ_INT(1, restored.book_current_pages[1]);
    ASSERT_EQ_INT(2, restored.book_current_pages[2]);
    ASSERT_EQ_INT(-1, restored.book_bookmark_pages[0]);
    ASSERT_EQ_INT(1, restored.book_bookmark_pages[1]);
    ASSERT_EQ_INT(2, restored.book_bookmark_pages[2]);
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
    app.book_current_pages[1] = 2;
    app.book_bookmark_pages[1] = 2;
    app.reader_page = 2;
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
    ASSERT_EQ_INT(2, restored.book_current_pages[1]);
    ASSERT_EQ_INT(2, restored.book_bookmark_pages[1]);
    ASSERT_EQ_INT(2, restored.reader_page);
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

int main(void) {
    test_framebuffer_has_ssd677_size();
    test_set_pixel_clips_out_of_bounds();
    test_rectangles_clip_and_place_black_pixels();
    test_display_commit_writes_480x800_ppm();
    test_epd_frame_pack_is_single_bw_plane();
    test_target_documents_reference_ssd677_and_no_game_module();
    test_esp_project_targets_ssd677_bw_panel();
    test_home_has_six_modules_and_no_game();
    test_home_selection_uses_outline_frame_and_larger_icon();
    test_home_icons_use_modern_line_style_without_large_solid_blocks();
    test_home_status_bar_content_is_vertically_centered();
    test_bookshelf_and_reader_keep_progress();
    test_bookshelf_renders_as_single_line_list_without_item_frames();
    test_bookshelf_selection_uses_rounded_outline_frame();
    test_reader_library_exposes_books_and_pages();
    test_app_init_uses_reader_library_page_counts();
    test_reader_library_builds_pages_from_source_text();
    test_reader_library_loads_source_text_from_file();
    test_reader_library_auto_paginates_plain_text_file();
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
    puts("tests passed");
    return 0;
}
