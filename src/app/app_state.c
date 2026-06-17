#include "app/app_state.h"

#include <stddef.h>

#define HOME_ITEM_COUNT 7
#define BOOK_COUNT 3
#define READER_PAGE_COUNT 5
#define ENGLISH_WORD_COUNT 3
#define SETTINGS_COUNT 6

static app_page_t page_for_home_selection(int selection) {
    switch (selection) {
        case 0:
            return APP_PAGE_BOOKSHELF;
        case 1:
            return APP_PAGE_WEATHER;
        case 2:
            return APP_PAGE_CALENDAR;
        case 3:
            return APP_PAGE_SNAKE;
        case 4:
            return APP_PAGE_ENGLISH;
        case 5:
            return APP_PAGE_SETTINGS;
        case 6:
        default:
            return APP_PAGE_ABOUT;
    }
}

static int wrap_index(int value, int count) {
    if (value < 0) {
        return count - 1;
    }
    if (value >= count) {
        return 0;
    }
    return value;
}

void app_init(app_state_t *app) {
    if (app == NULL) {
        return;
    }

    app->page = APP_PAGE_HOME;
    app->home_selection = 0;
    app->bookshelf_selection = 0;
    app->reader_page = 0;
    app->weather_refreshes = 0;
    app->calendar_month_offset = 0;
    app->english_word = 0;
    app->english_show_back = 0;
    app->settings_selection = 0;
    app->power_saving_enabled = 1;
    app->snake_x = 20;
    app->snake_y = 13;
    app->snake_score = 12;
}

static void handle_home(app_state_t *app, app_button_t button) {
    if (button == APP_BUTTON_UP) {
        app->home_selection = wrap_index(app->home_selection - 1, HOME_ITEM_COUNT);
    } else if (button == APP_BUTTON_DOWN) {
        app->home_selection = wrap_index(app->home_selection + 1, HOME_ITEM_COUNT);
    } else if (button == APP_BUTTON_HOME) {
        app->page = page_for_home_selection(app->home_selection);
    }
}

static void handle_bookshelf(app_state_t *app, app_button_t button) {
    if (button == APP_BUTTON_UP) {
        app->bookshelf_selection = wrap_index(app->bookshelf_selection - 1, BOOK_COUNT);
    } else if (button == APP_BUTTON_DOWN) {
        app->bookshelf_selection = wrap_index(app->bookshelf_selection + 1, BOOK_COUNT);
    } else if (button == APP_BUTTON_HOME) {
        app->page = APP_PAGE_READER;
        app->reader_page = 0;
    }
}

static void handle_reader(app_state_t *app, app_button_t button) {
    if (button == APP_BUTTON_UP && app->reader_page > 0) {
        app->reader_page--;
    } else if (button == APP_BUTTON_DOWN && app->reader_page < READER_PAGE_COUNT - 1) {
        app->reader_page++;
    } else if (button == APP_BUTTON_HOME) {
        app->page = APP_PAGE_BOOKSHELF;
    }
}

static void handle_secondary_page(app_state_t *app, app_button_t button) {
    switch (app->page) {
        case APP_PAGE_WEATHER:
            if (button == APP_BUTTON_HOME) {
                app->weather_refreshes++;
            }
            break;
        case APP_PAGE_CALENDAR:
            if (button == APP_BUTTON_UP) {
                app->calendar_month_offset--;
            } else if (button == APP_BUTTON_DOWN) {
                app->calendar_month_offset++;
            }
            break;
        case APP_PAGE_ENGLISH:
            if (button == APP_BUTTON_UP) {
                app->english_word = wrap_index(app->english_word - 1, ENGLISH_WORD_COUNT);
                app->english_show_back = 0;
            } else if (button == APP_BUTTON_DOWN) {
                app->english_word = wrap_index(app->english_word + 1, ENGLISH_WORD_COUNT);
                app->english_show_back = 0;
            } else if (button == APP_BUTTON_HOME) {
                app->english_show_back = !app->english_show_back;
            }
            break;
        case APP_PAGE_SETTINGS:
            if (button == APP_BUTTON_UP) {
                app->settings_selection = wrap_index(app->settings_selection - 1, SETTINGS_COUNT);
            } else if (button == APP_BUTTON_DOWN) {
                app->settings_selection = wrap_index(app->settings_selection + 1, SETTINGS_COUNT);
            } else if (button == APP_BUTTON_HOME && app->settings_selection == 5) {
                app->power_saving_enabled = !app->power_saving_enabled;
            }
            break;
        case APP_PAGE_SNAKE:
            if (button == APP_BUTTON_UP && app->snake_y > 0) {
                app->snake_y--;
            } else if (button == APP_BUTTON_DOWN && app->snake_y < 25) {
                app->snake_y++;
            } else if (button == APP_BUTTON_HOME) {
                app->snake_x = app->snake_x < 38 ? app->snake_x + 1 : 1;
            }
            break;
        default:
            break;
    }
}

void app_handle_button(app_state_t *app, app_button_t button) {
    if (app == NULL) {
        return;
    }

    if (button == APP_BUTTON_POWER && app->page != APP_PAGE_HOME) {
        app->page = APP_PAGE_HOME;
        return;
    }

    switch (app->page) {
        case APP_PAGE_HOME:
            handle_home(app, button);
            break;
        case APP_PAGE_BOOKSHELF:
            handle_bookshelf(app, button);
            break;
        case APP_PAGE_READER:
            handle_reader(app, button);
            break;
        default:
            handle_secondary_page(app, button);
            break;
    }
}

const char *app_page_name(app_page_t page) {
    switch (page) {
        case APP_PAGE_HOME:
            return "home";
        case APP_PAGE_BOOKSHELF:
            return "bookshelf";
        case APP_PAGE_READER:
            return "reader";
        case APP_PAGE_WEATHER:
            return "weather";
        case APP_PAGE_CALENDAR:
            return "calendar";
        case APP_PAGE_ENGLISH:
            return "english";
        case APP_PAGE_SETTINGS:
            return "settings";
        case APP_PAGE_SNAKE:
            return "snake";
        case APP_PAGE_ABOUT:
            return "about";
        default:
            return "unknown";
    }
}
