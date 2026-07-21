#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include <stddef.h>

#define FILE_BROWSER_MAX_ENTRIES 32
#define FILE_BROWSER_NAME_MAX 128
#define FILE_BROWSER_PATH_MAX 384

typedef struct {
    char name[FILE_BROWSER_NAME_MAX];
    size_t size;
    int is_directory;
    int is_text;
    int is_epub;
} file_browser_entry_t;

int file_browser_open(const char *root_path);
int file_browser_refresh(void);
int file_browser_count(void);
const file_browser_entry_t *file_browser_entry(int index);
const char *file_browser_current_path(void);
int file_browser_at_root(void);
int file_browser_enter_directory(int index);
int file_browser_go_parent(void);
int file_browser_entry_path(int index, char *path, size_t path_size);

#endif
