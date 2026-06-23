#ifndef APP_STATE_H
#define APP_STATE_H

typedef enum {
    APP_PAGE_HOME = 0,
    APP_PAGE_BOOKSHELF,
    APP_PAGE_READER,
    APP_PAGE_WEATHER,
    APP_PAGE_CALENDAR,
    APP_PAGE_ENGLISH,
    APP_PAGE_SETTINGS,
    APP_PAGE_ABOUT
} app_page_t;

typedef enum {
    APP_BUTTON_POWER = 0,
    APP_BUTTON_UP,
    APP_BUTTON_HOME,
    APP_BUTTON_DOWN,
    APP_BUTTON_POWER_LONG
} app_button_t;

#define APP_BOOK_COUNT 3
#define APP_ENGLISH_WORD_COUNT 3

typedef struct {
    app_page_t page;
    int home_selection;
    int bookshelf_selection;
    int current_book;
    int recent_book;
    int book_pages[APP_BOOK_COUNT];
    int book_current_pages[APP_BOOK_COUNT];
    int book_bookmark_pages[APP_BOOK_COUNT];
    int reader_page;
    int reader_menu_open;
    int reader_menu_selection;
    int reader_catalog_open;
    int reader_catalog_selection;
    int bookmark_added;
    int weather_refreshes;
    int weather_city_index;
    int weather_stale;
    int weather_last_updated_minutes;
    int calendar_month_offset;
    int calendar_selected_day;
    int calendar_detail_open;
    int english_word;
    int english_show_back;
    int english_known_count;
    int english_review_count;
    int english_answer_state[APP_ENGLISH_WORD_COUNT];
    int settings_selection;
    int font_size_index;
    int line_spacing_index;
    int wifi_connected;
    int power_saving_enabled;
} app_state_t;

void app_init(app_state_t *app);
void app_handle_button(app_state_t *app, app_button_t button);
const char *app_page_name(app_page_t page);

#endif
