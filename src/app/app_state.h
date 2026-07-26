#ifndef APP_STATE_H
#define APP_STATE_H

typedef enum {
    APP_PAGE_HOME = 0,
    APP_PAGE_BOOKSHELF,
    APP_PAGE_FILE_BROWSER,
    APP_PAGE_READER,
    APP_PAGE_READER_CATALOG,
    APP_PAGE_READER_SETTINGS,
    APP_PAGE_READER_FONT,
    APP_PAGE_WEATHER,
    APP_PAGE_CALENDAR,
    APP_PAGE_ENGLISH,
    APP_PAGE_SETTINGS,
    APP_PAGE_SYSTEM_FONT,
    APP_PAGE_WIFI_SETUP,
    APP_PAGE_ABOUT
} app_page_t;

typedef enum {
    APP_BUTTON_POWER = 0,
    APP_BUTTON_UP,
    APP_BUTTON_HOME,
    APP_BUTTON_DOWN,
    APP_BUTTON_BACK,
    APP_BUTTON_POWER_LONG
} app_button_t;

#define APP_BOOK_COUNT 9
#define APP_ENGLISH_WORD_COUNT 3
#define APP_WIFI_SCAN_MAX 10
#define APP_WIFI_SAVED_MAX 5

typedef enum {
    APP_WIFI_STAGE_NETWORKS = 0,
    APP_WIFI_STAGE_KEYBOARD,
    APP_WIFI_STAGE_CONFIRM
} app_wifi_stage_t;

typedef struct {
    app_page_t page;
    int home_selection;
    int bookshelf_selection;
    int bookshelf_layout;
    int file_browser_selection;
    int file_browser_error;
    int reader_library_loading;
    int reader_library_progress;
    int current_book;
    int recent_book;
    int book_pages[APP_BOOK_COUNT];
    int book_current_pages[APP_BOOK_COUNT];
    int book_bookmark_pages[APP_BOOK_COUNT];
    int book_repagination_old_pages[APP_BOOK_COUNT];
    int book_repagination_old_totals[APP_BOOK_COUNT];
    int book_repagination_preview_pages[APP_BOOK_COUNT];
    int reader_page;
    int reader_background_pagination_active;
    int reader_background_pagination_progress;
    int reader_page_turn_pending;
    int reader_menu_open;
    int reader_menu_selection;
    int reader_catalog_open;
    int reader_catalog_selection;
    int reader_settings_selection;
    int reader_settings_editing;
    int reader_pending_font_size_index;
    int reader_pending_font_index;
    int reader_pending_line_spacing_index;
    int reader_pending_margin_index;
    int reader_pending_indent_enabled;
    int reader_pending_bold_enabled;
    int reader_pending_page_turn_mode;
    int reader_pending_refresh_mode;
    int reader_layout_apply_requested;
    int reader_margin_index;
    int reader_indent_enabled;
    int reader_bold_enabled;
    int reader_page_turn_mode;
    int reader_refresh_mode;
    int bookmark_added;
    int weather_refreshes;
    int weather_city_index;
    int weather_stale;
    int weather_last_updated_minutes;
    int weather_type; /* 0=sunny, 1=cloudy, 2=rainy, 3=snowy */
    int weather_valid;
    int weather_temperature;
    int weather_humidity;
    char weather_text[24];
    char weather_error[64];
    char weather_wind[32];
    int weather_scroll;
    int calendar_month_offset;
    int calendar_selected_day;
    int calendar_detail_open;
    int english_word;
    int english_show_back;
    int english_known_count;
    int english_review_count;
    int english_answer_state[APP_ENGLISH_WORD_COUNT];
    int settings_selection;
    int settings_scroll;
    int bluetooth_enabled;
    int dictionary_enabled;
    int time_sync_requested;
    int update_check_requested;
    int reader_font_index;
    int reader_font_selection;
    int system_font_index;
    int system_font_selection;
    int font_size_index;
    int line_spacing_index;
    int wifi_connected;
    int time_synchronized;
    int battery_valid;
    int battery_percent;
    int wifi_setup_selection;
    int wifi_editor_active;
    int wifi_edit_char_index;
    int wifi_config_save_requested;
    int wifi_setup_stage;
    int wifi_network_count;
    int wifi_network_selection;
    int wifi_scan_requested;
    int wifi_scan_in_progress;
    char wifi_network_ssids[APP_WIFI_SCAN_MAX][33];
    int wifi_network_rssi[APP_WIFI_SCAN_MAX];
    int wifi_network_secure[APP_WIFI_SCAN_MAX];
    int wifi_saved_count;
    char wifi_saved_ssids[APP_WIFI_SAVED_MAX][33];
    char wifi_ip[16];
    char wifi_ssid[33];
    char wifi_password[65];
    int power_saving_enabled;
} app_state_t;

void app_init(app_state_t *app);
/* Reconcile saved reading state with the books currently indexed from SD. */
void app_sync_reader_library(app_state_t *app);
/* Apply a deferred reader layout change after leaving the reader settings page. */
int app_apply_pending_reader_layout(app_state_t *app);
void app_finish_background_pagination(app_state_t *app, int book_index);
void app_handle_button(app_state_t *app, app_button_t button);
const char *app_page_name(app_page_t page);

#endif
