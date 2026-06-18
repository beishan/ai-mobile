#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/gfx.h"
#include "platform/sim_display.h"
#include "platform/sdl_display.h"
#include "app/app_state.h"
#include "ui/pages.h"
#include "ui/icons.h"
#include "font/font.h"

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ_INT(expected, actual) ASSERT_TRUE((expected) == (actual))

static int count_color(const gfx_framebuffer_t *fb, gfx_color_t color);
static int file_contains(const char *path, const char *needle);

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

static void test_esp_project_files_exist(void) {
    ASSERT_TRUE(file_contains("platformio.ini", "framework = espidf"));
    ASSERT_TRUE(file_contains("platformio.ini", "esp32-n16r8"));
    ASSERT_TRUE(file_contains("platformio.ini", "-DBOARD_HAS_PSRAM"));
    ASSERT_TRUE(file_contains("partitions_16mb.csv", "app0"));
    ASSERT_TRUE(file_contains("sdkconfig.defaults", "CONFIG_SPIRAM"));
    ASSERT_TRUE(file_contains("sdkconfig.defaults", "CONFIG_ESPTOOLPY_FLASHSIZE_16MB"));
    ASSERT_TRUE(file_contains("CMakeLists.txt", "project(ai_mobile_reader)"));
}

static void test_esp_cmake_reuses_shared_ui_sources(void) {
    ASSERT_TRUE(file_contains("src/CMakeLists.txt", "main_esp.c"));
    ASSERT_TRUE(file_contains("src/CMakeLists.txt", "gfx/gfx.c"));
    ASSERT_TRUE(file_contains("src/CMakeLists.txt", "app/app_state.c"));
    ASSERT_TRUE(file_contains("src/CMakeLists.txt", "ui/pages.c"));
    ASSERT_TRUE(file_contains("src/CMakeLists.txt", "platform/esp_display.c"));
}

static void test_esp_epd_wiring_is_centralized_and_documented(void) {
    ASSERT_TRUE(file_contains("src/platform/esp_board_config.h", "#define ESP_EPD_PIN_BUSY 4"));
    ASSERT_TRUE(file_contains("src/platform/esp_board_config.h", "#define ESP_EPD_PIN_RST 16"));
    ASSERT_TRUE(file_contains("src/platform/esp_board_config.h", "#define ESP_EPD_PIN_DC 17"));
    ASSERT_TRUE(file_contains("src/platform/esp_board_config.h", "#define ESP_EPD_PIN_CS 5"));
    ASSERT_TRUE(file_contains("src/platform/esp_board_config.h", "#define ESP_EPD_PIN_SCK 18"));
    ASSERT_TRUE(file_contains("src/platform/esp_board_config.h", "#define ESP_EPD_PIN_SDA 23"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "esp_board_config.h"));
    ASSERT_TRUE(file_contains("README.md", "BUSY | GPIO4"));
    ASSERT_TRUE(file_contains("docs/OPERATION_MANUAL.md", "BUSY | GPIO4"));
}

static void test_esp_display_initializes_gpio_and_spi_bus(void) {
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "driver/gpio.h"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "driver/spi_master.h"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "gpio_config("));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "spi_bus_initialize("));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "ESP_EPD_SPI_HOST"));
    ASSERT_TRUE(file_contains("src/CMakeLists.txt", "esp_driver_gpio"));
    ASSERT_TRUE(file_contains("src/CMakeLists.txt", "esp_driver_spi"));
}

static void test_esp_display_has_controller_primitives(void) {
    ASSERT_TRUE(file_contains("src/platform/esp_display.h", "esp_display_reset"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.h", "esp_display_wait_busy"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.h", "esp_display_send_command"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.h", "esp_display_send_data"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "gpio_get_level(ESP_EPD_PIN_BUSY)"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "vTaskDelay(pdMS_TO_TICKS"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "spi_device_transmit"));
    ASSERT_TRUE(file_contains("src/platform/esp_display.c", "ESP_EPD_BUSY_TIMEOUT_MS"));
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

static void test_default_font_has_reader_catalog_glyphs(void) {
    const font_face_t *font = font_get_face(FONT_SIZE_16);
    ASSERT_TRUE(font_find_glyph(font, 0x7aef) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x6298) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x58f0) != NULL);
}

static void test_default_font_has_weather_flow_glyphs(void) {
    const font_face_t *font = font_get_face(FONT_SIZE_16);
    ASSERT_TRUE(font_find_glyph(font, 0x6d77) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x96e8) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x5e7f) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x7f13) != NULL);
}

static void test_default_font_has_calendar_detail_glyphs(void) {
    const font_face_t *font = font_get_face(FONT_SIZE_16);
    ASSERT_TRUE(font_find_glyph(font, 0x8be6) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x5b9c) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x590f) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x81f3) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x65e0) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x63d0) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x9192) != NULL);
}

static void test_default_font_has_english_flow_glyphs(void) {
    const font_face_t *font = font_get_face(FONT_SIZE_16);
    ASSERT_TRUE(font_find_glyph(font, 0x8bc6) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x7f18) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x5de7) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x9605) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x5668) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x7eb8) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x590d) != NULL);
}

static void test_default_font_has_game_flow_glyphs(void) {
    const font_face_t *font = font_get_face(FONT_SIZE_16);
    ASSERT_TRUE(font_find_glyph(font, 0x8f6c) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x5411) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x8fdb) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x7ed3) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x675f) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x91cd) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x59cb) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x9884) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x89c8) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x529f) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x80fd) != NULL);
}

static void test_default_font_has_settings_flow_glyphs(void) {
    const font_face_t *font = font_get_face(FONT_SIZE_16);
    ASSERT_TRUE(font_find_glyph(font, 0x5b8b) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x7d27) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x51d1) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x6807) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x51c6) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x8212) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x9002) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x5bbd) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x677e) != NULL);
}

static void test_default_font_has_bookshelf_flow_glyphs(void) {
    const font_face_t *font = font_get_face(FONT_SIZE_16);
    ASSERT_TRUE(font_find_glyph(font, 0x6700) != NULL);
    ASSERT_TRUE(font_find_glyph(font, 0x8fd1) != NULL);
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

static void test_font_faces_have_compact_ascii_metrics(void) {
    const font_face_t *small = font_get_face(FONT_SIZE_12);
    const font_face_t *body = font_get_face(FONT_SIZE_20);
    const font_face_t *reader_large = font_get_face(FONT_SIZE_24);
    const font_glyph_t *digit = font_find_glyph(small, '1');
    const font_glyph_t *han = font_find_glyph(small, 0x9605);
    ASSERT_TRUE(small != NULL);
    ASSERT_TRUE(body != NULL);
    ASSERT_TRUE(reader_large != NULL);
    ASSERT_TRUE(digit != NULL);
    ASSERT_TRUE(han != NULL);
    ASSERT_TRUE(digit->advance < han->advance);
    ASSERT_EQ_INT(20, body->size);
    ASSERT_EQ_INT(24, reader_large->size);
}

static void test_text_box_wraps_chinese_body_text(void) {
    gfx_framebuffer_t fb;
    const font_face_t *body = font_get_face(FONT_SIZE_20);
    int lower_pixels = 0;
    gfx_init(&fb);
    font_draw_text_box(body, &fb, 0, 0, 80, 60, "这是一个宁静的夜晚", GFX_BLACK);
    ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 50);
    for (int y = 25; y < 55; y++) {
        for (int x = 0; x < 80; x++) {
            if (gfx_get_pixel(&fb, x, y) == GFX_BLACK) {
                lower_pixels++;
            }
        }
    }
    ASSERT_TRUE(lower_pixels > 20);
}

static void test_text_box_line_spacing_changes_vertical_distribution(void) {
    gfx_framebuffer_t tight;
    gfx_framebuffer_t loose;
    const font_face_t *body = font_get_face(FONT_SIZE_16);
    int tight_lower_pixels = 0;
    int loose_lower_pixels = 0;
    gfx_init(&tight);
    gfx_init(&loose);
    font_draw_text_box_spaced(body, &tight, 0, 0, 64, 120, "这是一个宁静的夜晚城市灯光闪烁", body->line_height, GFX_BLACK);
    font_draw_text_box_spaced(body, &loose, 0, 0, 64, 120, "这是一个宁静的夜晚城市灯光闪烁", body->line_height + 10, GFX_BLACK);
    for (int y = 70; y < 120; y++) {
        for (int x = 0; x < 64; x++) {
            if (gfx_get_pixel(&tight, x, y) == GFX_BLACK) {
                tight_lower_pixels++;
            }
            if (gfx_get_pixel(&loose, x, y) == GFX_BLACK) {
                loose_lower_pixels++;
            }
        }
    }
    ASSERT_TRUE(tight_lower_pixels != loose_lower_pixels);
}

static void test_weather_refresh_changes_counter(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_WEATHER;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.weather_refreshes);
    ASSERT_EQ_INT(0, app.weather_stale);
    ASSERT_EQ_INT(0, app.weather_last_updated_minutes);
}

static void test_weather_city_selection_wraps(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_WEATHER;
    ASSERT_EQ_INT(0, app.weather_city_index);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.weather_city_index);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(0, app.weather_city_index);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(2, app.weather_city_index);
}

static void test_weather_offline_refresh_marks_stale_cache(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_WEATHER;
    app.wifi_connected = 0;
    app.weather_last_updated_minutes = 30;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(0, app.weather_refreshes);
    ASSERT_EQ_INT(1, app.weather_stale);
    ASSERT_EQ_INT(60, app.weather_last_updated_minutes);
}

static void test_calendar_month_selection_changes_month(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_CALENDAR;
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.calendar_month_offset);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(0, app.calendar_month_offset);
    ASSERT_EQ_INT(15, app.calendar_selected_day);
}

static void test_calendar_detail_day_selection_moves_by_week(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_CALENDAR;
    app.calendar_detail_open = 1;
    ASSERT_EQ_INT(15, app.calendar_selected_day);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(22, app.calendar_selected_day);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(15, app.calendar_selected_day);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(8, app.calendar_selected_day);
}

static void test_calendar_home_toggles_detail(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_CALENDAR;
    ASSERT_EQ_INT(0, app.calendar_detail_open);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.calendar_detail_open);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(0, app.calendar_detail_open);
}

static void test_english_flip_changes_state(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_ENGLISH;
    ASSERT_EQ_INT(0, app.english_show_back);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.english_show_back);
}

static void test_english_known_answer_advances_and_counts(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_ENGLISH;
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.english_known_count);
    ASSERT_EQ_INT(0, app.english_review_count);
    ASSERT_EQ_INT(1, app.english_word);
    ASSERT_EQ_INT(0, app.english_show_back);
    ASSERT_EQ_INT(1, app.english_answer_state[0]);
}

static void test_english_unknown_answer_marks_review(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_ENGLISH;
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_UP);
    ASSERT_EQ_INT(0, app.english_known_count);
    ASSERT_EQ_INT(1, app.english_review_count);
    ASSERT_EQ_INT(1, app.english_word);
    ASSERT_EQ_INT(0, app.english_show_back);
    ASSERT_EQ_INT(2, app.english_answer_state[0]);
}

static void test_game_home_starts_snake_from_list(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_SNAKE;
    ASSERT_EQ_INT(0, app.snake_running);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.snake_running);
    ASSERT_EQ_INT(0, app.snake_game_over);
    ASSERT_EQ_INT(0, app.snake_score);
}

static void test_snake_running_moves_and_turns(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_SNAKE;
    app_handle_button(&app, APP_BUTTON_HOME);
    int y = app.snake_y;
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.snake_direction);
    ASSERT_TRUE(app.snake_y > y);
}

static void test_snake_eats_food_and_scores(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_SNAKE;
    app.snake_running = 1;
    app.snake_x = 27;
    app.snake_y = 7;
    app.snake_direction = 0;
    app.snake_food_x = 28;
    app.snake_food_y = 7;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(10, app.snake_score);
    ASSERT_TRUE(app.snake_food_x != 28 || app.snake_food_y != 7);
}

static void test_snake_hits_wall_and_stops(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_SNAKE;
    app.snake_running = 1;
    app.snake_x = 35;
    app.snake_y = 5;
    app.snake_direction = 0;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(0, app.snake_running);
    ASSERT_EQ_INT(1, app.snake_game_over);
}

static void test_reader_home_toggles_menu(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;
    ASSERT_EQ_INT(0, app.reader_menu_open);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.reader_menu_open);
    ASSERT_EQ_INT(0, app.reader_menu_selection);
}

static void test_reader_menu_navigates_and_exits_to_bookshelf(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(3, app.reader_menu_selection);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_BOOKSHELF, app.page);
    ASSERT_EQ_INT(0, app.reader_menu_open);
}

static void test_bookshelf_opens_selected_book_and_keeps_progress(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_BOOKSHELF;
    app.bookshelf_selection = 1;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_READER, app.page);
    ASSERT_EQ_INT(1, app.current_book);
    ASSERT_EQ_INT(app.book_current_pages[1], app.reader_page);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.book_current_pages[1]);
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_BOOKSHELF, app.page);
    ASSERT_EQ_INT(1, app.book_current_pages[1]);
}

static void test_bookshelf_tracks_recent_book(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_BOOKSHELF;
    app.bookshelf_selection = 1;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.recent_book);
    app.page = APP_PAGE_BOOKSHELF;
    app.bookshelf_selection = 2;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(2, app.recent_book);
}

static void test_reader_menu_power_closes_without_leaving_reader(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_POWER);
    ASSERT_EQ_INT(APP_PAGE_READER, app.page);
    ASSERT_EQ_INT(0, app.reader_menu_open);
}

static void test_reader_menu_adds_bookmark(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;
    app.current_book = 2;
    app.reader_page = 3;
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(APP_PAGE_READER, app.page);
    ASSERT_EQ_INT(0, app.reader_menu_open);
    ASSERT_EQ_INT(3, app.book_bookmark_pages[2]);
}

static void test_reader_catalog_jumps_to_selected_chapter(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;
    app.current_book = 0;
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.reader_menu_open);
    ASSERT_EQ_INT(1, app.reader_catalog_open);
    app_handle_button(&app, APP_BUTTON_DOWN);
    ASSERT_EQ_INT(1, app.reader_catalog_selection);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(0, app.reader_menu_open);
    ASSERT_EQ_INT(0, app.reader_catalog_open);
    ASSERT_EQ_INT(2, app.reader_page);
    ASSERT_EQ_INT(2, app.book_current_pages[0]);
}

static void test_reader_catalog_power_returns_to_reader_menu(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;
    app_handle_button(&app, APP_BUTTON_HOME);
    app_handle_button(&app, APP_BUTTON_DOWN);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.reader_catalog_open);
    app_handle_button(&app, APP_BUTTON_POWER);
    ASSERT_EQ_INT(APP_PAGE_READER, app.page);
    ASSERT_EQ_INT(1, app.reader_menu_open);
    ASSERT_EQ_INT(0, app.reader_catalog_open);
}

static void test_reader_power_without_menu_keeps_reader(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_READER;
    app_handle_button(&app, APP_BUTTON_POWER);
    ASSERT_EQ_INT(APP_PAGE_READER, app.page);
}

static void test_reader_render_leaves_bottom_hint_area_blank(void) {
    gfx_framebuffer_t fb;
    app_state_t app;
    font_t font;
    int colored = 0;
    gfx_init(&fb);
    app_init(&app);
    app.page = APP_PAGE_READER;
    ASSERT_EQ_INT(1, font_load_default(&font));
    ui_render_page(&fb, &app, &font);
    for (int y = 226; y < 286; y++) {
        for (int x = 0; x < GFX_WIDTH; x++) {
            if (gfx_get_pixel(&fb, x, y) != GFX_WHITE) {
                colored++;
            }
        }
    }
    ASSERT_EQ_INT(0, colored);
    font_free(&font);
}

static void test_reader_render_uses_settings_font_size(void) {
    gfx_framebuffer_t small_fb;
    gfx_framebuffer_t large_fb;
    app_state_t app;
    font_t font;
    int small_pixels = 0;
    int large_pixels = 0;
    gfx_init(&small_fb);
    gfx_init(&large_fb);
    app_init(&app);
    app.page = APP_PAGE_READER;
    ASSERT_EQ_INT(1, font_load_default(&font));
    app.font_size_index = 0;
    ui_render_page(&small_fb, &app, &font);
    app.font_size_index = 4;
    ui_render_page(&large_fb, &app, &font);
    for (int y = 42; y < 204; y++) {
        for (int x = 18; x < 382; x++) {
            if (gfx_get_pixel(&small_fb, x, y) == GFX_BLACK) {
                small_pixels++;
            }
            if (gfx_get_pixel(&large_fb, x, y) == GFX_BLACK) {
                large_pixels++;
            }
        }
    }
    ASSERT_TRUE(small_pixels != large_pixels);
    font_free(&font);
}

static void test_reader_render_uses_settings_line_spacing(void) {
    gfx_framebuffer_t tight_fb;
    gfx_framebuffer_t loose_fb;
    app_state_t app;
    font_t font;
    int tight_lower_pixels = 0;
    int loose_lower_pixels = 0;
    gfx_init(&tight_fb);
    gfx_init(&loose_fb);
    app_init(&app);
    app.page = APP_PAGE_READER;
    app.font_size_index = 1;
    ASSERT_EQ_INT(1, font_load_default(&font));
    app.line_spacing_index = 0;
    ui_render_page(&tight_fb, &app, &font);
    app.line_spacing_index = 3;
    ui_render_page(&loose_fb, &app, &font);
    for (int y = 120; y < 204; y++) {
        for (int x = 18; x < 382; x++) {
            if (gfx_get_pixel(&tight_fb, x, y) == GFX_BLACK) {
                tight_lower_pixels++;
            }
            if (gfx_get_pixel(&loose_fb, x, y) == GFX_BLACK) {
                loose_lower_pixels++;
            }
        }
    }
    ASSERT_TRUE(tight_lower_pixels != loose_lower_pixels);
    font_free(&font);
}

static void test_settings_cycles_font_size(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_SETTINGS;
    app.settings_selection = 0;
    int before = app.font_size_index;
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_TRUE(app.font_size_index != before);
}

static void test_settings_cycles_city_for_weather(void) {
    app_state_t app;
    app_init(&app);
    app.page = APP_PAGE_SETTINGS;
    app.settings_selection = 4;
    ASSERT_EQ_INT(0, app.weather_city_index);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(1, app.weather_city_index);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(2, app.weather_city_index);
    app_handle_button(&app, APP_BUTTON_HOME);
    ASSERT_EQ_INT(0, app.weather_city_index);
}

static void test_home_can_open_all_modules(void) {
    app_page_t expected[] = {
        APP_PAGE_BOOKSHELF,
        APP_PAGE_WEATHER,
        APP_PAGE_CALENDAR,
        APP_PAGE_SNAKE,
        APP_PAGE_ENGLISH,
        APP_PAGE_SETTINGS,
        APP_PAGE_ABOUT
    };
    for (int i = 0; i < 7; i++) {
        app_state_t app;
        app_init(&app);
        app.home_selection = i;
        app_handle_button(&app, APP_BUTTON_HOME);
        ASSERT_EQ_INT(expected[i], app.page);
    }
}

static void test_sdl_key_mapping_for_core_buttons(void) {
    ASSERT_EQ_INT(APP_BUTTON_UP, sdl_display_button_from_key(SDLK_UP));
    ASSERT_EQ_INT(APP_BUTTON_DOWN, sdl_display_button_from_key(SDLK_s));
    ASSERT_EQ_INT(APP_BUTTON_HOME, sdl_display_button_from_key(SDLK_RETURN));
    ASSERT_EQ_INT(APP_BUTTON_POWER, sdl_display_button_from_key(SDLK_BACKSPACE));
}

static void test_home_icons_draw_distinct_pixels(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    ui_draw_icon(&fb, UI_ICON_READER, 10, 10, 0);
    ui_draw_icon(&fb, UI_ICON_WEATHER, 70, 10, 0);
    ui_draw_icon(&fb, UI_ICON_CALENDAR, 130, 10, 0);
    ASSERT_TRUE(count_color(&fb, GFX_BLACK) > 100);
    ASSERT_TRUE(count_color(&fb, GFX_RED) > 10);
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
    test_esp_project_files_exist();
    test_esp_cmake_reuses_shared_ui_sources();
    test_esp_epd_wiring_is_centralized_and_documented();
    test_esp_display_initializes_gpio_and_spi_bus();
    test_esp_display_has_controller_primitives();
    test_home_selection_wraps_and_opens_weather();
    test_power_returns_function_page_to_home();
    test_reader_page_bounds();
    test_settings_toggles_power_saving();
    test_utf8_decoder_reads_ascii_and_chinese();
    test_utf8_decoder_replaces_invalid_bytes();
    test_default_font_has_chinese_glyphs();
    test_default_font_has_reader_catalog_glyphs();
    test_default_font_has_weather_flow_glyphs();
    test_default_font_has_calendar_detail_glyphs();
    test_default_font_has_english_flow_glyphs();
    test_default_font_has_game_flow_glyphs();
    test_default_font_has_settings_flow_glyphs();
    test_default_font_has_bookshelf_flow_glyphs();
    test_font_draw_text_places_pixels_for_chinese();
    test_font_faces_have_compact_ascii_metrics();
    test_text_box_wraps_chinese_body_text();
    test_text_box_line_spacing_changes_vertical_distribution();
    test_weather_refresh_changes_counter();
    test_weather_city_selection_wraps();
    test_weather_offline_refresh_marks_stale_cache();
    test_calendar_month_selection_changes_month();
    test_calendar_detail_day_selection_moves_by_week();
    test_calendar_home_toggles_detail();
    test_english_flip_changes_state();
    test_english_known_answer_advances_and_counts();
    test_english_unknown_answer_marks_review();
    test_game_home_starts_snake_from_list();
    test_snake_running_moves_and_turns();
    test_snake_eats_food_and_scores();
    test_snake_hits_wall_and_stops();
    test_reader_home_toggles_menu();
    test_reader_menu_navigates_and_exits_to_bookshelf();
    test_bookshelf_opens_selected_book_and_keeps_progress();
    test_bookshelf_tracks_recent_book();
    test_reader_menu_power_closes_without_leaving_reader();
    test_reader_menu_adds_bookmark();
    test_reader_catalog_jumps_to_selected_chapter();
    test_reader_catalog_power_returns_to_reader_menu();
    test_reader_power_without_menu_keeps_reader();
    test_reader_render_leaves_bottom_hint_area_blank();
    test_reader_render_uses_settings_font_size();
    test_reader_render_uses_settings_line_spacing();
    test_settings_cycles_font_size();
    test_settings_cycles_city_for_weather();
    test_home_can_open_all_modules();
    test_sdl_key_mapping_for_core_buttons();
    test_home_icons_draw_distinct_pixels();
    test_home_render_uses_black_and_red();
    test_each_primary_page_renders_nonblank();
    puts("tests passed");
    return 0;
}
