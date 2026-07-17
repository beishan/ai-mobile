#include "app/file_browser.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static file_browser_entry_t entries[FILE_BROWSER_MAX_ENTRIES];
static char root[FILE_BROWSER_PATH_MAX] = "/sdcard";
static char current[FILE_BROWSER_PATH_MAX] = "/sdcard";
static int entry_count;

static int has_txt_extension(const char *name) {
    size_t length;
    if (name == NULL) {
        return 0;
    }
    length = strlen(name);
    if (length < 4) {
        return 0;
    }
    name += length - 4;
    return name[0] == '.' &&
           (name[1] == 't' || name[1] == 'T') &&
           (name[2] == 'x' || name[2] == 'X') &&
           (name[3] == 't' || name[3] == 'T');
}

static int join_path(const char *directory, const char *name, char *path, size_t path_size) {
    int written;
    if (directory == NULL || name == NULL || path == NULL || path_size == 0) {
        return -1;
    }
    written = snprintf(path, path_size, "%s/%s", directory, name);
    return written >= 0 && (size_t)written < path_size ? 0 : -1;
}

static int compare_entries(const void *left, const void *right) {
    const file_browser_entry_t *a = (const file_browser_entry_t *)left;
    const file_browser_entry_t *b = (const file_browser_entry_t *)right;
    if (a->is_directory != b->is_directory) {
        return b->is_directory - a->is_directory;
    }
    return strcmp(a->name, b->name);
}

int file_browser_refresh(void) {
    DIR *directory = opendir(current);
    struct dirent *item;
    entry_count = 0;
    if (directory == NULL) {
        return -1;
    }

    while (entry_count < FILE_BROWSER_MAX_ENTRIES && (item = readdir(directory)) != NULL) {
        char path[FILE_BROWSER_PATH_MAX];
        struct stat info;
        file_browser_entry_t *entry;
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0 ||
            join_path(current, item->d_name, path, sizeof(path)) != 0 ||
            stat(path, &info) != 0) {
            continue;
        }
        if (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode)) {
            continue;
        }
        entry = &entries[entry_count++];
        strncpy(entry->name, item->d_name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';
        entry->size = S_ISREG(info.st_mode) && info.st_size > 0 ? (size_t)info.st_size : 0;
        entry->is_directory = S_ISDIR(info.st_mode);
        entry->is_text = !entry->is_directory && has_txt_extension(entry->name);
    }
    closedir(directory);
    qsort(entries, (size_t)entry_count, sizeof(entries[0]), compare_entries);
    return entry_count;
}

int file_browser_open(const char *root_path) {
    size_t length;
    if (root_path == NULL || root_path[0] == '\0') {
        return -1;
    }
    length = strlen(root_path);
    while (length > 1 && root_path[length - 1] == '/') {
        length--;
    }
    if (length >= sizeof(root)) {
        return -1;
    }
    memcpy(root, root_path, length);
    root[length] = '\0';
    memcpy(current, root, length + 1);
    return file_browser_refresh();
}

int file_browser_count(void) {
    return entry_count;
}

const file_browser_entry_t *file_browser_entry(int index) {
    return index >= 0 && index < entry_count ? &entries[index] : NULL;
}

const char *file_browser_current_path(void) {
    return current;
}

int file_browser_at_root(void) {
    return strcmp(current, root) == 0;
}

int file_browser_entry_path(int index, char *path, size_t path_size) {
    const file_browser_entry_t *entry = file_browser_entry(index);
    return entry != NULL ? join_path(current, entry->name, path, path_size) : -1;
}

int file_browser_enter_directory(int index) {
    const file_browser_entry_t *entry = file_browser_entry(index);
    char previous[FILE_BROWSER_PATH_MAX];
    char next[FILE_BROWSER_PATH_MAX];
    if (entry == NULL || !entry->is_directory ||
        join_path(current, entry->name, next, sizeof(next)) != 0) {
        return -1;
    }
    memcpy(previous, current, sizeof(previous));
    memcpy(current, next, sizeof(current));
    if (file_browser_refresh() < 0) {
        memcpy(current, previous, sizeof(current));
        (void)file_browser_refresh();
        return -1;
    }
    return 0;
}

int file_browser_go_parent(void) {
    char *slash;
    if (file_browser_at_root()) {
        return 0;
    }
    slash = strrchr(current, '/');
    if (slash == NULL || slash < current + strlen(root)) {
        memcpy(current, root, strlen(root) + 1);
    } else {
        *slash = '\0';
    }
    return file_browser_refresh() < 0 ? -1 : 1;
}
