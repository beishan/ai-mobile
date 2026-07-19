#include "app/app_state.h"
#include "app/file_browser.h"
#include "app/reader_library.h"
#include "font/font.h"
#include "gfx/gfx.h"
#include "platform/esp_time_sync.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define HOME_ITEM_COUNT 7
#define READER_MENU_COUNT 3
#define READER_SETTINGS_COUNT 9
#define WEATHER_CITY_COUNT 3
#define WEATHER_SCROLL_STEP 120
#define WEATHER_SCROLL_MAX 190
#define CALENDAR_DAYS_IN_MONTH 30
#define SETTINGS_COUNT 11
#define SETTINGS_SCROLL_MAX 332
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

static int configure_reader_layout(const app_state_t *app) {
    static const int sizes[] = {16, 18, 20, 22, 24};
    static const int spacing[] = {2, 5, 8, 14};
    static const int margins[] = {20, 28, 36, 48};
    int size_index = app->font_size_index;
    int spacing_index = app->line_spacing_index;
    int margin_index = app->reader_margin_index;
    const font_face_t *face;
    if (size_index < 0 || size_index >= 5) size_index = 2;
    if (spacing_index < 0 || spacing_index >= 4) spacing_index = 2;
    if (margin_index < 0 || margin_index >= 4) margin_index = 1;
    face = font_get_face((font_size_t)sizes[size_index]);
    return reader_library_configure_layout_for_book(
        sizes[size_index], app->reader_font_index,
        GFX_WIDTH - margins[margin_index] * 2, 690,
        face->size + spacing[spacing_index] + 12,
        app->reader_indent_enabled, app->reader_bold_enabled,
        app->current_book);
}

static void ensure_reader_book_layout(app_state_t *app, int book_index) {
    int old_total;
    int old_page;
    int new_total;
    if (book_index < 0 || book_index >= reader_library_book_count()) return;
    old_total = app->book_pages[book_index];
    old_page = app->book_current_pages[book_index];
    if (reader_library_ensure_book_layout(book_index) <= 0) return;
    new_total = reader_library_page_count(book_index);
    app->book_pages[book_index] = new_total;
    if (old_total > 0 && new_total > 0) {
        app->book_current_pages[book_index] = old_page * new_total / old_total;
        if (app->book_bookmark_pages[book_index] >= 0) {
            app->book_bookmark_pages[book_index] =
                app->book_bookmark_pages[book_index] * new_total / old_total;
        }
    }
    if (!reader_library_book_layout_complete(book_index)) {
        app->book_repagination_old_pages[book_index] = old_page;
        app->book_repagination_old_totals[book_index] = old_total;
        app->book_repagination_preview_pages[book_index] =
            app->book_current_pages[book_index];
    }
}

void app_sync_reader_library(app_state_t *app) {
    int book_count;
    if (app == NULL) {
        return;
    }
    (void)configure_reader_layout(app);
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
    app->bookshelf_layout = 0;
    app->file_browser_selection = 0;
    app->file_browser_error = file_browser_open("/sdcard") < 0;
    app->reader_library_loading = 0;
    app->reader_library_progress = 0;
    app->current_book = 0;
    app->recent_book = -1;
    for (int i = 0; i < APP_BOOK_COUNT; i++) {
        app->book_pages[i] = 0;
        app->book_current_pages[i] = 0;
        app->book_bookmark_pages[i] = -1;
        app->book_repagination_old_pages[i] = 0;
        app->book_repagination_old_totals[i] = 0;
        app->book_repagination_preview_pages[i] = 0;
    }
    app->reader_page = 0;
    app->reader_background_pagination_active = 0;
    app->reader_background_pagination_progress = 0;
    app->reader_menu_open = 0;
    app->reader_menu_selection = 0;
    app->reader_catalog_open = 0;
    app->reader_catalog_selection = 0;
    app->reader_settings_selection = 0;
    app->reader_settings_editing = 0;
    app->reader_pending_font_size_index = 2;
    app->reader_pending_font_index = 0;
    app->reader_pending_line_spacing_index = 2;
    app->reader_pending_margin_index = 1;
    app->reader_pending_indent_enabled = 1;
    app->reader_pending_bold_enabled = 0;
    app->reader_pending_page_turn_mode = 0;
    app->reader_pending_refresh_mode = 0;
    app->reader_layout_apply_requested = 0;
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
    app->weather_valid = 0;
    app->weather_temperature = 0;
    app->weather_humidity = 0;
    app->weather_text[0] = '\0';
    app->weather_error[0] = '\0';
    app->weather_wind[0] = '\0';
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
    app->bluetooth_enabled = 0;
    app->dictionary_enabled = 1;
    app->time_sync_requested = 0;
    app->update_check_requested = 0;
    app->reader_font_index = 0;
    app->reader_font_selection = 0;
    app->system_font_index = 0;
    app->system_font_selection = 0;
    app->font_size_index = 2;
    app->line_spacing_index = 2;
    app->wifi_connected = 0;
    app->time_synchronized = 0;
    app->battery_valid = 0;
    app->battery_percent = 0;
    app->wifi_setup_selection = 0;
    app->wifi_editor_active = 0;
    app->wifi_edit_char_index = 0;
    app->wifi_config_save_requested = 0;
    app->wifi_setup_stage = APP_WIFI_STAGE_NETWORKS;
    app->wifi_network_count = 0;
    app->wifi_network_selection = 0;
    app->wifi_scan_requested = 0;
    app->wifi_scan_in_progress = 0;
    memset(app->wifi_network_ssids, 0, sizeof(app->wifi_network_ssids));
    memset(app->wifi_network_rssi, 0, sizeof(app->wifi_network_rssi));
    memset(app->wifi_network_secure, 0, sizeof(app->wifi_network_secure));
    app->wifi_saved_count = 0;
    memset(app->wifi_saved_ssids, 0, sizeof(app->wifi_saved_ssids));
    app->wifi_ip[0] = '\0';
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
        ensure_reader_book_layout(app, app->bookshelf_selection);
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
            if (app->reader_menu_selection == 0) {
                app->reader_menu_open = 0;
                app->reader_catalog_open = 0;
                app->reader_catalog_selection = 0;
                app->page = APP_PAGE_READER_CATALOG;
            } else if (app->reader_menu_selection == 1) {
                app->book_bookmark_pages[app->current_book] = app->reader_page;
                app->bookmark_added = 1;
                app->reader_menu_open = 0;
            } else if (app->reader_menu_selection == 2) {
                app->reader_menu_open = 0;
                app->reader_catalog_open = 0;
                app->page = APP_PAGE_READER_SETTINGS;
                app->reader_settings_selection = 0;
                app->reader_settings_editing = 0;
                app->reader_pending_font_size_index = app->font_size_index;
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

static void apply_reader_layout_change(app_state_t *app, int old_page, int old_total) {
    (void)configure_reader_layout(app);
    for (int i = 0; i < APP_BOOK_COUNT; i++) {
        app->book_pages[i] = reader_library_page_count(i);
    }
    if (old_total > 0 && app->book_pages[app->current_book] > 0) {
        app->reader_page = old_page * app->book_pages[app->current_book] / old_total;
    }
    sync_reader_page(app);
    if (!reader_library_book_layout_complete(app->current_book)) {
        app->book_repagination_old_pages[app->current_book] = old_page;
        app->book_repagination_old_totals[app->current_book] = old_total;
        app->book_repagination_preview_pages[app->current_book] = app->reader_page;
    } else {
        app->book_repagination_old_totals[app->current_book] = 0;
    }
}

void app_finish_background_pagination(app_state_t *app, int book_index) {
    int new_total;
    int page;
    int delta;
    if (app == NULL || book_index < 0 || book_index >= APP_BOOK_COUNT) return;
    new_total = reader_library_page_count(book_index);
    page = app->book_current_pages[book_index];
    if (app->book_repagination_old_totals[book_index] > 0 && new_total > 0) {
        delta = page - app->book_repagination_preview_pages[book_index];
        page = app->book_repagination_old_pages[book_index] * new_total /
               app->book_repagination_old_totals[book_index] + delta;
        if (page < 0) page = 0;
        if (page >= new_total) page = new_total - 1;
    }
    app->book_repagination_old_totals[book_index] = 0;
    app_sync_reader_library(app);
    app->book_current_pages[book_index] = page;
    if (app->current_book == book_index) app->reader_page = page;
}

static void handle_reader_settings(app_state_t *app, app_button_t button) {
    if (app->reader_settings_editing) {
        if (button == APP_BUTTON_UP) {
            switch (app->reader_settings_selection) {
                case 0:
                    if (app->reader_pending_font_size_index < 4) app->reader_pending_font_size_index++;
                    break;
                case 1:
                    if (app->reader_pending_font_index + 1 < font_manager_family_count()) app->reader_pending_font_index++;
                    break;
                case 2:
                    if (app->reader_pending_line_spacing_index < 3) app->reader_pending_line_spacing_index++;
                    break;
                case 3:
                    if (app->reader_pending_margin_index < 3) app->reader_pending_margin_index++;
                    break;
                case 4: app->reader_pending_indent_enabled = 1; break;
                case 5: app->reader_pending_bold_enabled = 1; break;
                case 6:
                    if (app->reader_pending_page_turn_mode < 2) app->reader_pending_page_turn_mode++;
                    break;
                case 7:
                    if (app->reader_pending_refresh_mode < 2) app->reader_pending_refresh_mode++;
                    break;
                default: break;
            }
        } else if (button == APP_BUTTON_DOWN) {
            switch (app->reader_settings_selection) {
                case 0:
                    if (app->reader_pending_font_size_index > 0) app->reader_pending_font_size_index--;
                    break;
                case 1:
                    if (app->reader_pending_font_index > 0) app->reader_pending_font_index--;
                    break;
                case 2:
                    if (app->reader_pending_line_spacing_index > 0) app->reader_pending_line_spacing_index--;
                    break;
                case 3:
                    if (app->reader_pending_margin_index > 0) app->reader_pending_margin_index--;
                    break;
                case 4: app->reader_pending_indent_enabled = 0; break;
                case 5: app->reader_pending_bold_enabled = 0; break;
                case 6:
                    if (app->reader_pending_page_turn_mode > 0) app->reader_pending_page_turn_mode--;
                    break;
                case 7:
                    if (app->reader_pending_refresh_mode > 0) app->reader_pending_refresh_mode--;
                    break;
                default: break;
            }
        } else if (button == APP_BUTTON_BACK) {
            app->reader_settings_editing = 0;
            switch (app->reader_settings_selection) {
                case 0:
                    if (app->font_size_index != app->reader_pending_font_size_index) app->reader_layout_apply_requested = 1;
                    app->font_size_index = app->reader_pending_font_size_index;
                    break;
                case 1:
                    if (app->reader_font_index != app->reader_pending_font_index) app->reader_layout_apply_requested = 1;
                    app->reader_font_index = app->reader_pending_font_index;
                    break;
                case 2:
                    if (app->line_spacing_index != app->reader_pending_line_spacing_index) app->reader_layout_apply_requested = 1;
                    app->line_spacing_index = app->reader_pending_line_spacing_index;
                    break;
                case 3:
                    if (app->reader_margin_index != app->reader_pending_margin_index) app->reader_layout_apply_requested = 1;
                    app->reader_margin_index = app->reader_pending_margin_index;
                    break;
                case 4:
                    if (app->reader_indent_enabled != app->reader_pending_indent_enabled) app->reader_layout_apply_requested = 1;
                    app->reader_indent_enabled = app->reader_pending_indent_enabled;
                    break;
                case 5:
                    if (app->reader_bold_enabled != app->reader_pending_bold_enabled) app->reader_layout_apply_requested = 1;
                    app->reader_bold_enabled = app->reader_pending_bold_enabled;
                    break;
                case 6: app->reader_page_turn_mode = app->reader_pending_page_turn_mode; break;
                case 7: app->reader_refresh_mode = app->reader_pending_refresh_mode; break;
                default: break;
            }
        } else if (button == APP_BUTTON_POWER) {
            app->reader_settings_editing = 0;
            app->page = APP_PAGE_READER;
        }
        return;
    }

    if (button == APP_BUTTON_UP) {
        app->reader_settings_selection = wrap_index(app->reader_settings_selection - 1, READER_SETTINGS_COUNT);
    } else if (button == APP_BUTTON_DOWN) {
        app->reader_settings_selection = wrap_index(app->reader_settings_selection + 1, READER_SETTINGS_COUNT);
    } else if (button == APP_BUTTON_POWER) {
        app->page = APP_PAGE_READER;
    } else if (button == APP_BUTTON_HOME) {
        if (app->reader_settings_selection == 1) {
            app->reader_font_selection = app->reader_font_index;
            app->reader_settings_editing = 0;
            app->page = APP_PAGE_READER_FONT;
        } else if (app->reader_settings_selection < 8) {
            app->reader_pending_font_size_index = app->font_size_index;
            app->reader_pending_font_index = app->reader_font_index;
            app->reader_pending_line_spacing_index = app->line_spacing_index;
            app->reader_pending_margin_index = app->reader_margin_index;
            app->reader_pending_indent_enabled = app->reader_indent_enabled;
            app->reader_pending_bold_enabled = app->reader_bold_enabled;
            app->reader_pending_page_turn_mode = app->reader_page_turn_mode;
            app->reader_pending_refresh_mode = app->reader_refresh_mode;
            app->reader_settings_editing = 1;
        } else {
            reset_reader_settings(app);
            app->reader_layout_apply_requested = 1;
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
                app->settings_selection = wrap_index(app->settings_selection - 1, SETTINGS_COUNT);
            } else if (button == APP_BUTTON_DOWN) {
                app->settings_selection = wrap_index(app->settings_selection + 1, SETTINGS_COUNT);
            } else if (button == APP_BUTTON_HOME) {
                switch (app->settings_selection) {
                    case 0:
                        app->page = APP_PAGE_WIFI_SETUP;
                        app->wifi_setup_selection = 0;
                        app->wifi_editor_active = 0;
                        app->wifi_edit_char_index = 0;
                        app->wifi_setup_stage = APP_WIFI_STAGE_NETWORKS;
                        app->wifi_scan_requested = 0;
                        break;
                    case 1:
                        app->bluetooth_enabled = !app->bluetooth_enabled;
                        break;
                    case 2:
                        app->weather_city_index = wrap_index(app->weather_city_index + 1, WEATHER_CITY_COUNT);
                        break;
                    case 3:
                        /* NTP runs on boot after Wi-Fi setup; expose a user-visible request state. */
                        app->time_sync_requested = 1;
                        break;
                    case 4:
                        app->power_saving_enabled = !app->power_saving_enabled;
                        break;
                    case 5:
                        app->file_browser_selection = 0;
                        app->file_browser_error = file_browser_open("/sdcard") < 0;
                        app->page = APP_PAGE_FILE_BROWSER;
                        break;
                    case 6:
                        app->dictionary_enabled = !app->dictionary_enabled;
                        break;
                    case 7:
                        app->page = APP_PAGE_ABOUT;
                        break;
                    case 8:
                        app->update_check_requested = !app->update_check_requested;
                        break;
                    case 9:
                        app->system_font_selection = app->system_font_index;
                        app->page = APP_PAGE_SYSTEM_FONT;
                        break;
                    case 10:
                        app->bookshelf_layout = wrap_index(app->bookshelf_layout + 1, 2);
                        break;
                    default:
                        break;
                }
            }
            {
                static const int row_y[SETTINGS_COUNT] = {184, 240, 364, 420, 544, 600, 724, 854, 910, 966, 1022};
                int selected_y = row_y[app->settings_selection];
                if (selected_y - app->settings_scroll > 690) {
                    app->settings_scroll = selected_y - 690;
                } else if (selected_y - app->settings_scroll < 150) {
                    app->settings_scroll = selected_y - 150;
                }
                if (app->settings_scroll < 0) {
                    app->settings_scroll = 0;
                } else if (app->settings_scroll > SETTINGS_SCROLL_MAX) {
                    app->settings_scroll = SETTINGS_SCROLL_MAX;
                }
            }
            break;
        case APP_PAGE_SYSTEM_FONT:
            if (button == APP_BUTTON_UP) {
                app->system_font_selection = wrap_index(app->system_font_selection - 1,
                                                        font_manager_family_count());
            } else if (button == APP_BUTTON_DOWN) {
                app->system_font_selection = wrap_index(app->system_font_selection + 1,
                                                        font_manager_family_count());
            } else if (button == APP_BUTTON_HOME) {
                app->system_font_index = app->system_font_selection;
                font_manager_set_system_family(app->system_font_index);
                app->page = APP_PAGE_SETTINGS;
            }
            break;
        case APP_PAGE_READER_FONT:
            if (button == APP_BUTTON_UP) {
                app->reader_font_selection = wrap_index(app->reader_font_selection - 1,
                                                        font_manager_family_count());
            } else if (button == APP_BUTTON_DOWN) {
                app->reader_font_selection = wrap_index(app->reader_font_selection + 1,
                                                        font_manager_family_count());
            } else if (button == APP_BUTTON_HOME) {
                if (app->reader_font_index != app->reader_font_selection) {
                    app->reader_layout_apply_requested = 1;
                }
                app->reader_font_index = app->reader_font_selection;
                app->reader_pending_font_index = app->reader_font_index;
                app->page = APP_PAGE_READER_SETTINGS;
            }
            break;
        case APP_PAGE_WIFI_SETUP:
            if (app->wifi_setup_stage == APP_WIFI_STAGE_KEYBOARD) {
                int length = (int)strlen(app->wifi_password);
                int char_count = (int)strlen(wifi_edit_characters);
                if (button == APP_BUTTON_UP) {
                    app->wifi_edit_char_index = wrap_index(app->wifi_edit_char_index - 1, char_count);
                } else if (button == APP_BUTTON_DOWN) {
                    app->wifi_edit_char_index = wrap_index(app->wifi_edit_char_index + 1, char_count);
                } else if (button == APP_BUTTON_HOME && length < 64) {
                    app->wifi_password[length] = wifi_edit_characters[app->wifi_edit_char_index];
                    app->wifi_password[length + 1] = '\0';
                } else if (button == APP_BUTTON_POWER && length > 0) {
                    app->wifi_password[length - 1] = '\0';
                }
            } else if (app->wifi_setup_stage == APP_WIFI_STAGE_CONFIRM) {
                if (button == APP_BUTTON_UP || button == APP_BUTTON_DOWN) {
                    app->wifi_setup_selection = 1 - app->wifi_setup_selection;
                } else if (button == APP_BUTTON_HOME && app->wifi_setup_selection == 0) {
                    app->wifi_setup_stage = APP_WIFI_STAGE_KEYBOARD;
                    app->wifi_editor_active = 1;
                } else if (button == APP_BUTTON_HOME && app->wifi_ssid[0] != '\0') {
                    app->wifi_config_save_requested = 1;
                }
            } else if (!app->wifi_scan_in_progress) {
                int network_count = app->wifi_saved_count + app->wifi_network_count;
                if (button == APP_BUTTON_UP && network_count > 0) {
                    app->wifi_network_selection = wrap_index(app->wifi_network_selection - 1,
                                                             network_count);
                } else if (button == APP_BUTTON_DOWN && network_count > 0) {
                    app->wifi_network_selection = wrap_index(app->wifi_network_selection + 1,
                                                             network_count);
                } else if (button == APP_BUTTON_HOME && network_count > 0) {
                    if (app->wifi_network_selection < app->wifi_saved_count) {
                        snprintf(app->wifi_ssid, sizeof(app->wifi_ssid), "%s",
                                 app->wifi_saved_ssids[app->wifi_network_selection]);
                        if (esp_time_sync_load_saved_password(app->wifi_ssid,
                                                              app->wifi_password,
                                                              sizeof(app->wifi_password)) != 0) {
                            app->wifi_password[0] = '\0';
                        }
                        app->wifi_setup_stage = APP_WIFI_STAGE_CONFIRM;
                        app->wifi_setup_selection = 1;
                        app->wifi_editor_active = 0;
                    } else {
                        int scanned = app->wifi_network_selection - app->wifi_saved_count;
                        snprintf(app->wifi_ssid, sizeof(app->wifi_ssid), "%s",
                                 app->wifi_network_ssids[scanned]);
                        app->wifi_password[0] = '\0';
                        app->wifi_setup_stage = APP_WIFI_STAGE_KEYBOARD;
                        app->wifi_editor_active = 1;
                        app->wifi_edit_char_index = 0;
                    }
                } else if (button == APP_BUTTON_POWER) {
                    app->wifi_scan_requested = 1;
                }
            }
            break;
        case APP_PAGE_READER_SETTINGS:
            handle_reader_settings(app, button);
            break;
        default:
            break;
    }
}

int app_apply_pending_reader_layout(app_state_t *app) {
    int old_page;
    int old_total;
    if (app == NULL || !app->reader_layout_apply_requested ||
        app->page == APP_PAGE_READER_SETTINGS) {
        return 0;
    }
    app->reader_layout_apply_requested = 0;
    old_page = app->reader_page;
    old_total = app->book_pages[app->current_book];
    apply_reader_layout_change(app, old_page, old_total);
    return 1;
}

void app_handle_button(app_state_t *app, app_button_t button) {
    if (app == NULL) {
        return;
    }

    if (button == APP_BUTTON_BACK) {
        if (app->page == APP_PAGE_READER_SETTINGS && app->reader_settings_editing) {
            handle_reader_settings(app, button);
            return;
        }
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
            case APP_PAGE_READER_FONT:
                app->page = APP_PAGE_READER_SETTINGS;
                return;
            case APP_PAGE_BOOKSHELF:
            case APP_PAGE_WEATHER:
            case APP_PAGE_CALENDAR:
            case APP_PAGE_ENGLISH:
            case APP_PAGE_SETTINGS:
            case APP_PAGE_SYSTEM_FONT:
            case APP_PAGE_ABOUT:
            default:
                app->page = app->page == APP_PAGE_SYSTEM_FONT ? APP_PAGE_SETTINGS : APP_PAGE_HOME;
                return;
            case APP_PAGE_WIFI_SETUP:
                if (app->wifi_setup_stage == APP_WIFI_STAGE_KEYBOARD) {
                    app->wifi_editor_active = 0;
                    app->wifi_setup_stage = APP_WIFI_STAGE_CONFIRM;
                    app->wifi_setup_selection = 1;
                } else if (app->wifi_setup_stage == APP_WIFI_STAGE_CONFIRM) {
                    app->wifi_setup_stage = APP_WIFI_STAGE_NETWORKS;
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
        case APP_PAGE_READER_FONT:
            return "reader_font";
        case APP_PAGE_WEATHER:
            return "weather";
        case APP_PAGE_CALENDAR:
            return "calendar";
        case APP_PAGE_ENGLISH:
            return "english";
        case APP_PAGE_SETTINGS:
            return "settings";
        case APP_PAGE_SYSTEM_FONT:
            return "system_font";
        case APP_PAGE_WIFI_SETUP:
            return "wifi_setup";
        case APP_PAGE_ABOUT:
            return "about";
        default:
            return "unknown";
    }
}
