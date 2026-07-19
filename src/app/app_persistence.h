#ifndef APP_PERSISTENCE_H
#define APP_PERSISTENCE_H

#include "app/app_state.h"

#include <stddef.h>
#include <stdint.h>

#define APP_PERSISTENCE_VERSION 3
#define APP_PERSISTENCE_TEXT_MAX 768

typedef struct {
    int version;
    int current_book;
    int recent_book;
    uint32_t book_ids[APP_BOOK_COUNT];
    int book_current_pages[APP_BOOK_COUNT];
    int book_bookmark_pages[APP_BOOK_COUNT];
    int reader_font_index;
    int system_font_index;
    char system_font_name[64];
    int font_size_index;
    int line_spacing_index;
    int reader_margin_index;
    int reader_indent_enabled;
    int reader_bold_enabled;
    int reader_page_turn_mode;
    int reader_refresh_mode;
    int wifi_connected;
    int weather_city_index;
    int power_saving_enabled;
    int bookshelf_layout;
    int bluetooth_enabled;
    int dictionary_enabled;
} app_persisted_state_t;

void app_persistence_capture(const app_state_t *app, app_persisted_state_t *snapshot);
int app_persistence_apply(app_state_t *app, const app_persisted_state_t *snapshot);
int app_persistence_encode(const app_persisted_state_t *snapshot, char *buffer, size_t buffer_size);
int app_persistence_decode(const char *buffer, app_persisted_state_t *snapshot);
int app_persistence_save_file(const char *path, const app_persisted_state_t *snapshot);
int app_persistence_load_file(const char *path, app_persisted_state_t *snapshot);
int app_persistence_save_app_file(const char *path, const app_state_t *app);
int app_persistence_load_app_file(const char *path, app_state_t *app);
int app_persistence_save_nvs(const char *namespace_name, const char *key, const app_state_t *app);
int app_persistence_load_nvs(const char *namespace_name, const char *key, app_state_t *app);

#endif
