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
    APP_PAGE_SNAKE,
    APP_PAGE_ABOUT
} app_page_t;

typedef enum {
    APP_BUTTON_POWER = 0,
    APP_BUTTON_UP,
    APP_BUTTON_HOME,
    APP_BUTTON_DOWN
} app_button_t;

typedef struct {
    app_page_t page;
    int home_selection;
    int bookshelf_selection;
    int reader_page;
    int weather_refreshes;
    int calendar_month_offset;
    int english_word;
    int english_show_back;
    int settings_selection;
    int power_saving_enabled;
    int snake_x;
    int snake_y;
    int snake_score;
} app_state_t;

void app_init(app_state_t *app);
void app_handle_button(app_state_t *app, app_button_t button);
const char *app_page_name(app_page_t page);

#endif
