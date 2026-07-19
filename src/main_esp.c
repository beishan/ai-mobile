#include "app/app_persistence.h"
#include "app/reader_library.h"
#include "app/app_state.h"
#include "font/font.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx/gfx.h"
#include "platform/esp_display.h"
#include "platform/esp_input.h"
#include "platform/esp_board_config.h"
#include "platform/esp_battery.h"
#include "platform/esp_config_backup.h"
#include "platform/esp_sd.h"
#include "platform/esp_time_sync.h"
#include "platform/esp_web_admin.h"
#include "platform/esp_weather.h"
#include "ui/pages.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

static const char *TAG = "ai_mobile";

#define APP_NVS_NAMESPACE "reader"
#define APP_NVS_KEY "app_state"
#define SD_LIBRARY_TASK_STACK_SIZE 12288
#define READER_PAGINATION_TASK_STACK_SIZE 6144
#define ESP_SD_FONT_DIRECTORY ESP_SD_MOUNT_POINT "/fonts"
#define ESP_SD_CONFIG_DIRECTORY ESP_SD_MOUNT_POINT "/.ai_mobile"
#define ESP_SD_APP_CONFIG_PATH ESP_SD_CONFIG_DIRECTORY "/app_state.cfg"
#define ESP_SD_APP_CONFIG_TEMP ESP_SD_CONFIG_DIRECTORY "/app_state.tmp"
#define ESP_SD_APP_CONFIG_BACKUP ESP_SD_CONFIG_DIRECTORY "/app_state.bak"

enum {
    SD_LIBRARY_IDLE = 0,
    SD_LIBRARY_LOADING,
    SD_LIBRARY_COMPLETE,
    SD_LIBRARY_READY
};

static volatile int sd_library_state = SD_LIBRARY_IDLE;
static volatile int sd_library_loaded_count;
static volatile int sd_library_progress;
static volatile int sd_library_total_count;

static int save_app_configuration(const app_state_t *app, int sd_mounted) {
    int nvs_result = app_persistence_save_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, app);
    if (sd_mounted) {
        if (app_persistence_save_app_file(ESP_SD_APP_CONFIG_TEMP, app) == 0) {
            remove(ESP_SD_APP_CONFIG_BACKUP);
            rename(ESP_SD_APP_CONFIG_PATH, ESP_SD_APP_CONFIG_BACKUP);
            if (rename(ESP_SD_APP_CONFIG_TEMP, ESP_SD_APP_CONFIG_PATH) != 0) {
                rename(ESP_SD_APP_CONFIG_BACKUP, ESP_SD_APP_CONFIG_PATH);
                ESP_LOGW(TAG, "failed to commit app configuration backup to SD");
            } else {
                remove(ESP_SD_APP_CONFIG_BACKUP);
            }
        } else {
            ESP_LOGW(TAG, "failed to write app configuration backup to SD");
        }
    }
    return nvs_result;
}

static void sd_library_progress_callback(int book_index, int percent, void *context) {
    int total = sd_library_total_count;
    (void)context;
    if (total <= 0) {
        sd_library_progress = 100;
        return;
    }
    sd_library_progress = (book_index * 100 + percent) / total;
    if (sd_library_progress > 100) sd_library_progress = 100;
}

enum {
    READER_PAGINATION_IDLE = 0,
    READER_PAGINATION_RUNNING,
    READER_PAGINATION_COMPLETE
};

static volatile int reader_pagination_state = READER_PAGINATION_IDLE;
static volatile int reader_pagination_result;
static volatile int reader_pagination_book_index = -1;

static void reader_pagination_task(void *arg) {
    int book_index = (int)(intptr_t)arg;
    reader_pagination_result = reader_library_build_book_layout_background(book_index);
    reader_pagination_state = READER_PAGINATION_COMPLETE;
    vTaskDelete(NULL);
}

static void start_reader_background_pagination(int book_index) {
    if (reader_pagination_state != READER_PAGINATION_IDLE ||
        reader_library_book_layout_complete(book_index)) return;
    reader_pagination_state = READER_PAGINATION_RUNNING;
    reader_pagination_book_index = book_index;
    if (xTaskCreatePinnedToCore(reader_pagination_task, "page_rest",
                                READER_PAGINATION_TASK_STACK_SIZE,
                                (void *)(intptr_t)book_index,
                                tskIDLE_PRIORITY + 1, NULL, 0) != pdPASS) {
        reader_pagination_state = READER_PAGINATION_IDLE;
        reader_pagination_book_index = -1;
        ESP_LOGE(TAG, "failed to start background pagination task");
    } else {
        ESP_LOGI(TAG, "first pages ready; continuing pagination in background");
    }
}

static void sd_library_load_task(void *arg) {
    (void)arg;
    sd_library_total_count = reader_library_count_directory(ESP_SD_BOOK_DIRECTORY);
    sd_library_progress = 0;
    reader_library_set_progress_callback(sd_library_progress_callback, NULL);
    sd_library_loaded_count = reader_library_load_directory(ESP_SD_BOOK_DIRECTORY);
    reader_library_set_progress_callback(NULL, NULL);
    sd_library_progress = 100;
    sd_library_state = SD_LIBRARY_COMPLETE;
    vTaskDelete(NULL);
}

static void init_nvs_storage(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase before initialization: %s", esp_err_to_name(err));
        if (nvs_flash_erase() == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS persistence unavailable: %s", esp_err_to_name(err));
    }
}

static int present_home_selection_change(esp_display_t *display,
                                         const gfx_framebuffer_t *fb,
                                         int previous_selection,
                                         int current_selection) {
    int old_x;
    int old_y;
    int old_w;
    int old_h;
    int new_x;
    int new_y;
    int new_w;
    int new_h;
    const int padding = 4;

    if (ui_home_tile_bounds(previous_selection, &old_x, &old_y, &old_w, &old_h) != 0 ||
        ui_home_tile_bounds(current_selection, &new_x, &new_y, &new_w, &new_h) != 0) {
        return -1;
    }
    if (esp_display_present_partial(display, fb, old_x - padding, old_y - padding,
                                    old_w + padding * 2, old_h + padding * 2) != 0 ||
        esp_display_present_partial(display, fb, new_x - padding, new_y - padding,
                                    new_w + padding * 2, new_h + padding * 2) != 0) {
        return -1;
    }
    ESP_LOGI(TAG, "home selection partial refresh: %d -> %d",
             previous_selection, current_selection);
    return 0;
}

static int present_reader_menu_selection_change(esp_display_t *display,
                                                const gfx_framebuffer_t *fb,
                                                int previous_selection,
                                                int current_selection) {
    const int menu_x = 80;
    const int menu_y = 328;
    const int selection_x = menu_x + 8;
    const int selection_width = 304;
    const int selection_height = 40;
    const int padding = 4;
    int previous_y = menu_y + 20 + previous_selection * 40 - 4;
    int current_y = menu_y + 20 + current_selection * 40 - 4;

    if (esp_display_present_partial(display, fb, selection_x - padding, previous_y - padding,
                                    selection_width + padding * 2, selection_height + padding * 2) != 0 ||
        esp_display_present_partial(display, fb, selection_x - padding, current_y - padding,
                                    selection_width + padding * 2, selection_height + padding * 2) != 0) {
        return -1;
    }
    return 0;
}

static int present_two_windows(esp_display_t *display, const gfx_framebuffer_t *fb,
                               int x1, int y1, int w1, int h1,
                               int x2, int y2, int w2, int h2) {
    if (esp_display_present_partial(display, fb, x1, y1, w1, h1) != 0 ||
        esp_display_present_partial(display, fb, x2, y2, w2, h2) != 0) {
        return -1;
    }
    return 0;
}

static int font_picker_first_row(int selection, int count) {
    const int visible_rows = 9;
    int first;
    if (count < 1) count = 1;
    if (selection < 0 || selection >= count) selection = 0;
    first = selection - visible_rows / 2;
    if (first < 0) first = 0;
    if (first > count - visible_rows) first = count - visible_rows;
    return first < 0 ? 0 : first;
}

static int present_system_font_selection_change(esp_display_t *display,
                                                const gfx_framebuffer_t *fb,
                                                int previous_selection,
                                                int current_selection) {
    const int row_top = 126;
    const int row_height = 62;
    int count = font_manager_family_count();
    int old_first = font_picker_first_row(previous_selection, count);
    int new_first = font_picker_first_row(current_selection, count);
    if (old_first != new_first) {
        return esp_display_present_partial(display, fb, 20, 120, 440, 570);
    }
    return present_two_windows(display, fb,
                               20, row_top + (previous_selection - old_first) * row_height - 4,
                               440, row_height + 2,
                               20, row_top + (current_selection - new_first) * row_height - 4,
                               440, row_height + 2);
}

static int bookshelf_card_bounds(int index, int layout,
                                 int *x, int *y, int *width, int *height) {
    const int cover_w = 124;
    const int cover_h = 170;
    if (index < 0 || index >= APP_BOOK_COUNT || x == NULL || y == NULL ||
        width == NULL || height == NULL) {
        return -1;
    }
    if (layout == 1) {
        *x = 14;
        *y = 58 + index * 76;
        *width = 452;
        *height = 76;
    } else {
        *x = 24 + (index % 3) * 156 - 20;
        *y = 54 + (index / 3) * 222;
        *width = cover_w + 40;
        *height = cover_h + 58;
    }
    return 0;
}

static int present_bookshelf_selection_change(esp_display_t *display,
                                              const gfx_framebuffer_t *fb,
                                              int previous_selection,
                                              int current_selection,
                                              int layout) {
    int old_x;
    int old_y;
    int old_w;
    int old_h;
    int new_x;
    int new_y;
    int new_w;
    int new_h;
    if (bookshelf_card_bounds(previous_selection, layout, &old_x, &old_y, &old_w, &old_h) != 0 ||
        bookshelf_card_bounds(current_selection, layout, &new_x, &new_y, &new_w, &new_h) != 0) {
        return -1;
    }
    return present_two_windows(display, fb, old_x, old_y, old_w, old_h,
                               new_x, new_y, new_w, new_h);
}

static int present_file_browser_selection_change(esp_display_t *display,
                                                 const gfx_framebuffer_t *fb,
                                                 int previous_selection,
                                                 int current_selection) {
    const int visible_rows = 8;
    const int row_height = 78;
    int previous_start = previous_selection >= visible_rows ? previous_selection - visible_rows + 1 : 0;
    int current_start = current_selection >= visible_rows ? current_selection - visible_rows + 1 : 0;
    if (previous_start != current_start) {
        return esp_display_present_partial(display, fb, 16, 84, 448, 668);
    }
    return present_two_windows(display, fb, 16,
                               92 + (previous_selection - previous_start) * row_height - 8,
                               448, row_height + 10,
                               16,
                               92 + (current_selection - current_start) * row_height - 8,
                               448, row_height + 10);
}

static int present_reader_page_animation(esp_display_t *display,
                                         const gfx_framebuffer_t *fb,
                                         int page_turn_mode) {
    const int x = 20;
    const int y = 60;
    const int width = 440;
    const int height = 740;
    const int steps = 4;
    if (page_turn_mode == 2) {
        return esp_display_present_partial(display, fb, x, y, width, height);
    }
    for (int step = 0; step < steps; step++) {
        int result;
        if (page_turn_mode == 1) {
            int band_y = y + step * height / steps;
            int band_h = (step == steps - 1) ? y + height - band_y : height / steps;
            result = esp_display_present_partial(display, fb, x, band_y, width, band_h);
        } else {
            int band_x = x + (steps - step - 1) * width / steps;
            int band_right = x + (steps - step) * width / steps;
            int band_w = band_right - band_x;
            result = esp_display_present_partial(display, fb, band_x, y, band_w, height);
        }
        if (result != 0) return -1;
    }
    return 0;
}

static int present_reader_incremental_change(esp_display_t *display,
                                             const gfx_framebuffer_t *fb,
                                             app_page_t previous_page,
                                             const app_state_t *previous,
                                             const app_state_t *current,
                                             app_button_t button) {
    if (previous_page == APP_PAGE_READER && current->page == APP_PAGE_READER) {
        if (previous->reader_page != current->reader_page) {
            /* Reader page turns use the partial waveform in every mode.
             * The display layer still performs a periodic full refresh to
             * clear accumulated ghosting. */
            if (current->reader_refresh_mode == 0) {
                return esp_display_present_partial(display, fb, 20, 60, 440, 740);
            }
            if (current->reader_refresh_mode == 2) {
                return esp_display_present_partial(display, fb, 20, 60, 440, 740);
            }
            return present_reader_page_animation(display, fb,
                                                 current->reader_page_turn_mode);
        }
        if (previous->reader_menu_open != current->reader_menu_open) {
            /* Opening/closing the centered reader menu only changes this overlay. */
            return esp_display_present_partial(display, fb, 76, 324, 328, 152);
        }
        if (current->reader_menu_open &&
            previous->reader_menu_selection != current->reader_menu_selection) {
            return present_reader_menu_selection_change(display, fb,
                                                        previous->reader_menu_selection,
                                                        current->reader_menu_selection);
        }
    }

    if (current->page == APP_PAGE_READER && previous_page != APP_PAGE_READER) {
        return esp_display_present_partial(display, fb, 0, 32,
                                           GFX_WIDTH, GFX_HEIGHT - 32);
    }

    if (previous_page == APP_PAGE_SYSTEM_FONT && current->page == APP_PAGE_SYSTEM_FONT &&
        previous->system_font_selection != current->system_font_selection) {
        return present_system_font_selection_change(display, fb,
                                                    previous->system_font_selection,
                                                    current->system_font_selection);
    }

    if (previous_page == APP_PAGE_READER_FONT && current->page == APP_PAGE_READER_FONT &&
        previous->reader_font_selection != current->reader_font_selection) {
        return present_system_font_selection_change(display, fb,
                                                    previous->reader_font_selection,
                                                    current->reader_font_selection);
    }

    if ((previous_page == APP_PAGE_READER_SETTINGS && current->page == APP_PAGE_READER_FONT) ||
        (previous_page == APP_PAGE_READER_FONT && current->page == APP_PAGE_READER_SETTINGS)) {
        return esp_display_present_partial(display, fb, 0, 32,
                                           GFX_WIDTH, GFX_HEIGHT - 32);
    }

    if ((previous_page == APP_PAGE_SETTINGS && current->page == APP_PAGE_SYSTEM_FONT) ||
        (previous_page == APP_PAGE_SYSTEM_FONT && current->page == APP_PAGE_SETTINGS)) {
        return esp_display_present_partial(display, fb, 0, 32,
                                           GFX_WIDTH, GFX_HEIGHT - 32);
    }

    if (previous_page == APP_PAGE_READER_CATALOG && current->page == APP_PAGE_READER_CATALOG &&
        previous->reader_catalog_selection != current->reader_catalog_selection) {
        const int row_x = 10;
        const int row_width = 460;
        const int row_height = 52;
        int old_start = previous->reader_catalog_selection >= 9
                            ? previous->reader_catalog_selection - 8 : 0;
        int new_start = current->reader_catalog_selection >= 9
                            ? current->reader_catalog_selection - 8 : 0;
        int old_y;
        int new_y;
        if (old_start != new_start) {
            return esp_display_present_partial(display, fb, 10, 18, 460, 510);
        }
        old_y = 22 + (previous->reader_catalog_selection - old_start) * 52;
        new_y = 22 + (current->reader_catalog_selection - new_start) * 52;
        if (esp_display_present_partial(display, fb, row_x, old_y, row_width, row_height) != 0 ||
            esp_display_present_partial(display, fb, row_x, new_y, row_width, row_height) != 0) {
            return -1;
        }
        return 0;
    }

    if (previous_page == APP_PAGE_READER_SETTINGS && current->page == APP_PAGE_READER_SETTINGS &&
        (previous->reader_settings_selection != current->reader_settings_selection ||
         previous->reader_settings_editing != current->reader_settings_editing ||
         previous->reader_pending_font_size_index != current->reader_pending_font_size_index ||
         previous->reader_pending_font_index != current->reader_pending_font_index ||
         previous->reader_pending_line_spacing_index != current->reader_pending_line_spacing_index ||
         previous->reader_pending_margin_index != current->reader_pending_margin_index ||
         previous->reader_pending_indent_enabled != current->reader_pending_indent_enabled ||
         previous->reader_pending_bold_enabled != current->reader_pending_bold_enabled ||
         previous->reader_pending_page_turn_mode != current->reader_pending_page_turn_mode ||
         previous->reader_pending_refresh_mode != current->reader_pending_refresh_mode ||
         previous->font_size_index != current->font_size_index ||
         previous->reader_font_index != current->reader_font_index ||
         previous->line_spacing_index != current->line_spacing_index ||
         previous->reader_margin_index != current->reader_margin_index ||
         previous->reader_indent_enabled != current->reader_indent_enabled ||
         previous->reader_bold_enabled != current->reader_bold_enabled ||
         previous->reader_page_turn_mode != current->reader_page_turn_mode ||
         previous->reader_refresh_mode != current->reader_refresh_mode)) {
        return esp_display_present_partial(display, fb, 20, 84, 440, 640);
    }

    if (previous_page == APP_PAGE_BOOKSHELF && current->page == APP_PAGE_BOOKSHELF &&
        previous->bookshelf_selection != current->bookshelf_selection) {
        return present_bookshelf_selection_change(display, fb,
                                                  previous->bookshelf_selection,
                                                  current->bookshelf_selection,
                                                  current->bookshelf_layout);
    }

    if (previous_page == APP_PAGE_FILE_BROWSER && current->page == APP_PAGE_FILE_BROWSER &&
        previous->file_browser_selection != current->file_browser_selection &&
        (button == APP_BUTTON_UP || button == APP_BUTTON_DOWN)) {
        return present_file_browser_selection_change(display, fb,
                                                     previous->file_browser_selection,
                                                     current->file_browser_selection);
    }

    if (previous_page == APP_PAGE_WEATHER && current->page == APP_PAGE_WEATHER &&
        (previous->weather_city_index != current->weather_city_index ||
         previous->weather_scroll != current->weather_scroll ||
         previous->weather_stale != current->weather_stale ||
         previous->weather_refreshes != current->weather_refreshes ||
         previous->weather_last_updated_minutes != current->weather_last_updated_minutes)) {
        return esp_display_present_partial(display, fb, 0, 40, GFX_WIDTH, GFX_HEIGHT - 40);
    }

    if (previous_page == APP_PAGE_CALENDAR && current->page == APP_PAGE_CALENDAR &&
        (previous->calendar_month_offset != current->calendar_month_offset ||
         previous->calendar_selected_day != current->calendar_selected_day ||
         previous->calendar_detail_open != current->calendar_detail_open)) {
        return esp_display_present_partial(display, fb, 8, 40, 464, 720);
    }

    if (previous_page == APP_PAGE_ENGLISH && current->page == APP_PAGE_ENGLISH &&
        (previous->english_word != current->english_word ||
         previous->english_show_back != current->english_show_back ||
         previous->english_known_count != current->english_known_count ||
         previous->english_review_count != current->english_review_count ||
         memcmp(previous->english_answer_state, current->english_answer_state,
                sizeof(previous->english_answer_state)) != 0)) {
        return esp_display_present_partial(display, fb, 20, 48, 440, 660);
    }

    if (previous_page == APP_PAGE_SETTINGS && current->page == APP_PAGE_SETTINGS &&
        (previous->settings_selection != current->settings_selection ||
         previous->settings_scroll != current->settings_scroll ||
         previous->bluetooth_enabled != current->bluetooth_enabled ||
         previous->dictionary_enabled != current->dictionary_enabled ||
         previous->time_sync_requested != current->time_sync_requested ||
         previous->update_check_requested != current->update_check_requested ||
         previous->weather_city_index != current->weather_city_index ||
         previous->power_saving_enabled != current->power_saving_enabled ||
         previous->bookshelf_layout != current->bookshelf_layout)) {
        return esp_display_present_partial(display, fb, 0, 40, GFX_WIDTH, GFX_HEIGHT - 40);
    }

    if (previous_page == APP_PAGE_WIFI_SETUP && current->page == APP_PAGE_WIFI_SETUP &&
        (previous->wifi_setup_selection != current->wifi_setup_selection ||
         previous->wifi_editor_active != current->wifi_editor_active ||
         previous->wifi_edit_char_index != current->wifi_edit_char_index ||
         previous->wifi_setup_stage != current->wifi_setup_stage ||
         previous->wifi_saved_count != current->wifi_saved_count ||
         previous->wifi_network_count != current->wifi_network_count ||
         previous->wifi_network_selection != current->wifi_network_selection ||
         previous->wifi_scan_in_progress != current->wifi_scan_in_progress ||
         strcmp(previous->wifi_ip, current->wifi_ip) != 0 ||
         memcmp(previous->wifi_saved_ssids, current->wifi_saved_ssids,
                sizeof(current->wifi_saved_ssids)) != 0 ||
         strcmp(previous->wifi_ssid, current->wifi_ssid) != 0 ||
         strcmp(previous->wifi_password, current->wifi_password) != 0)) {
        return esp_display_present_partial(display, fb, 0, 40, GFX_WIDTH, GFX_HEIGHT - 40);
    }

    if ((previous_page == APP_PAGE_SETTINGS && current->page == APP_PAGE_WIFI_SETUP) ||
        (previous_page == APP_PAGE_WIFI_SETUP && current->page == APP_PAGE_SETTINGS)) {
        return esp_display_present_partial(display, fb, 0, 40, GFX_WIDTH, GFX_HEIGHT - 40);
    }
    return -1;
}

static long long current_epoch_minute(void) {
    time_t now = time(NULL);
    return now >= (time_t)1700000000 ? (long long)(now / 60) : -1;
}

typedef struct {
    gfx_framebuffer_t *fb;
    esp_display_t *display;
} reader_pagination_progress_t;

static void present_reader_pagination_progress(int book_index, int percent, void *context) {
    reader_pagination_progress_t *progress = context;
    char percent_text[16];
    int fill_width;
    (void)book_index;
    if (progress == NULL || progress->fb == NULL || progress->display == NULL) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (percent == 0) {
        gfx_clear(progress->fb, GFX_WHITE);
    } else {
        gfx_fill_rect(progress->fb, 0, 250, GFX_WIDTH, 300, GFX_WHITE);
    }
    font_draw_text_aligned_builtin(28, progress->fb, 0, 292, GFX_WIDTH,
                                   "正在重新分页", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned_builtin(18, progress->fb, 0, 344, GFX_WIDTH,
                                   "正在处理当前书籍，请稍候", FONT_ALIGN_CENTER, GFX_BLACK);
    gfx_draw_rounded_rect_thick(progress->fb, 40, 400, 400, 30, 6, 2, GFX_BLACK);
    fill_width = percent * 392 / 100;
    if (fill_width > 0) {
        gfx_fill_rounded_rect(progress->fb, 44, 404, fill_width, 22, 4, GFX_BLACK);
    }
    snprintf(percent_text, sizeof(percent_text), "%d%%", percent);
    font_draw_text_aligned_builtin(20, progress->fb, 0, 454, GFX_WIDTH,
                                   percent_text, FONT_ALIGN_CENTER, GFX_BLACK);
    (void)esp_display_present_partial(progress->display, progress->fb,
                                      0,
                                      percent == 0 ? 32 : 250,
                                      GFX_WIDTH,
                                      percent == 0 ? GFX_HEIGHT - 32 : 300);
}

static void refresh_wifi_networks(app_state_t *app) {
    esp_wifi_scan_result_t results[ESP_WIFI_SCAN_MAX];
    int count;
    int saved_count;
    int displayed_saved = 0;
    int displayed_scanned = 0;
    if (app == NULL) return;
    app->wifi_ip[0] = '\0';
    if (app->wifi_connected) {
        (void)esp_time_sync_get_ip(app->wifi_ip, sizeof(app->wifi_ip));
    }
    memset(app->wifi_saved_ssids, 0, sizeof(app->wifi_saved_ssids));
    saved_count = esp_time_sync_load_saved_networks(app->wifi_saved_ssids,
                                                     APP_WIFI_SAVED_MAX);
    if (saved_count < 0) saved_count = 0;
    for (int i = 0; i < saved_count; i++) {
        if (app->wifi_connected && strcmp(app->wifi_saved_ssids[i], app->wifi_ssid) == 0) {
            continue;
        }
        if (displayed_saved != i) {
            memmove(app->wifi_saved_ssids[displayed_saved], app->wifi_saved_ssids[i],
                    sizeof(app->wifi_saved_ssids[displayed_saved]));
        }
        displayed_saved++;
    }
    app->wifi_saved_count = displayed_saved;
    for (int i = displayed_saved; i < APP_WIFI_SAVED_MAX; i++) {
        app->wifi_saved_ssids[i][0] = '\0';
    }
    memset(results, 0, sizeof(results));
    count = esp_time_sync_scan_networks(results, ESP_WIFI_SCAN_MAX);
    for (int i = 0; i < (count > 0 ? count : 0); i++) {
        int duplicate = app->wifi_connected && strcmp(results[i].ssid, app->wifi_ssid) == 0;
        for (int j = 0; !duplicate && j < app->wifi_saved_count; j++) {
            duplicate = strcmp(results[i].ssid, app->wifi_saved_ssids[j]) == 0;
        }
        if (duplicate || results[i].ssid[0] == '\0') continue;
        snprintf(app->wifi_network_ssids[displayed_scanned],
                 sizeof(app->wifi_network_ssids[displayed_scanned]), "%s", results[i].ssid);
        app->wifi_network_rssi[displayed_scanned] = results[i].rssi;
        app->wifi_network_secure[displayed_scanned] = results[i].secure;
        displayed_scanned++;
    }
    app->wifi_network_count = displayed_scanned;
    for (int i = app->wifi_network_count; i < APP_WIFI_SCAN_MAX; i++) {
        app->wifi_network_ssids[i][0] = '\0';
        app->wifi_network_rssi[i] = 0;
        app->wifi_network_secure[i] = 0;
    }
    if (app->wifi_network_selection >= app->wifi_saved_count + app->wifi_network_count) {
        app->wifi_network_selection = 0;
    }
    app->wifi_scan_in_progress = 0;
    app->wifi_scan_requested = 0;
    if (count < 0) ESP_LOGW(TAG, "Wi-Fi scan failed");
}

void app_main(void) {
    app_state_t app;
    gfx_framebuffer_t *fb;
    esp_display_t display;
    esp_input_t input;
    esp_sd_t sd;
    font_t font;
    long long last_clock_minute;
    long long last_weather_request_minute = -1;
    long long last_battery_sample_minute = -1;
    unsigned int weather_generation = 0;
    int sd_mounted = 0;
    int provisioning_mode;

    ESP_LOGI(TAG, "booting ESP32 E-Ink reader firmware");

    fb = heap_caps_malloc(sizeof(*fb), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (fb == NULL) {
        ESP_LOGE(TAG, "failed to allocate %u-byte framebuffer in PSRAM",
                 (unsigned int)sizeof(*fb));
        return;
    }
    ESP_LOGI(TAG, "allocated %u-byte framebuffer in PSRAM",
             (unsigned int)sizeof(*fb));

    init_nvs_storage();
    sd_mounted = esp_sd_init(&sd) == 0;
    if (sd_mounted) {
        int font_count = font_manager_load_dir(ESP_SD_FONT_DIRECTORY);
        ESP_LOGI(TAG, "cataloged %d external font file(s) from %s",
                 font_count > 0 ? font_count : 0, ESP_SD_FONT_DIRECTORY);
        if (esp_config_backup_restore_platform() == 0) {
            ESP_LOGI(TAG, "restored missing platform configuration from SD");
        }
    }
    provisioning_mode = !esp_time_sync_has_credentials();
    esp_time_sync_start();
    esp_time_sync_wait_for_time(ESP_NTP_BOOT_TIMEOUT_MS);
    if (provisioning_mode &&
        esp_time_sync_start_provisioning_ap("AI-Reader-Setup") != 0) {
        ESP_LOGE(TAG, "failed to start Wi-Fi provisioning access point");
    }
    app_init(&app);
    if (app_persistence_load_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, &app) == 0) {
        ESP_LOGI(TAG, "restored app state from NVS");
    } else if (sd_mounted &&
               app_persistence_load_app_file(ESP_SD_APP_CONFIG_PATH, &app) == 0) {
        ESP_LOGI(TAG, "NVS app state missing; restored settings from SD");
    }
    if (sd_mounted && esp_config_backup_save_platform() != 0) {
        ESP_LOGW(TAG, "failed to refresh platform configuration backup on SD");
    }
    app_sync_reader_library(&app);
    app.wifi_connected = esp_time_sync_wifi_connected();
    app.time_synchronized = esp_time_sync_is_ready();
    {
        int battery_mv;
        app.battery_valid = esp_battery_read_percent(&app.battery_percent,
                                                     &battery_mv) == 0;
    }
    if (esp_web_admin_start(sd_mounted, provisioning_mode) != 0) {
        ESP_LOGE(TAG, "failed to start web administration service");
    }
    gfx_init(fb);
    esp_display_init(&display);
    esp_input_init(&input);

    if (!font_load_default(&font)) {
        ESP_LOGE(TAG, "failed to load built-in bitmap font");
        heap_caps_free(fb);
        return;
    }

    ui_render_page(fb, &app, &font);
    if (esp_display_present(&display, fb) != 0) {
        ESP_LOGE(TAG, "failed to present first frame");
    }
    last_clock_minute = current_epoch_minute();
    if (app.wifi_connected && esp_weather_request_update() == 0) {
        last_weather_request_minute = last_clock_minute >= 0 ? last_clock_minute : 0;
    }

    if (sd_mounted) {
        sd_library_state = SD_LIBRARY_LOADING;
        if (xTaskCreatePinnedToCore(sd_library_load_task, "book_index",
                                    SD_LIBRARY_TASK_STACK_SIZE, NULL,
                                    tskIDLE_PRIORITY + 1, NULL, 0) == pdPASS) {
            app.reader_library_loading = 1;
            app.reader_library_progress = 0;
            ESP_LOGI(TAG, "desktop ready; indexing TXT books in background");
        } else {
            sd_library_state = SD_LIBRARY_IDLE;
            app.reader_library_loading = 0;
            ESP_LOGE(TAG, "failed to start background book indexing task");
        }
    }

    while (1) {
        app_button_t button;
        if (sd_library_state == SD_LIBRARY_COMPLETE) {
            sd_library_state = SD_LIBRARY_READY;
            app.reader_library_loading = 0;
            app.reader_library_progress = 100;
            if (app_persistence_load_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, &app) == 0) {
                ESP_LOGI(TAG, "restored reading progress after background indexing");
            } else if (sd_mounted &&
                       app_persistence_load_app_file(ESP_SD_APP_CONFIG_PATH, &app) == 0) {
                ESP_LOGI(TAG, "restored reading progress from SD after background indexing");
            }
            app_sync_reader_library(&app);
            if (save_app_configuration(&app, sd_mounted) != 0) {
                ESP_LOGW(TAG, "failed to persist restored library state");
            }
            ESP_LOGI(TAG, "background indexing complete: loaded %d TXT book(s) from %s",
                     sd_library_loaded_count, ESP_SD_BOOK_DIRECTORY);
            if (app.page == APP_PAGE_BOOKSHELF) {
                ui_render_page(fb, &app, &font);
                if (esp_display_present_partial(&display, fb, 0, 32,
                                                GFX_WIDTH, GFX_HEIGHT - 32) != 0) {
                    ESP_LOGW(TAG, "failed to refresh bookshelf after indexing");
                }
            }
        }
        if (sd_library_state == SD_LIBRARY_LOADING &&
            app.reader_library_progress != sd_library_progress) {
            app.reader_library_progress = sd_library_progress;
            if (app.page == APP_PAGE_BOOKSHELF) {
                ui_render_page(fb, &app, &font);
                if (esp_display_present_partial(&display, fb, 0, 32,
                                                GFX_WIDTH, GFX_HEIGHT - 32) != 0) {
                    ESP_LOGW(TAG, "failed to update bookshelf loading progress");
                }
            }
        }
        if (reader_pagination_state == READER_PAGINATION_COMPLETE) {
            int completed_book = -1;
            int commit_result = reader_pagination_result > 0
                                    ? reader_library_commit_background_layout(&completed_book)
                                    : -1;
            reader_pagination_state = READER_PAGINATION_IDLE;
            reader_pagination_book_index = -1;
            app.reader_background_pagination_active = 0;
            app.reader_background_pagination_progress = 100;
            if (commit_result > 0) {
                app_finish_background_pagination(&app, completed_book);
                if (save_app_configuration(&app, sd_mounted) != 0) {
                    ESP_LOGW(TAG, "failed to save state after background pagination");
                }
                ESP_LOGI(TAG, "background pagination complete for book %d", completed_book);
                if (app.page == APP_PAGE_READER && app.current_book == completed_book) {
                    ui_render_page(fb, &app, &font);
                    if (esp_display_present_partial(&display, fb, 0, 32,
                                                    GFX_WIDTH, GFX_HEIGHT - 32) != 0) {
                        ESP_LOGW(TAG, "failed to refresh reader after background pagination");
                    }
                }
            } else if (commit_result < 0) {
                ESP_LOGW(TAG, "background pagination result was discarded");
            }
        }
        if (app.page == APP_PAGE_READER) {
            start_reader_background_pagination(app.current_book);
        }
        if (reader_pagination_state == READER_PAGINATION_RUNNING &&
            app.page == APP_PAGE_READER &&
            app.current_book == reader_pagination_book_index) {
            int progress = reader_library_background_progress();
            if (!app.reader_background_pagination_active ||
                app.reader_background_pagination_progress != progress) {
                app.reader_background_pagination_active = 1;
                app.reader_background_pagination_progress = progress;
                ui_render_page(fb, &app, &font);
                if (esp_display_present_partial(&display, fb, 0, 770,
                                                GFX_WIDTH, 30) != 0) {
                    ESP_LOGW(TAG, "failed to update background pagination progress");
                }
            }
        } else if (app.reader_background_pagination_active &&
                   (app.page != APP_PAGE_READER ||
                    app.current_book != reader_pagination_book_index)) {
            app.reader_background_pagination_active = 0;
        }
        if (esp_input_poll_button(&input, &button)) {
            app_state_t previous_app = app;
            app_page_t previous_page = previous_app.page;
            int previous_home_selection = previous_app.home_selection;
            int partial_home_selection;
            reader_pagination_progress_t button_pagination_progress = {fb, &display};
            ESP_LOGI(TAG, "button event %d on page %s", button, app_page_name(app.page));
            if (button == APP_BUTTON_POWER_LONG) {
                if (sd_library_state != SD_LIBRARY_LOADING &&
                    save_app_configuration(&app, sd_mounted) != 0) {
                    ESP_LOGW(TAG, "failed to save app state before display sleep");
                }
                esp_display_sleep(&display);
                continue;
            }
            if (sd_library_state == SD_LIBRARY_LOADING && app.page == APP_PAGE_HOME &&
                button == APP_BUTTON_HOME &&
                app.home_selection == 1) {
                ESP_LOGI(TAG, "book indexing is still running; file browser will be available shortly");
                continue;
            }
            if (sd_library_state != SD_LIBRARY_LOADING) {
                reader_library_set_progress_callback(present_reader_pagination_progress,
                                                      &button_pagination_progress);
            }
            app_handle_button(&app, button);
            if (sd_library_state != SD_LIBRARY_LOADING) {
                reader_library_set_progress_callback(NULL, NULL);
            }
            if (previous_page != APP_PAGE_WIFI_SETUP && app.page == APP_PAGE_WIFI_SETUP) {
                esp_time_sync_load_credentials(app.wifi_ssid, sizeof(app.wifi_ssid),
                                                app.wifi_password, sizeof(app.wifi_password));
                app.wifi_scan_requested = 1;
            }
            if (app.page == APP_PAGE_WIFI_SETUP && app.wifi_scan_requested) {
                app.wifi_scan_in_progress = 1;
                ui_render_page(fb, &app, &font);
                if (esp_display_present_partial(&display, fb, 0, 40,
                                                GFX_WIDTH, GFX_HEIGHT - 40) != 0) {
                    ESP_LOGW(TAG, "failed to show Wi-Fi scan progress");
                }
                refresh_wifi_networks(&app);
            }
            if (app.wifi_config_save_requested) {
                app.wifi_config_save_requested = 0;
                if (esp_time_sync_save_credentials(app.wifi_ssid, app.wifi_password) == 0) {
                    if (sd_mounted && esp_config_backup_save_platform() != 0) {
                        ESP_LOGW(TAG, "failed to back up Wi-Fi configuration to SD");
                    }
                    ESP_LOGI(TAG, "Wi-Fi configuration saved; rebooting for network time sync");
                    vTaskDelay(pdMS_TO_TICKS(250));
                    esp_restart();
                }
                ESP_LOGE(TAG, "failed to save Wi-Fi configuration");
            }
            if (app.time_sync_requested) {
                ESP_LOGI(TAG, "time synchronization requested from Settings");
                esp_time_sync_start();
                app.wifi_connected = esp_time_sync_wifi_connected();
                app.time_synchronized = esp_time_sync_is_ready();
            }
            if (app.weather_refreshes != previous_app.weather_refreshes &&
                app.wifi_connected && esp_weather_request_update() == 0) {
                long long request_minute = current_epoch_minute();
                last_weather_request_minute = request_minute >= 0 ? request_minute : 0;
            }
            partial_home_selection = previous_page == APP_PAGE_HOME &&
                                     app.page == APP_PAGE_HOME &&
                                     previous_home_selection != app.home_selection &&
                                     (button == APP_BUTTON_UP || button == APP_BUTTON_DOWN);
            if (sd_library_state != SD_LIBRARY_LOADING) {
                if (save_app_configuration(&app, sd_mounted) != 0) {
                    ESP_LOGW(TAG, "failed to save app state to NVS");
                }
            } else {
                ESP_LOGD(TAG, "deferred state save until book library is ready");
            }
            ui_render_page(fb, &app, &font);
            if (!(partial_home_selection &&
                  present_home_selection_change(&display, fb, previous_home_selection,
                                                app.home_selection) == 0) &&
                present_reader_incremental_change(&display, fb, previous_page,
                                                  &previous_app, &app, button) != 0 &&
                esp_display_present(&display, fb) != 0) {
                ESP_LOGE(TAG, "failed to present button frame");
            }
            {
                reader_pagination_progress_t pagination_progress = {fb, &display};
                int layout_applied;
                reader_library_set_progress_callback(present_reader_pagination_progress,
                                                      &pagination_progress);
                layout_applied = app_apply_pending_reader_layout(&app);
                reader_library_set_progress_callback(NULL, NULL);
                if (layout_applied) {
                ESP_LOGI(TAG, "reader layout re-pagination complete");
                if (save_app_configuration(&app, sd_mounted) != 0) {
                    ESP_LOGW(TAG, "failed to save reading position after re-pagination");
                }
                ui_render_page(fb, &app, &font);
                if (esp_display_present_partial(&display, fb, 0, 32,
                                                GFX_WIDTH, GFX_HEIGHT - 32) != 0) {
                    ESP_LOGE(TAG, "failed to present re-paginated reader frame");
                }
                }
            }
            if (app.page == APP_PAGE_READER) {
                start_reader_background_pagination(app.current_book);
            }
        }
        {
            int connected = esp_time_sync_wifi_connected();
            int time_ready = esp_time_sync_is_ready();
            long long minute = current_epoch_minute();
            int status_changed = connected != app.wifi_connected ||
                                 time_ready != app.time_synchronized;
            app.wifi_connected = connected;
            app.time_synchronized = time_ready;
            if (minute != last_clock_minute) {
                last_clock_minute = minute;
                status_changed = 1;
            }
            if (connected &&
                (last_weather_request_minute < 0 ||
                 (minute >= 0 && minute - last_weather_request_minute >= 30)) &&
                esp_weather_is_configured() &&
                esp_weather_request_update() == 0) {
                last_weather_request_minute = minute >= 0 ? minute : 0;
            }
            {
                esp_weather_result_t weather;
                if (esp_weather_get_result(&weather) == 0 &&
                    weather.generation != weather_generation) {
                    weather_generation = weather.generation;
                    app.weather_valid = weather.valid;
                    app.weather_temperature = weather.temperature;
                    app.weather_humidity = weather.humidity;
                    app.weather_type = weather.type;
                    snprintf(app.weather_text, sizeof(app.weather_text), "%s", weather.text);
                    snprintf(app.weather_error, sizeof(app.weather_error), "%s", weather.error);
                    snprintf(app.weather_wind, sizeof(app.weather_wind), "%s", weather.wind);
                    app.weather_stale = 0;
                    app.weather_last_updated_minutes = 0;
                    ui_render_page(fb, &app, &font);
                    if (app.page == APP_PAGE_WEATHER) {
                        esp_display_present_partial(&display, fb, 0, 32,
                                                    GFX_WIDTH, GFX_HEIGHT - 32);
                    } else if (app.page == APP_PAGE_HOME) {
                        esp_display_present_partial(&display, fb, 0, 0, GFX_WIDTH, 180);
                    } else {
                        esp_display_present_partial(&display, fb, 0, 0, GFX_WIDTH, 32);
                    }
                    status_changed = 0;
                }
            }
            if (minute >= 0 && (last_battery_sample_minute < 0 ||
                                minute - last_battery_sample_minute >= 5)) {
                int battery_mv;
                int battery_percent;
                int battery_valid = esp_battery_read_percent(&battery_percent,
                                                             &battery_mv) == 0;
                if (battery_valid != app.battery_valid ||
                    (battery_valid && battery_percent != app.battery_percent)) {
                    app.battery_valid = battery_valid;
                    app.battery_percent = battery_percent;
                    status_changed = 1;
                }
                last_battery_sample_minute = minute;
            }
            if (status_changed) {
            ui_render_page(fb, &app, &font);
                int height = app.page == APP_PAGE_HOME ? 180 : 32;
                if (esp_display_present_partial(&display, fb, 0, 0,
                                                GFX_WIDTH, height) != 0) {
                ESP_LOGW(TAG, "failed to update clock status strip");
            }
        }
        }
        vTaskDelay(pdMS_TO_TICKS(ESP_BUTTON_POLL_MS));
    }
}
