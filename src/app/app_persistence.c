#include "app/app_persistence.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef ESP_PLATFORM
#include "nvs.h"
#endif

#define APP_PERSISTENCE_CITY_COUNT 3
#define APP_PERSISTENCE_FONT_COUNT 5
#define APP_PERSISTENCE_LINE_SPACING_COUNT 4

static int clamp_int(int value, int min, int max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static int clamp_book_index(int value) {
    return clamp_int(value, 0, APP_BOOK_COUNT - 1);
}

static int clamp_recent_book(int value) {
    if (value < 0) {
        return -1;
    }
    return clamp_book_index(value);
}

static int clamp_page_index(int value, int total_pages) {
    if (total_pages <= 0) {
        return 0;
    }
    return clamp_int(value, 0, total_pages - 1);
}

static int clamp_bookmark_page(int value, int total_pages) {
    if (value < 0 || total_pages <= 0) {
        return -1;
    }
    return clamp_int(value, 0, total_pages - 1);
}

static int normalize_bool(int value) {
    return value > 0 ? 1 : 0;
}

static int ensure_parent_dir(const char *path) {
    const char *slash;
    char dir[256];
    size_t len;
    if (path == NULL) {
        return -1;
    }

    slash = strrchr(path, '/');
    if (slash == NULL) {
        return 0;
    }

    len = (size_t)(slash - path);
    if (len == 0 || len >= sizeof(dir)) {
        return -1;
    }
    memcpy(dir, path, len);
    dir[len] = '\0';

    if (mkdir(dir, 0777) == 0 || errno == EEXIST) {
        return 0;
    }
    return -1;
}

void app_persistence_capture(const app_state_t *app, app_persisted_state_t *snapshot) {
    if (app == NULL || snapshot == NULL) {
        return;
    }

    snapshot->version = APP_PERSISTENCE_VERSION;
    snapshot->current_book = app->current_book;
    snapshot->recent_book = app->recent_book;
    for (int i = 0; i < APP_BOOK_COUNT; i++) {
        snapshot->book_current_pages[i] = app->book_current_pages[i];
        snapshot->book_bookmark_pages[i] = app->book_bookmark_pages[i];
    }
    snapshot->font_size_index = app->font_size_index;
    snapshot->line_spacing_index = app->line_spacing_index;
    snapshot->wifi_connected = app->wifi_connected;
    snapshot->weather_city_index = app->weather_city_index;
    snapshot->power_saving_enabled = app->power_saving_enabled;
}

int app_persistence_apply(app_state_t *app, const app_persisted_state_t *snapshot) {
    if (app == NULL || snapshot == NULL || snapshot->version != APP_PERSISTENCE_VERSION) {
        return -1;
    }

    app->current_book = clamp_book_index(snapshot->current_book);
    app->recent_book = clamp_recent_book(snapshot->recent_book);
    for (int i = 0; i < APP_BOOK_COUNT; i++) {
        app->book_current_pages[i] = clamp_page_index(snapshot->book_current_pages[i], app->book_pages[i]);
        app->book_bookmark_pages[i] = clamp_bookmark_page(snapshot->book_bookmark_pages[i], app->book_pages[i]);
    }
    app->reader_page = app->book_current_pages[app->current_book];
    app->bookshelf_selection = app->current_book;
    app->font_size_index = clamp_int(snapshot->font_size_index, 0, APP_PERSISTENCE_FONT_COUNT - 1);
    app->line_spacing_index = clamp_int(snapshot->line_spacing_index, 0, APP_PERSISTENCE_LINE_SPACING_COUNT - 1);
    app->wifi_connected = normalize_bool(snapshot->wifi_connected);
    app->weather_city_index = clamp_int(snapshot->weather_city_index, 0, APP_PERSISTENCE_CITY_COUNT - 1);
    app->power_saving_enabled = normalize_bool(snapshot->power_saving_enabled);
    return 0;
}

int app_persistence_encode(const app_persisted_state_t *snapshot, char *buffer, size_t buffer_size) {
    int written;
    if (snapshot == NULL || buffer == NULL || buffer_size == 0 || snapshot->version != APP_PERSISTENCE_VERSION) {
        return -1;
    }

    written = snprintf(buffer,
                       buffer_size,
                       "AIPERSIST %d\n"
                       "current=%d\n"
                       "recent=%d\n"
                       "pages=%d,%d,%d\n"
                       "bookmarks=%d,%d,%d\n"
                       "font=%d\n"
                       "spacing=%d\n"
                       "wifi=%d\n"
                       "city=%d\n"
                       "power=%d\n",
                       snapshot->version,
                       snapshot->current_book,
                       snapshot->recent_book,
                       snapshot->book_current_pages[0],
                       snapshot->book_current_pages[1],
                       snapshot->book_current_pages[2],
                       snapshot->book_bookmark_pages[0],
                       snapshot->book_bookmark_pages[1],
                       snapshot->book_bookmark_pages[2],
                       snapshot->font_size_index,
                       snapshot->line_spacing_index,
                       snapshot->wifi_connected,
                       snapshot->weather_city_index,
                       snapshot->power_saving_enabled);
    if (written < 0 || (size_t)written >= buffer_size) {
        if (buffer_size > 0) {
            buffer[0] = '\0';
        }
        return -1;
    }
    return 0;
}

int app_persistence_decode(const char *buffer, app_persisted_state_t *snapshot) {
    app_persisted_state_t parsed;
    int consumed = 0;
    int matched;
    if (buffer == NULL || snapshot == NULL) {
        return -1;
    }

    matched = sscanf(buffer,
                     "AIPERSIST %d\n"
                     "current=%d\n"
                     "recent=%d\n"
                     "pages=%d,%d,%d\n"
                     "bookmarks=%d,%d,%d\n"
                     "font=%d\n"
                     "spacing=%d\n"
                     "wifi=%d\n"
                     "city=%d\n"
                     "power=%d\n%n",
                     &parsed.version,
                     &parsed.current_book,
                     &parsed.recent_book,
                     &parsed.book_current_pages[0],
                     &parsed.book_current_pages[1],
                     &parsed.book_current_pages[2],
                     &parsed.book_bookmark_pages[0],
                     &parsed.book_bookmark_pages[1],
                     &parsed.book_bookmark_pages[2],
                     &parsed.font_size_index,
                     &parsed.line_spacing_index,
                     &parsed.wifi_connected,
                     &parsed.weather_city_index,
                     &parsed.power_saving_enabled,
                     &consumed);
    if (matched != 14 || parsed.version != APP_PERSISTENCE_VERSION) {
        return -1;
    }
    while (buffer[consumed] != '\0') {
        if (!isspace((unsigned char)buffer[consumed])) {
            return -1;
        }
        consumed++;
    }

    *snapshot = parsed;
    return 0;
}

int app_persistence_save_file(const char *path, const app_persisted_state_t *snapshot) {
    char encoded[APP_PERSISTENCE_TEXT_MAX];
    FILE *file;
    size_t length;
    if (path == NULL || snapshot == NULL) {
        return -1;
    }
    if (app_persistence_encode(snapshot, encoded, sizeof(encoded)) != 0) {
        return -1;
    }
    if (ensure_parent_dir(path) != 0) {
        return -1;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    length = strlen(encoded);
    if (fwrite(encoded, 1, length, file) != length) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

int app_persistence_load_file(const char *path, app_persisted_state_t *snapshot) {
    char encoded[APP_PERSISTENCE_TEXT_MAX];
    FILE *file;
    size_t read;
    if (path == NULL || snapshot == NULL) {
        return -1;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }
    read = fread(encoded, 1, sizeof(encoded) - 1, file);
    if (ferror(file)) {
        fclose(file);
        return -1;
    }
    if (!feof(file)) {
        fclose(file);
        return -1;
    }
    fclose(file);
    encoded[read] = '\0';

    return app_persistence_decode(encoded, snapshot);
}

int app_persistence_save_app_file(const char *path, const app_state_t *app) {
    app_persisted_state_t snapshot;
    if (path == NULL || app == NULL) {
        return -1;
    }
    app_persistence_capture(app, &snapshot);
    return app_persistence_save_file(path, &snapshot);
}

int app_persistence_load_app_file(const char *path, app_state_t *app) {
    app_persisted_state_t snapshot;
    if (path == NULL || app == NULL) {
        return -1;
    }
    if (app_persistence_load_file(path, &snapshot) != 0) {
        return -1;
    }
    return app_persistence_apply(app, &snapshot);
}

#ifdef ESP_PLATFORM
int app_persistence_save_nvs(const char *namespace_name, const char *key, const app_state_t *app) {
    app_persisted_state_t snapshot;
    char encoded[APP_PERSISTENCE_TEXT_MAX];
    nvs_handle_t handle;
    esp_err_t err;
    if (namespace_name == NULL || key == NULL || app == NULL) {
        return -1;
    }

    app_persistence_capture(app, &snapshot);
    if (app_persistence_encode(&snapshot, encoded, sizeof(encoded)) != 0) {
        return -1;
    }

    err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return -1;
    }
    err = nvs_set_str(handle, key, encoded);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK ? 0 : -1;
}

int app_persistence_load_nvs(const char *namespace_name, const char *key, app_state_t *app) {
    char encoded[APP_PERSISTENCE_TEXT_MAX];
    app_persisted_state_t snapshot;
    nvs_handle_t handle;
    size_t required = sizeof(encoded);
    esp_err_t err;
    if (namespace_name == NULL || key == NULL || app == NULL) {
        return -1;
    }

    err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return -1;
    }
    err = nvs_get_str(handle, key, encoded, &required);
    nvs_close(handle);
    if (err != ESP_OK || required > sizeof(encoded)) {
        return -1;
    }
    if (app_persistence_decode(encoded, &snapshot) != 0) {
        return -1;
    }
    return app_persistence_apply(app, &snapshot);
}
#else
int app_persistence_save_nvs(const char *namespace_name, const char *key, const app_state_t *app) {
    (void)namespace_name;
    (void)key;
    (void)app;
    return -1;
}

int app_persistence_load_nvs(const char *namespace_name, const char *key, app_state_t *app) {
    (void)namespace_name;
    (void)key;
    (void)app;
    return -1;
}
#endif
