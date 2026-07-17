#include "app/app_state.h"
#include "app/file_browser.h"
#include "app/reader_library.h"

#include <stddef.h>
#include <string.h>

#define HOME_ITEM_COUNT 7
#define READER_MENU_COUNT 5
#define READER_SETTINGS_COUNT 10
#define READER_FONT_COUNT 4
#define WEATHER_CITY_COUNT 3
#define WEATHER_SCROLL_STEP 120
#define WEATHER_SCROLL_MAX 190
#define CALENDAR_DAYS_IN_MONTH 30
#define SETTINGS_COUNT 6
#define SETTINGS_SCROLL_STEP 120
#define SETTINGS_SCROLL_MAX 220
#define WIFI_SETUP_ROWS 3

static const char wifi_edit_characters[] =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.@!#";

static app_page_t page_for_home_selection(int selection) {
    switch (selection) {
        case 0:
            return APP_PAGE_BOOKSHELF;
        case 1:
            return APP_PAGE_FILE_BROWSER;
        case 2:
            return APP_PAGE_WEATHER;
        case 3:
            return APP_PAGE_CALENDAR;
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

static int clamp_page(int page, int total) {
    if (page < 0) {
        return 0;
    }
    if (page >= total) {
        return total - 1;
    }
    return page;
}

static int chapter_page_for_selection(const app_state_t *app, int selection) {
    int total = app->book_pages[app->current_book];
    int page = reader_library_chapter_page(app->current_book, selection);
    return clamp_page(page, total);
}

static void sync_reader_page(app_state_t *app) {
    int total = app->book_pages[app->current_book];
    app->reader_page = clamp_page(app->reader_page, total);
    app->book_current_pages[app->current_book] = app->reader_page;
}

static void record_english_answer(app_state_t *app, int known) {
    int word = app->english_word;
    int previous = app->english_answer_state[word];
    if (previous == 1) {
        app->english_known_count--;
    } else if (previous == 2) {
        app->english_review_count--;
    }
    app->english_answer_state[word] = known ? 1 : 2;
    if (known) {
        app->english_known_count++;
    } else {
        app->english_review_count++;
    }
    app->english_word = wrap_index(app->english_word + 1, APP_ENGLISH_WORD_COUNT);
    app->english_show_back = 0;
}

void app_sync_reader_library(app_state_t *app) {
    int book_count;
    if (app == NULL) {
        return;
    }
    book_count = reader_library_book_count();
    for (int i = 0; i < APP_BOOK_COUNT; i++) {
        app->book_pages[i] = i < book_count ? reader_library_page_count(i) : 0;
        if (i >= book_count) {
            app->book_current_pages[i] = 0;
            app->book_bookmark_pages[i] = -1;
        }
    }
    if (book_count == 0) {
        app->bookshelf_selection = 0;
        app->current_book = 0;
        app->reader_page = 0;
        return;
    }
    if (app->bookshelf_selection < 0 || app->bookshelf_selection >= book_count) {
        app->bookshelf_selection = 0;
    }
    if (app->current_book < 0 || app->current_book >= book_count) {
        app->current_book = 0;
    }
    sync_reader_page(app);
}

void app_init(app_state_t *app) {
    if (app == NULL) {
        return;
    }

    app->page = APP_PAGE_HOME;
    app->home_selection = 0;
    app->bookshelf_selection = 0;
    app->file_browser_selection = 0;
    app->file_browser_error = file_browser_open("/sdcard") < 0;
    app->current_book = 0;
    app->recent_book = -1;
    for (int i = 0; i < APP_BOOK_COUNT; i++) {
        app->book_pages[i] = 0;
        app->book_current_pages[i] = 0;
        app->book_bookmark_pages[i] = -1;
    }
    app->reader_page = 0;
    app->reader_menu_open = 0;
    app->reader_menu_selection = 0;
    app->reader_catalog_open = 0;
    app->reader_catalog_selection = 0;
    app->reader_settings_selection = 9;
    app->reader_margin_index = 1;
    app->reader_indent_enabled = 1;
    app->reader_bold_enabled = 0;
    app->reader_page_turn_mode = 0;
    app->reader_refresh_mode = 0;
    app->bookmark_added = 0;
    app->weather_refreshes = 0;
    app->weather_city_index = 0;
    app->weather_stale = 0;
    app->weather_last_updated_minutes = 15;
    app->weather_type = 0; /* sunny - default */
    app->weather_scroll = 0;
    app->calendar_month_offset = 0;
    app->calendar_selected_day = 21;
    app->calendar_detail_open = 0;
    app->english_word = 0;
    app->english_show_back = 0;
    app->english_known_count = 0;
    app->english_review_count = 0;
    for (int i = 0; i < APP_ENGLISH_WORD_COUNT; i++) {
        app->english_answer_state[i] = 0;
    }
    app->settings_selection = 0;
    app->settings_scroll = 0;
    app->reader_font_index = 0;
    app->font_size_index = 2;
    app->line_spacing_index = 2;
    app->wifi_connected = 0;
    app->wifi_setup_selection = 0;
    app->wifi_editor_active = 0;
    app->wifi_edit_char_index = 0;
    app->wifi_config_save_requested = 0;
    app->wifi_ssid[0] = '\0';
    app->wifi_password[0] = '\0';
    app->power_saving_enabled = 1;
    app_sync_reader_library(app);
}

static void handle_home(app_state_t *app, app_button_t button) {
    if (button == APP_BUTTON_UP) {
        app->home_selection = wrap_index(app->home_selection - 1, HOME_ITEM_COUNT);
    } else if (button == APP_BUTTON_DOWN) {
        app->home_selection = wrap_index(app->home_selection + 1, HOME_ITEM_COUNT);
    } else if (button == APP_BUTTON_HOME) {
        if (app->home_selection == 1) {
            app->file_browser_selection = 0;
            app->file_browser_error = file_browser_open("/sdcard") < 0;
        }
        app->page = page_for_home_selection(app->home_selection);
    }
}

static void handle_bookshelf(app_state_t *app, app_button_t button) {
    int book_count = reader_library_book_count();
    if (book_count <= 0) {
        return;
    }
    if (button == APP_BUTTON_UP) {
        app->bookshelf_selection = wrap_index(app->bookshelf_selection - 1, book_count);
    } else if (button == APP_BUTTON_DOWN) {
        app->bookshelf_selection = wrap_index(app->bookshelf_selection + 1, book_count);
    } else if (button == APP_BUTTON_HOME) {
        app->current_book = app->bookshelf_selection;
        app->recent_book = app->current_book;
        app->page = APP_PAGE_READER;
        app->reader_page = app->book_current_pages[app->current_book];
        app->reader_menu_open = 0;
        app->reader_menu_selection = 0;
        app->reader_catalog_open = 0;
        app->reader_catalog_selection = 0;
    }
}

static void handle_file_browser(app_state_t *app, app_button_t button) {
    int count = file_browser_count();
    if (button == APP_BUTTON_UP && count > 0) {
        app->file_browser_selection = wrap_index(app->file_browser_selection - 1, count);
        app->file_browser_error = 0;
    } else if (button == APP_BUTTON_DOWN && count > 0) {
        app->file_browser_selection = wrap_index(app->file_browser_selection + 1, count);
        app->file_browser_error = 0;
    } else if (button == APP_BUTTON_POWER) {
        if (file_browser_at_root()) {
            app->page = APP_PAGE_HOME;
        } else {
            app->file_browser_error = file_browser_go_parent() < 0;
            app->file_browser_selection = 0;
        }
    } else if (button == APP_BUTTON_HOME && count > 0) {
        const file_browser_entry_t *entry = file_browser_entry(app->file_browser_selection);
        if (entry == NULL) {
            app->file_browser_error = 1;
        } else if (entry->is_directory) {
            app->file_browser_error = file_browser_enter_directory(app->file_browser_selection) < 0;
            app->file_browser_selection = 0;
        } else if (entry->is_text) {
            char path[FILE_BROWSER_PATH_MAX];
            if (file_browser_entry_path(app->file_browser_selection, path, sizeof(path)) == 0 &&
                reader_library_open_book_file(0, path) == 0) {
                app->current_book = 0;
                app->recent_book = 0;
                app_sync_reader_library(app);
                app->reader_page = 0;
                app->reader_menu_open = 0;
                app->page = APP_PAGE_READER;
                app->file_browser_error = 0;
            } else {
                app->file_browser_error = 1;
            }
        } else {
            app->file_browser_error = 2;
        }
    }
}

static void handle_reader(app_state_t *app, app_button_t button) {
    if (app->reader_menu_open) {
        if (button == APP_BUTTON_UP) {
            app->reader_menu_selection = wrap_index(app->reader_menu_selection - 1, READER_MENU_COUNT);
        } else if (button == APP_BUTTON_DOWN) {
            app->reader_menu_selection = wrap_index(app->reader_menu_selection + 1, READER_MENU_COUNT);
        } else if (button == APP_BUTTON_POWER) {
            app->reader_menu_open = 0;
        } else if (button == APP_BUTTON_HOME) {
            if (app->reader_menu_selection == 1) {
                app->reader_menu_open = 0;
                app->reader_catalog_open = 0;
                app->reader_catalog_selection = 0;
                app->page = APP_PAGE_READER_CATALOG;
            } else if (app->reader_menu_selection == 2) {
                app->book_bookmark_pages[app->current_book] = app->reader_page;
                app->bookmark_added = 1;
                app->reader_menu_open = 0;
            } else if (app->reader_menu_selection == 3) {
                app->reader_menu_open = 0;
                app->reader_catalog_open = 0;
                app->page = APP_PAGE_READER_SETTINGS;
                app->reader_settings_selection = 9;
            } else if (app->reader_menu_selection == 4) {
                app->reader_menu_open = 0;
                app->reader_catalog_open = 0;
                app->page = APP_PAGE_BOOKSHELF;
            } else {
                app->reader_menu_open = 0;
            }
        }
        return;
    }

    if (button == APP_BUTTON_POWER) {
        return;
    } else if (button == APP_BUTTON_UP && app->reader_page > 0) {
        app->reader_page--;
        sync_reader_page(app);
    } else if (button == APP_BUTTON_DOWN && app->reader_page < app->book_pages[app->current_book] - 1) {
        app->reader_page++;
        sync_reader_page(app);
    } else if (button == APP_BUTTON_HOME) {
        app->reader_menu_open = 1;
        app->reader_menu_selection = 0;
        app->reader_catalog_open = 0;
    }
}

static void handle_reader_catalog(app_state_t *app, app_button_t button) {
    if (button == APP_BUTTON_UP) {
        app->reader_catalog_selection = wrap_index(app->reader_catalog_selection - 1, reader_library_chapter_count(app->current_book));
    } else if (button == APP_BUTTON_DOWN) {
        app->reader_catalog_selection = wrap_index(app->reader_catalog_selection + 1, reader_library_chapter_count(app->current_book));
    } else if (button == APP_BUTTON_POWER) {
        app->page = APP_PAGE_READER;
    } else if (button == APP_BUTTON_HOME) {
        app->reader_page = chapter_page_for_selection(app, app->reader_catalog_selection);
        sync_reader_page(app);
        app->page = APP_PAGE_READER;
    }
}

static void reset_reader_settings(app_state_t *app) {
    app->reader_font_index = 0;
    app->font_size_index = 2;
    app->line_spacing_index = 2;
    app->reader_margin_index = 1;
    app->reader_indent_enabled = 1;
    app->reader_bold_enabled = 0;
    app->reader_page_turn_mode = 0;
    app->reader_refresh_mode = 0;
}

static void handle_reader_settings(app_state_t *app, app_button_t button) {
    if (button == APP_BUTTON_UP) {
        app->reader_settings_selection = wrap_index(app->reader_settings_selection - 1, READER_SETTINGS_COUNT);
    } else if (button == APP_BUTTON_DOWN) {
        app->reader_settings_selection = wrap_index(app->reader_settings_selection + 1, READER_SETTINGS_COUNT);
    } else if (button == APP_BUTTON_POWER) {
        app->page = APP_PAGE_READER;
    } else if (button == APP_BUTTON_HOME) {
        switch (app->reader_settings_selection) {
            case 0:
                app->font_size_index = wrap_index(app->font_size_index + 1, 5);
                break;
            case 1:
                app->reader_font_index = wrap_index(app->reader_font_index + 1, READER_FONT_COUNT);
                break;
            case 2:
                app->line_spacing_index = wrap_index(app->line_spacing_index + 1, 3);
                break;
            case 3:
                app->reader_margin_index = wrap_index(app->reader_margin_index + 1, 4);
                break;
            case 4:
                app->reader_indent_enabled = !app->reader_indent_enabled;
                break;
            case 5:
                app->reader_bold_enabled = !app->reader_bold_enabled;
                break;
            case 6:
                app->reader_page_turn_mode = wrap_index(app->reader_page_turn_mode + 1, 3);
                break;
            case 7:
                app->reader_refresh_mode = wrap_index(app->reader_refresh_mode + 1, 3);
                break;
            case 8:
                reset_reader_settings(app);
                app->reader_settings_selection = 9;
                break;
            case 9:
            default:
                app->page = APP_PAGE_READER;
                break;
        }
    }
}

static void handle_secondary_page(app_state_t *app, app_button_t button) {
    switch (app->page) {
        case APP_PAGE_WEATHER:
            if (button == APP_BUTTON_UP) {
                app->weather_city_index = wrap_index(app->weather_city_index - 1, WEATHER_CITY_COUNT);
                app->weather_scroll -= WEATHER_SCROLL_STEP;
                if (app->weather_scroll < 0) {
                    app->weather_scroll = 0;
                }
                app->weather_stale = 1;
            } else if (button == APP_BUTTON_DOWN) {
                app->weather_city_index = wrap_index(app->weather_city_index + 1, WEATHER_CITY_COUNT);
                app->weather_scroll += WEATHER_SCROLL_STEP;
                if (app->weather_scroll > WEATHER_SCROLL_MAX) {
                    app->weather_scroll = WEATHER_SCROLL_MAX;
                }
                app->weather_stale = 1;
            } else if (button == APP_BUTTON_HOME && app->wifi_connected) {
                app->weather_refreshes++;
                app->weather_stale = 0;
                app->weather_last_updated_minutes = 0;
            } else if (button == APP_BUTTON_HOME) {
                app->weather_stale = 1;
                app->weather_last_updated_minutes += 30;
            }
            break;
        case APP_PAGE_CALENDAR:
            if (app->calendar_detail_open) {
                if (button == APP_BUTTON_UP) {
                    app->calendar_selected_day -= 7;
                    if (app->calendar_selected_day < 1) {
                        app->calendar_selected_day = 1;
                    }
                } else if (button == APP_BUTTON_DOWN) {
                    app->calendar_selected_day += 7;
                    if (app->calendar_selected_day > CALENDAR_DAYS_IN_MONTH) {
                        app->calendar_selected_day = CALENDAR_DAYS_IN_MONTH;
                    }
                } else if (button == APP_BUTTON_HOME) {
                    app->calendar_detail_open = 0;
                }
            } else if (button == APP_BUTTON_UP) {
                app->calendar_month_offset--;
                app->calendar_selected_day = 21;
            } else if (button == APP_BUTTON_DOWN) {
                app->calendar_month_offset++;
                app->calendar_selected_day = 21;
            } else if (button == APP_BUTTON_HOME) {
                app->calendar_detail_open = 1;
            }
            break;
        case APP_PAGE_ENGLISH:
            if (app->english_show_back && button == APP_BUTTON_UP) {
                record_english_answer(app, 0);
            } else if (app->english_show_back && button == APP_BUTTON_DOWN) {
                record_english_answer(app, 1);
            } else if (button == APP_BUTTON_UP) {
                app->english_word = wrap_index(app->english_word - 1, APP_ENGLISH_WORD_COUNT);
                app->english_show_back = 0;
            } else if (button == APP_BUTTON_DOWN) {
                app->english_word = wrap_index(app->english_word + 1, APP_ENGLISH_WORD_COUNT);
                app->english_show_back = 0;
            } else if (button == APP_BUTTON_HOME) {
                app->english_show_back = !app->english_show_back;
            }
            break;
        case APP_PAGE_SETTINGS:
            if (button == APP_BUTTON_UP) {
                app->settings_scroll -= SETTINGS_SCROLL_STEP;
                if (app->settings_scroll < 0) {
                    app->settings_scroll = 0;
                }
            } else if (button == APP_BUTTON_DOWN) {
                app->settings_scroll += SETTINGS_SCROLL_STEP;
                if (app->settings_scroll > SETTINGS_SCROLL_MAX) {
                    app->settings_scroll = SETTINGS_SCROLL_MAX;
                }
            } else if (button == APP_BUTTON_HOME) {
                app->page = APP_PAGE_WIFI_SETUP;
                app->wifi_setup_selection = 0;
                app->wifi_editor_active = 0;
                app->wifi_edit_char_index = 0;
            }
            break;
        case APP_PAGE_WIFI_SETUP:
            if (app->wifi_editor_active) {
                char *value = app->wifi_setup_selection == 0 ? app->wifi_ssid : app->wifi_password;
                int max_length = app->wifi_setup_selection == 0 ? 32 : 64;
                int length = (int)strlen(value);
                int char_count = (int)strlen(wifi_edit_characters);
                if (button == APP_BUTTON_UP) {
                    app->wifi_edit_char_index = wrap_index(app->wifi_edit_char_index - 1, char_count);
                } else if (button == APP_BUTTON_DOWN) {
                    app->wifi_edit_char_index = wrap_index(app->wifi_edit_char_index + 1, char_count);
                } else if (button == APP_BUTTON_HOME && length < max_length) {
                    value[length] = wifi_edit_characters[app->wifi_edit_char_index];
                    value[length + 1] = '\0';
                } else if (button == APP_BUTTON_POWER && length > 0) {
                    value[length - 1] = '\0';
                } else if (button == APP_BUTTON_BACK) {
                    app->wifi_editor_active = 0;
                }
            } else if (button == APP_BUTTON_UP) {
                app->wifi_setup_selection = wrap_index(app->wifi_setup_selection - 1, WIFI_SETUP_ROWS);
            } else if (button == APP_BUTTON_DOWN) {
                app->wifi_setup_selection = wrap_index(app->wifi_setup_selection + 1, WIFI_SETUP_ROWS);
            } else if (button == APP_BUTTON_HOME) {
                if (app->wifi_setup_selection < 2) {
                    app->wifi_editor_active = 1;
                    app->wifi_edit_char_index = 0;
                } else if (app->wifi_ssid[0] != '\0') {
                    app->wifi_config_save_requested = 1;
                }
            } else if (button == APP_BUTTON_BACK) {
                app->page = APP_PAGE_SETTINGS;
            }
            break;
        case APP_PAGE_READER_SETTINGS:
            handle_reader_settings(app, button);
            break;
        default:
            break;
    }
}

void app_handle_button(app_state_t *app, app_button_t button) {
    if (app == NULL) {
        return;
    }

    if (button == APP_BUTTON_BACK) {
        switch (app->page) {
            case APP_PAGE_HOME:
                return;
            case APP_PAGE_FILE_BROWSER:
                handle_file_browser(app, APP_BUTTON_POWER);
                return;
            case APP_PAGE_READER:
                if (app->reader_menu_open) {
                    app->reader_menu_open = 0;
                } else {
                    app->page = APP_PAGE_BOOKSHELF;
                }
                return;
            case APP_PAGE_READER_CATALOG:
            case APP_PAGE_READER_SETTINGS:
                app->page = APP_PAGE_READER;
                return;
            case APP_PAGE_BOOKSHELF:
            case APP_PAGE_WEATHER:
            case APP_PAGE_CALENDAR:
            case APP_PAGE_ENGLISH:
            case APP_PAGE_SETTINGS:
            case APP_PAGE_ABOUT:
            default:
                app->page = APP_PAGE_HOME;
                return;
            case APP_PAGE_WIFI_SETUP:
                if (app->wifi_editor_active) {
                    app->wifi_editor_active = 0;
                } else {
                    app->page = APP_PAGE_SETTINGS;
                }
                return;
        }
    }

    if (button == APP_BUTTON_POWER && app->page != APP_PAGE_HOME && app->page != APP_PAGE_FILE_BROWSER && app->page != APP_PAGE_READER && app->page != APP_PAGE_READER_CATALOG && app->page != APP_PAGE_WIFI_SETUP) {
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
        case APP_PAGE_FILE_BROWSER:
            handle_file_browser(app, button);
            break;
        case APP_PAGE_READER:
            handle_reader(app, button);
            break;
        case APP_PAGE_READER_CATALOG:
            handle_reader_catalog(app, button);
            break;
        case APP_PAGE_READER_SETTINGS:
            handle_reader_settings(app, button);
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
        case APP_PAGE_FILE_BROWSER:
            return "file_browser";
        case APP_PAGE_READER:
            return "reader";
        case APP_PAGE_READER_CATALOG:
            return "reader_catalog";
        case APP_PAGE_READER_SETTINGS:
            return "reader_settings";
        case APP_PAGE_WEATHER:
            return "weather";
        case APP_PAGE_CALENDAR:
            return "calendar";
        case APP_PAGE_ENGLISH:
            return "english";
        case APP_PAGE_SETTINGS:
            return "settings";
        case APP_PAGE_WIFI_SETUP:
            return "wifi_setup";
        case APP_PAGE_ABOUT:
            return "about";
        default:
            return "unknown";
    }
}
