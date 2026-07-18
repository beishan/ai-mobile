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
#include "platform/esp_sd.h"
#include "platform/esp_time_sync.h"
#include "ui/pages.h"

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
#define SD_LIBRARY_TASK_STACK_SIZE 8192
#define ESP_SD_FONT_DIRECTORY ESP_SD_MOUNT_POINT "/fonts"

enum {
    SD_LIBRARY_IDLE = 0,
    SD_LIBRARY_LOADING,
    SD_LIBRARY_COMPLETE,
    SD_LIBRARY_READY
};

static volatile int sd_library_state = SD_LIBRARY_IDLE;
static volatile int sd_library_loaded_count;

static void sd_library_load_task(void *arg) {
    (void)arg;
    sd_library_loaded_count = reader_library_load_directory(ESP_SD_BOOK_DIRECTORY);
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
    const int menu_y = 288;
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

static int bookshelf_card_bounds(int index, int *x, int *y, int *width, int *height) {
    const int cover_w = 124;
    const int cover_h = 170;
    if (index < 0 || index >= APP_BOOK_COUNT || x == NULL || y == NULL ||
        width == NULL || height == NULL) {
        return -1;
    }
    *x = 24 + index * 156 - 20;
    *y = 54;
    *width = cover_w + 40;
    *height = cover_h + 58;
    return 0;
}

static int present_bookshelf_selection_change(esp_display_t *display,
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
    if (bookshelf_card_bounds(previous_selection, &old_x, &old_y, &old_w, &old_h) != 0 ||
        bookshelf_card_bounds(current_selection, &new_x, &new_y, &new_w, &new_h) != 0) {
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
            /* Normal mode favors clean full refreshes. Fast mode reveals the
             * new page in bands; extreme mode uses one partial transaction. */
            if (current->reader_refresh_mode == 0) {
                return -1;
            }
            if (current->reader_refresh_mode == 2) {
                return esp_display_present_partial(display, fb, 20, 60, 440, 740);
            }
            return present_reader_page_animation(display, fb,
                                                 current->reader_page_turn_mode);
        }
        if (previous->reader_menu_open != current->reader_menu_open) {
            /* Opening/closing the centered reader menu only changes this overlay. */
            return esp_display_present_partial(display, fb, 76, 284, 328, 232);
        }
        if (current->reader_menu_open &&
            previous->reader_menu_selection != current->reader_menu_selection) {
            return present_reader_menu_selection_change(display, fb,
                                                        previous->reader_menu_selection,
                                                        current->reader_menu_selection);
        }
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
                                                  current->bookshelf_selection);
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
         previous->power_saving_enabled != current->power_saving_enabled)) {
        return esp_display_present_partial(display, fb, 0, 40, GFX_WIDTH, GFX_HEIGHT - 40);
    }

    if (previous_page == APP_PAGE_WIFI_SETUP && current->page == APP_PAGE_WIFI_SETUP &&
        (previous->wifi_setup_selection != current->wifi_setup_selection ||
         previous->wifi_editor_active != current->wifi_editor_active ||
         previous->wifi_edit_char_index != current->wifi_edit_char_index ||
         strcmp(previous->wifi_ssid, current->wifi_ssid) != 0 ||
         strcmp(previous->wifi_password, current->wifi_password) != 0)) {
        return esp_display_present_partial(display, fb, 16, 156, 448, 492);
    }
    return -1;
}

static long long current_epoch_minute(void) {
    time_t now = time(NULL);
    return now >= (time_t)1700000000 ? (long long)(now / 60) : -1;
}

void app_main(void) {
    app_state_t app;
    gfx_framebuffer_t *fb;
    esp_display_t display;
    esp_input_t input;
    esp_sd_t sd;
    font_t font;
    long long last_clock_minute;
    int sd_mounted = 0;

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
    esp_time_sync_start();
    esp_time_sync_wait_for_time(ESP_NTP_BOOT_TIMEOUT_MS);
    sd_mounted = esp_sd_init(&sd) == 0;
    if (sd_mounted) {
        int font_count = font_manager_load_dir(ESP_SD_FONT_DIRECTORY);
        ESP_LOGI(TAG, "cataloged %d external font file(s) from %s",
                 font_count > 0 ? font_count : 0, ESP_SD_FONT_DIRECTORY);
    }
    app_init(&app);
    if (app_persistence_load_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, &app) == 0) {
        ESP_LOGI(TAG, "restored app state from NVS");
    }
    app_sync_reader_library(&app);
    app.wifi_connected = esp_time_sync_is_ready();
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

    if (sd_mounted) {
        sd_library_state = SD_LIBRARY_LOADING;
        if (xTaskCreatePinnedToCore(sd_library_load_task, "book_index",
                                    SD_LIBRARY_TASK_STACK_SIZE, NULL,
                                    tskIDLE_PRIORITY + 1, NULL, 0) == pdPASS) {
            ESP_LOGI(TAG, "desktop ready; indexing TXT books in background");
        } else {
            sd_library_state = SD_LIBRARY_IDLE;
            ESP_LOGE(TAG, "failed to start background book indexing task");
        }
    }

    while (1) {
        app_button_t button;
        if (sd_library_state == SD_LIBRARY_COMPLETE) {
            sd_library_state = SD_LIBRARY_READY;
            if (app_persistence_load_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, &app) == 0) {
                ESP_LOGI(TAG, "restored reading progress after background indexing");
            }
            app_sync_reader_library(&app);
            ESP_LOGI(TAG, "background indexing complete: loaded %d TXT book(s) from %s",
                     sd_library_loaded_count, ESP_SD_BOOK_DIRECTORY);
        }
        if (esp_input_poll_button(&input, &button)) {
            app_state_t previous_app = app;
            app_page_t previous_page = previous_app.page;
            int previous_home_selection = previous_app.home_selection;
            int partial_home_selection;
            ESP_LOGI(TAG, "button event %d on page %s", button, app_page_name(app.page));
            if (button == APP_BUTTON_POWER_LONG) {
                if (app_persistence_save_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, &app) != 0) {
                    ESP_LOGW(TAG, "failed to save app state before display sleep");
                }
                esp_display_sleep(&display);
                continue;
            }
            if (sd_library_state == SD_LIBRARY_LOADING && app.page == APP_PAGE_HOME &&
                button == APP_BUTTON_HOME &&
                (app.home_selection == 0 || app.home_selection == 1)) {
                ESP_LOGI(TAG, "book indexing is still running; bookshelf/files will be available shortly");
                continue;
            }
            app_handle_button(&app, button);
            if (previous_page != APP_PAGE_WIFI_SETUP && app.page == APP_PAGE_WIFI_SETUP) {
                esp_time_sync_load_credentials(app.wifi_ssid, sizeof(app.wifi_ssid),
                                                app.wifi_password, sizeof(app.wifi_password));
            }
            if (app.wifi_config_save_requested) {
                app.wifi_config_save_requested = 0;
                if (esp_time_sync_save_credentials(app.wifi_ssid, app.wifi_password) == 0) {
                    ESP_LOGI(TAG, "Wi-Fi configuration saved; rebooting for network time sync");
                    vTaskDelay(pdMS_TO_TICKS(250));
                    esp_restart();
                }
                ESP_LOGE(TAG, "failed to save Wi-Fi configuration");
            }
            if (app.time_sync_requested) {
                ESP_LOGI(TAG, "time synchronization requested from Settings");
                esp_time_sync_start();
                app.wifi_connected = esp_time_sync_is_ready();
            }
            partial_home_selection = previous_page == APP_PAGE_HOME &&
                                     app.page == APP_PAGE_HOME &&
                                     previous_home_selection != app.home_selection &&
                                     (button == APP_BUTTON_UP || button == APP_BUTTON_DOWN);
            if (app_persistence_save_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, &app) != 0) {
                ESP_LOGW(TAG, "failed to save app state to NVS");
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
        }
        if (esp_time_sync_is_ready() && current_epoch_minute() != last_clock_minute) {
            app.wifi_connected = 1;
            last_clock_minute = current_epoch_minute();
            ui_render_page(fb, &app, &font);
            if (esp_display_present_partial(&display, fb, 0, 0, GFX_WIDTH, 32) != 0) {
                ESP_LOGW(TAG, "failed to update clock status strip");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(ESP_BUTTON_POLL_MS));
    }
}
