#include "app/app_state.h"

const char *app_page_name(app_page_t page) {
    switch (page) {
        case APP_PAGE_HOME: return "home";
        case APP_PAGE_BOOKSHELF: return "bookshelf";
        case APP_PAGE_FILE_BROWSER: return "file_browser";
        case APP_PAGE_READER: return "reader";
        case APP_PAGE_READER_CATALOG: return "reader_catalog";
        case APP_PAGE_READER_SETTINGS: return "reader_settings";
        case APP_PAGE_READER_FONT: return "reader_font";
        case APP_PAGE_WEATHER: return "weather";
        case APP_PAGE_CALENDAR: return "calendar";
        case APP_PAGE_ENGLISH: return "english";
        case APP_PAGE_SETTINGS: return "settings";
        case APP_PAGE_SYSTEM_FONT: return "system_font";
        case APP_PAGE_WIFI_SETUP: return "wifi_setup";
        case APP_PAGE_ABOUT: return "about";
        default: return "unknown";
    }
}
