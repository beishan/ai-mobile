#include "app/reader_library.h"

#include "app/app_state.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define READER_MAX_PAGES 2048
#define READER_PAGE_TEXT_MAX 1024
#define READER_AUTO_PAGE_BYTES 600
#define READER_BOOK_PATH_MAX 512
#define READER_META_TEXT_MAX 160

typedef struct {
    reader_book_t info;
    char path[READER_BOOK_PATH_MAX];
    char title[READER_META_TEXT_MAX];
    char author[24];
    char size_label[24];
    char file_type[8];
    long page_offsets[READER_MAX_PAGES + 1];
    int page_count;
    int loaded;
} reader_book_slot_t;

static reader_book_slot_t slots[APP_BOOK_COUNT];
static char page_text[READER_PAGE_TEXT_MAX];

static int is_utf8_continuation(unsigned char value) {
    return (value & 0xc0u) == 0x80u;
}

static int has_txt_extension(const char *name) {
    size_t len;
    if (name == NULL) {
        return 0;
    }
    len = strlen(name);
    return len > 4 && name[len - 4] == '.' &&
           (name[len - 3] == 't' || name[len - 3] == 'T') &&
           (name[len - 2] == 'x' || name[len - 2] == 'X') &&
           (name[len - 1] == 't' || name[len - 1] == 'T');
}

static const char *path_basename(const char *path) {
    const char *base = path == NULL ? "" : path;
    for (const char *p = base; *p != '\0'; p++) {
        if (*p == '/') {
            base = p + 1;
        }
    }
    return base;
}

static void copy_title(char *dest, size_t dest_size, const char *path) {
    const char *base = path_basename(path);
    size_t len = strlen(base);
    if (len > 4 && has_txt_extension(base)) {
        len -= 4;
    }
    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, base, len);
    dest[len] = '\0';
}

static int page_length(const char *buffer, int available) {
    int last_break = -1;
    int length = 0;
    while (length < available && buffer[length] != '\0') {
        if (buffer[length] == '\f') {
            return length + 1;
        }
        if (buffer[length] == '\n' || buffer[length] == ' ') {
            last_break = length + 1;
        }
        length++;
    }
    if (length == 0) {
        return 0;
    }
    if (length == available && last_break > 32) {
        length = last_break;
    }
    while (length > 0 && is_utf8_continuation((unsigned char)buffer[length])) {
        length--;
    }
    return length > 0 ? length : available;
}

static int index_book_pages(reader_book_slot_t *slot) {
    FILE *file;
    char buffer[READER_AUTO_PAGE_BYTES + 1];
    long offset = 0;
    int count = 0;

    file = fopen(slot->path, "rb");
    if (file == NULL) {
        return -1;
    }
    while (count < READER_MAX_PAGES) {
        size_t read;
        int length;
        slot->page_offsets[count] = offset;
        if (fseek(file, offset, SEEK_SET) != 0) {
            break;
        }
        read = fread(buffer, 1, READER_AUTO_PAGE_BYTES, file);
        buffer[read] = '\0';
        length = page_length(buffer, (int)read);
        if (length <= 0) {
            break;
        }
        offset += length;
        count++;
    }
    fclose(file);
    slot->page_offsets[count] = offset;
    slot->page_count = count;
    return count > 0 ? 0 : -1;
}

static void clear_slot(reader_book_slot_t *slot) {
    memset(slot, 0, sizeof(*slot));
}

static int load_slot(int book_index, const char *path) {
    reader_book_slot_t *slot;
    struct stat info;
    if (book_index < 0 || book_index >= APP_BOOK_COUNT || path == NULL ||
        !has_txt_extension(path) || stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return -1;
    }
    slot = &slots[book_index];
    clear_slot(slot);
    if (snprintf(slot->path, sizeof(slot->path), "%s", path) >= (int)sizeof(slot->path)) {
        return -1;
    }
    copy_title(slot->title, sizeof(slot->title), path);
    snprintf(slot->author, sizeof(slot->author), "SD 卡文件");
    if (info.st_size >= 1024 * 1024) {
        snprintf(slot->size_label, sizeof(slot->size_label), "%.1f MB",
                 (double)info.st_size / (1024.0 * 1024.0));
    } else {
        snprintf(slot->size_label, sizeof(slot->size_label), "%u KB",
                 (unsigned int)((info.st_size + 1023) / 1024));
    }
    snprintf(slot->file_type, sizeof(slot->file_type), "TXT");
    slot->info.title = slot->title;
    slot->info.author = slot->author;
    slot->info.size_label = slot->size_label;
    slot->info.file_type = slot->file_type;
    slot->info.chapter_title = slot->title;
    if (index_book_pages(slot) != 0) {
        clear_slot(slot);
        return -1;
    }
    slot->loaded = 1;
    return 0;
}

static void clear_library(void) {
    memset(slots, 0, sizeof(slots));
}

static void insert_sorted_path(char paths[][READER_BOOK_PATH_MAX], int *count, const char *path) {
    int pos;
    if (*count >= APP_BOOK_COUNT && strcmp(path, paths[APP_BOOK_COUNT - 1]) >= 0) {
        return;
    }
    pos = *count < APP_BOOK_COUNT ? *count : APP_BOOK_COUNT - 1;
    while (pos > 0 && strcmp(path, paths[pos - 1]) < 0) {
        memmove(paths[pos], paths[pos - 1], READER_BOOK_PATH_MAX);
        pos--;
    }
    snprintf(paths[pos], READER_BOOK_PATH_MAX, "%s", path);
    if (*count < APP_BOOK_COUNT) {
        (*count)++;
    }
}

static int collect_txt_paths(const char *directory, char paths[][READER_BOOK_PATH_MAX]) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    if (directory == NULL || (dir = opendir(directory)) == NULL) {
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        char path[READER_BOOK_PATH_MAX];
        if (!has_txt_extension(entry->d_name) ||
            snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) >= (int)sizeof(path)) {
            continue;
        }
        insert_sorted_path(paths, &count, path);
    }
    closedir(dir);
    return count;
}

int reader_library_book_count(void) {
    int count = 0;
    while (count < APP_BOOK_COUNT && slots[count].loaded) {
        count++;
    }
    return count;
}

const reader_book_t *reader_library_book(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count()
               ? &slots[book_index].info : NULL;
}

int reader_library_page_count(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count()
               ? slots[book_index].page_count : 0;
}

const char *reader_library_source_text(int book_index) {
    (void)book_index;
    return "";
}

int reader_library_load_book_file(int book_index, const char *path) {
    return load_slot(book_index, path);
}

int reader_library_open_book_file(int book_index, const char *path) {
    (void)book_index;
    clear_library();
    return load_slot(0, path);
}

int reader_library_load_directory(const char *directory) {
    char paths[APP_BOOK_COUNT][READER_BOOK_PATH_MAX] = {{0}};
    int count = collect_txt_paths(directory, paths);
    int loaded = 0;
    clear_library();
    for (int i = 0; i < count; i++) {
        if (load_slot(loaded, paths[i]) == 0) {
            loaded++;
        }
    }
    return loaded;
}

int reader_library_load_external_books(void) {
    return reader_library_load_directory("assets/books/realbook");
}

const char *reader_library_page_text(int book_index, int page_index) {
    reader_book_slot_t *slot;
    FILE *file = NULL;
    long length;
    size_t read;
    if (book_index < 0 || book_index >= reader_library_book_count()) {
        return "";
    }
    slot = &slots[book_index];
    if (page_index < 0 || page_index >= slot->page_count) {
        return "";
    }
    length = slot->page_offsets[page_index + 1] - slot->page_offsets[page_index];
    if (length <= 0 || length >= READER_PAGE_TEXT_MAX ||
        (file = fopen(slot->path, "rb")) == NULL ||
        fseek(file, slot->page_offsets[page_index], SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return "";
    }
    read = fread(page_text, 1, (size_t)length, file);
    fclose(file);
    while (read > 0 && page_text[read - 1] == '\f') {
        read--;
    }
    page_text[read] = '\0';
    return page_text;
}

const char *reader_library_chapter_title(int book_index, int chapter_index) {
    const reader_book_t *book = reader_library_book(book_index);
    return book != NULL && chapter_index == 0 ? book->chapter_title : "";
}

int reader_library_chapter_page(int book_index, int chapter_index) {
    return reader_library_book(book_index) != NULL && chapter_index == 0 ? 0 : 0;
}

int reader_library_chapter_count(int book_index) {
    return reader_library_book(book_index) != NULL ? 1 : 0;
}
