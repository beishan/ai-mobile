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
#include "platform/storage_io.h"
#include "ui/pages.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
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
#define APP_SAVE_DEBOUNCE_MS 1000
#define APP_SAVE_TASK_STACK_SIZE 6144
#define APP_SD_BACKUP_SAVE_INTERVAL 10
#define WIFI_SCAN_TASK_STACK_SIZE 6144
#define PAGE_PREFETCH_TASK_STACK_SIZE 4096

enum {
    SD_LIBRARY_IDLE = 0,
    SD_LIBRARY_LOADING,
    SD_LIBRARY_COMPLETE,
    SD_LIBRARY_READY
};

enum {
    WIFI_SCAN_IDLE = 0,
    WIFI_SCAN_RUNNING,
    WIFI_SCAN_COMPLETE,
    WIFI_SCAN_FAILED
};

static volatile int sd_library_state = SD_LIBRARY_IDLE;
static volatile int sd_library_loaded_count;
static volatile int sd_library_progress;
static volatile int sd_library_total_count;
static app_state_t deferred_save_app;
static int deferred_save_sd_mounted;
static int deferred_save_force_sd_backup;
static TaskHandle_t deferred_save_task_handle;
static portMUX_TYPE deferred_save_lock = portMUX_INITIALIZER_UNLOCKED;
static app_state_t wifi_scan_request_app;
static app_state_t wifi_scan_result_app;
static volatile int wifi_scan_task_state;
static TaskHandle_t page_prefetch_task_handle;
static int page_prefetch_book;
static int page_prefetch_page;
static portMUX_TYPE page_prefetch_lock = portMUX_INITIALIZER_UNLOCKED;

static int save_app_configuration_nvs(const app_state_t *app) {
    return app_persistence_save_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, app);
}

static int backup_app_configuration_sd(const app_state_t *app) {
    int result = -1;
    storage_io_lock(STORAGE_IO_BACKGROUND);
    if (app_persistence_save_app_file(ESP_SD_APP_CONFIG_TEMP, app) == 0) {
        remove(ESP_SD_APP_CONFIG_BACKUP);
        rename(ESP_SD_APP_CONFIG_PATH, ESP_SD_APP_CONFIG_BACKUP);
        if (rename(ESP_SD_APP_CONFIG_TEMP, ESP_SD_APP_CONFIG_PATH) != 0) {
            rename(ESP_SD_APP_CONFIG_BACKUP, ESP_SD_APP_CONFIG_PATH);
            ESP_LOGW(TAG, "failed to commit app configuration backup to SD");
            goto done;
        } else {
            remove(ESP_SD_APP_CONFIG_BACKUP);
        }
        result = 0;
        goto done;
    }
    ESP_LOGW(TAG, "failed to write app configuration backup to SD");
done:
    storage_io_unlock();
    return result;
}

static int save_app_configuration(const app_state_t *app, int sd_mounted) {
    int result = save_app_configuration_nvs(app);
    if (sd_mounted && backup_app_configuration_sd(app) != 0) {
        result = -1;
    }
    return result;
}

static void deferred_configuration_save_task(void *context) {
    app_state_t *snapshot;
    (void)context;
    snapshot = heap_caps_malloc(sizeof(*snapshot), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (snapshot == NULL) {
        ESP_LOGE(TAG, "failed to allocate deferred save snapshot");
        deferred_save_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        int result;
        int force_sd_backup;
        static int saves_since_sd_backup;
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(APP_SAVE_DEBOUNCE_MS)) > 0) {
            /* Restart the idle window whenever a newer state arrives. */
        }
        taskENTER_CRITICAL(&deferred_save_lock);
        *snapshot = deferred_save_app;
        force_sd_backup = deferred_save_force_sd_backup;
        deferred_save_force_sd_backup = 0;
        taskEXIT_CRITICAL(&deferred_save_lock);

        result = save_app_configuration_nvs(snapshot);
        saves_since_sd_backup++;
        if (deferred_save_sd_mounted &&
            sd_library_state != SD_LIBRARY_LOADING &&
            (force_sd_backup ||
             saves_since_sd_backup >= APP_SD_BACKUP_SAVE_INTERVAL)) {
            if (backup_app_configuration_sd(snapshot) != 0) {
                result = -1;
            } else {
                saves_since_sd_backup = 0;
            }
        }
        if (result != 0) {
            ESP_LOGW(TAG, "background app state save failed; retrying");
            taskENTER_CRITICAL(&deferred_save_lock);
            if (force_sd_backup) {
                deferred_save_force_sd_backup = 1;
            }
            taskEXIT_CRITICAL(&deferred_save_lock);
            xTaskNotifyGive(deferred_save_task_handle);
        }
    }
}

static int start_deferred_configuration_save_task(int sd_mounted) {
    deferred_save_sd_mounted = sd_mounted;
    if (xTaskCreatePinnedToCore(deferred_configuration_save_task, "app_save",
                                APP_SAVE_TASK_STACK_SIZE, NULL,
                                tskIDLE_PRIORITY + 1,
                                &deferred_save_task_handle, 0) != pdPASS) {
        deferred_save_task_handle = NULL;
        return -1;
    }
    return 0;
}

static void schedule_app_configuration_save_with_backup(const app_state_t *app,
                                                        int force_sd_backup) {
    if (app == NULL) return;
    if (deferred_save_task_handle == NULL) {
        if (save_app_configuration(
                app,
                deferred_save_sd_mounted && sd_library_state != SD_LIBRARY_LOADING) != 0) {
            ESP_LOGW(TAG, "synchronous fallback app state save failed");
        }
        return;
    }
    taskENTER_CRITICAL(&deferred_save_lock);
    deferred_save_app = *app;
    if (force_sd_backup) {
        deferred_save_force_sd_backup = 1;
    }
    taskEXIT_CRITICAL(&deferred_save_lock);
    xTaskNotifyGive(deferred_save_task_handle);
}

static void schedule_app_configuration_save(const app_state_t *app) {
    schedule_app_configuration_save_with_backup(app, 0);
}

static int persisted_app_state_changed(const app_state_t *previous,
                                       const app_state_t *current) {
    app_persisted_state_t previous_snapshot = {0};
    app_persisted_state_t current_snapshot = {0};

    if (previous == NULL || current == NULL) {
        return 1;
    }
    app_persistence_capture(previous, &previous_snapshot);
    app_persistence_capture(current, &current_snapshot);
    return memcmp(&previous_snapshot, &current_snapshot,
                  sizeof(previous_snapshot)) != 0;
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

static long long current_epoch_minute(void) {
    time_t now = time(NULL);
    return now >= (time_t)1700000000 ? (long long)(now / 60) : -1;
}

typedef struct {
    gfx_framebuffer_t *fb;
    esp_display_t *display;
    int last_presented_percent;
} reader_pagination_progress_t;

static void present_reader_pagination_progress(int book_index, int percent, void *context) {
    reader_pagination_progress_t *progress = context;
    char percent_text[16];
    int fill_width;
    (void)book_index;
    if (progress == NULL || progress->fb == NULL || progress->display == NULL) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    /* E-paper progress animation is expensive. Show the initial feedback and
     * one useful midpoint; the completed page is rendered immediately after. */
    if (percent >= 100 ||
        (percent > 0 && percent - progress->last_presented_percent < 50)) {
        return;
    }
    progress->last_presented_percent = percent;
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

static void wifi_scan_task(void *context) {
    (void)context;
    wifi_scan_result_app = wifi_scan_request_app;
    refresh_wifi_networks(&wifi_scan_result_app);
    wifi_scan_task_state = WIFI_SCAN_COMPLETE;
    vTaskDelete(NULL);
}

static int start_wifi_scan(const app_state_t *app) {
    if (app == NULL || wifi_scan_task_state == WIFI_SCAN_RUNNING) {
        return -1;
    }
    wifi_scan_request_app = *app;
    wifi_scan_task_state = WIFI_SCAN_RUNNING;
    if (xTaskCreate(wifi_scan_task, "wifi_scan", WIFI_SCAN_TASK_STACK_SIZE,
                    NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        wifi_scan_task_state = WIFI_SCAN_FAILED;
        return -1;
    }
    return 0;
}

static void apply_wifi_scan_result(app_state_t *app) {
    if (app == NULL) return;
    snprintf(app->wifi_ip, sizeof(app->wifi_ip), "%s", wifi_scan_result_app.wifi_ip);
    memcpy(app->wifi_saved_ssids, wifi_scan_result_app.wifi_saved_ssids,
           sizeof(app->wifi_saved_ssids));
    app->wifi_saved_count = wifi_scan_result_app.wifi_saved_count;
    memcpy(app->wifi_network_ssids, wifi_scan_result_app.wifi_network_ssids,
           sizeof(app->wifi_network_ssids));
    memcpy(app->wifi_network_rssi, wifi_scan_result_app.wifi_network_rssi,
           sizeof(app->wifi_network_rssi));
    memcpy(app->wifi_network_secure, wifi_scan_result_app.wifi_network_secure,
           sizeof(app->wifi_network_secure));
    app->wifi_network_count = wifi_scan_result_app.wifi_network_count;
    app->wifi_network_selection = wifi_scan_result_app.wifi_network_selection;
    app->wifi_scan_in_progress = 0;
    app->wifi_scan_requested = 0;
}

static void page_prefetch_task(void *context) {
    (void)context;
    while (1) {
        int book_index;
        int page_index;
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        taskENTER_CRITICAL(&page_prefetch_lock);
        book_index = page_prefetch_book;
        page_index = page_prefetch_page;
        taskEXIT_CRITICAL(&page_prefetch_lock);
        reader_library_prefetch_adjacent_pages(book_index, page_index);
    }
}

static int start_page_prefetch_task(void) {
    if (xTaskCreatePinnedToCore(page_prefetch_task, "page_prefetch",
                                PAGE_PREFETCH_TASK_STACK_SIZE, NULL,
                                tskIDLE_PRIORITY + 1,
                                &page_prefetch_task_handle, 0) != pdPASS) {
        page_prefetch_task_handle = NULL;
        return -1;
    }
    return 0;
}

static void schedule_page_prefetch(int book_index, int page_index) {
    if (page_prefetch_task_handle == NULL) return;
    taskENTER_CRITICAL(&page_prefetch_lock);
    page_prefetch_book = book_index;
    page_prefetch_page = page_index;
    taskEXIT_CRITICAL(&page_prefetch_lock);
    xTaskNotifyGive(page_prefetch_task_handle);
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
    storage_io_init();
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
    if (start_deferred_configuration_save_task(sd_mounted) != 0) {
        ESP_LOGW(TAG, "background app state save unavailable; using synchronous fallback");
    }
    if (start_page_prefetch_task() != 0) {
        ESP_LOGW(TAG, "reader page prefetch task unavailable");
    }

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
            ESP_LOGI(TAG, "desktop ready; indexing TXT/EPUB books in background");
        } else {
            sd_library_state = SD_LIBRARY_IDLE;
            app.reader_library_loading = 0;
            ESP_LOGE(TAG, "failed to start background book indexing task");
        }
    }

    while (1) {
        app_button_t button;
        int button_repeat_count = 1;
        if (wifi_scan_task_state == WIFI_SCAN_COMPLETE ||
            wifi_scan_task_state == WIFI_SCAN_FAILED) {
            int scan_succeeded = wifi_scan_task_state == WIFI_SCAN_COMPLETE;
            wifi_scan_task_state = WIFI_SCAN_IDLE;
            if (scan_succeeded) {
                apply_wifi_scan_result(&app);
                ESP_LOGI(TAG, "background Wi-Fi scan complete: %d network(s)",
                         app.wifi_saved_count + app.wifi_network_count);
            } else {
                app.wifi_scan_in_progress = 0;
                app.wifi_scan_requested = 0;
                ESP_LOGW(TAG, "background Wi-Fi scan task failed");
            }
            if (app.page == APP_PAGE_WIFI_SETUP) {
                ui_render_page(fb, &app, &font);
                if (esp_display_present_partial(&display, fb, 0, 40,
                                                GFX_WIDTH, GFX_HEIGHT - 40) != 0) {
                    ESP_LOGW(TAG, "failed to show Wi-Fi scan result");
                }
            }
        }
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
            schedule_app_configuration_save(&app);
            ESP_LOGI(TAG, "background indexing complete: loaded %d TXT/EPUB book(s) from %s",
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
                schedule_app_configuration_save(&app);
                ESP_LOGI(TAG, "background pagination complete for book %d", completed_book);
                if (app.page == APP_PAGE_READER && app.current_book == completed_book) {
                    ui_render_page(fb, &app, &font);
                    if (esp_display_present_partial(&display, fb, 0, 32,
                                                    GFX_WIDTH, GFX_HEIGHT - 32) != 0) {
                        ESP_LOGW(TAG, "failed to refresh reader after background pagination");
                    }
                    schedule_page_prefetch(app.current_book, app.reader_page);
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
        if (esp_input_poll_button_batch(&input, &button, &button_repeat_count)) {
            app_state_t previous_app = app;
            app_page_t previous_page = previous_app.page;
            int button_state_changed;
            int persistence_changed;
            int64_t button_started_us = esp_timer_get_time();
            int64_t render_time_us = 0;
            int64_t display_time_us = 0;
            reader_pagination_progress_t button_pagination_progress = {fb, &display, -100};
            ESP_LOGD(TAG, "button event %d on page %s", button, app_page_name(app.page));
            if (button == APP_BUTTON_POWER_LONG) {
                schedule_app_configuration_save_with_backup(&app, 1);
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
            for (int repeat = 0; repeat < button_repeat_count; repeat++) {
                app_handle_button(&app, button);
            }
            if (sd_library_state != SD_LIBRARY_LOADING) {
                reader_library_set_progress_callback(NULL, NULL);
            }
            if (previous_page != APP_PAGE_WIFI_SETUP && app.page == APP_PAGE_WIFI_SETUP) {
                esp_time_sync_load_credentials(app.wifi_ssid, sizeof(app.wifi_ssid),
                                                app.wifi_password, sizeof(app.wifi_password));
                app.wifi_scan_requested = 1;
            }
            if (app.page == APP_PAGE_WIFI_SETUP && app.wifi_scan_requested) {
                if (wifi_scan_task_state == WIFI_SCAN_RUNNING) {
                    app.wifi_scan_requested = 0;
                    app.wifi_scan_in_progress = 1;
                    ESP_LOGD(TAG, "Wi-Fi scan already running");
                } else {
                    app.wifi_scan_in_progress = 1;
                    app.wifi_scan_requested = 0;
                    ui_render_page(fb, &app, &font);
                    if (esp_display_present_partial(&display, fb, 0, 40,
                                                    GFX_WIDTH, GFX_HEIGHT - 40) != 0) {
                        ESP_LOGW(TAG, "failed to show Wi-Fi scan progress");
                    }
                    if (start_wifi_scan(&app) != 0) {
                        app.wifi_scan_in_progress = 0;
                        wifi_scan_task_state = WIFI_SCAN_FAILED;
                    }
                }
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
            button_state_changed = memcmp(&previous_app, &app, sizeof(app)) != 0;
            persistence_changed = persisted_app_state_changed(&previous_app, &app);
            if (button_state_changed) {
                int present_result;
                int64_t phase_started_us = esp_timer_get_time();
                ui_render_page(fb, &app, &font);
                render_time_us = esp_timer_get_time() - phase_started_us;
                phase_started_us = esp_timer_get_time();
                present_result = esp_display_present_auto(&display, fb);
                if (present_result != 0 &&
                    esp_display_present(&display, fb) != 0) {
                    ESP_LOGE(TAG, "failed to present button frame");
                }
                display_time_us = esp_timer_get_time() - phase_started_us;
            } else {
                ESP_LOGD(TAG, "button produced no visible state change; skipped refresh");
            }

            /* Persist after the visible update so flash and SD latency never
             * delays button feedback. Transient cursor/menu state is skipped. */
            if (sd_library_state == SD_LIBRARY_LOADING) {
                ESP_LOGD(TAG, "deferred state save until book library is ready");
            } else if (persistence_changed) {
                schedule_app_configuration_save(&app);
                ESP_LOGD(TAG, "scheduled app state save after %d ms idle",
                         APP_SAVE_DEBOUNCE_MS);
            } else {
                ESP_LOGD(TAG, "skipped persistence for transient UI state");
            }
            {
                reader_pagination_progress_t pagination_progress = {fb, &display, -100};
                int layout_applied;
                reader_library_set_progress_callback(present_reader_pagination_progress,
                                                      &pagination_progress);
                layout_applied = app_apply_pending_reader_layout(&app);
                reader_library_set_progress_callback(NULL, NULL);
                if (layout_applied) {
                    ESP_LOGI(TAG, "reader layout re-pagination complete");
                    schedule_app_configuration_save(&app);
                    ui_render_page(fb, &app, &font);
                    if (esp_display_present_partial(&display, fb, 0, 32,
                                                    GFX_WIDTH, GFX_HEIGHT - 32) != 0) {
                        ESP_LOGE(TAG, "failed to present re-paginated reader frame");
                    }
                }
            }
            if (app.page == APP_PAGE_READER) {
                start_reader_background_pagination(app.current_book);
                schedule_page_prefetch(app.current_book, app.reader_page);
            }
            ESP_LOGI(TAG,
                     "button performance: event=%d repeats=%d render=%lldms display=%lldms total=%lldms",
                     (int)button, button_repeat_count,
                     (long long)(render_time_us / 1000),
                     (long long)(display_time_us / 1000),
                     (long long)((esp_timer_get_time() - button_started_us) / 1000));
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
