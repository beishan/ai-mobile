#ifndef APP_PERSISTENCE_H
#define APP_PERSISTENCE_H

#include "app/app_state.h"

#include <stddef.h>

#define APP_PERSISTENCE_VERSION 1
#define APP_PERSISTENCE_TEXT_MAX 256

typedef struct {
    int version;
    int current_book;
    int recent_book;
    int book_current_pages[APP_BOOK_COUNT];
    int book_bookmark_pages[APP_BOOK_COUNT];
    int reader_font_index;
    int font_size_index;
    int line_spacing_index;
    int wifi_connected;
    int weather_city_index;
    int power_saving_enabled;
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
