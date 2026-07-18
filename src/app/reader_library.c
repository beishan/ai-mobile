#include "app/reader_library.h"

#include "app/app_state.h"
#include "font/font.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define READER_MAX_PAGES 2048
#define READER_MAX_CHAPTERS 128
#define READER_PAGE_TEXT_MAX 4096
#define READER_BOOK_PATH_MAX 512
#define READER_META_TEXT_MAX 160
#define READER_CHAPTER_TITLE_MAX 96
#define READER_CACHE_VERSION 1
#define READER_CACHE_FILE_NAME ".ai_mobile_index.bin"
#define READER_CACHE_TEMP_NAME ".ai_mobile_index.tmp"
#define READER_CACHE_BACKUP_NAME ".ai_mobile_index.bak"

typedef struct {
    reader_book_t info;
    char path[READER_BOOK_PATH_MAX];
    char title[READER_META_TEXT_MAX];
    char author[24];
    char size_label[24];
    char file_type[8];
    long page_offsets[READER_MAX_PAGES + 1];
    char chapter_titles[READER_MAX_CHAPTERS][READER_CHAPTER_TITLE_MAX];
    int chapter_pages[READER_MAX_CHAPTERS];
    uint32_t id;
    int page_count;
    int chapter_count;
    int truncated;
    int loaded;
} reader_book_slot_t;

typedef struct {
    int font_size;
    int font_family;
    int content_width;
    int content_height;
    int line_height;
    int indent_enabled;
    int bold_enabled;
} reader_layout_t;

typedef struct {
    char magic[8];
    uint32_t version;
    reader_layout_t layout;
    uint32_t entry_count;
    uint32_t checksum;
} reader_cache_header_t;

typedef struct {
    char path[READER_BOOK_PATH_MAX];
    int64_t file_size;
    int64_t modified_time;
    uint32_t id;
    int32_t page_count;
    int32_t chapter_count;
    int32_t truncated;
    uint32_t page_offsets[READER_MAX_PAGES + 1];
    char chapter_titles[READER_MAX_CHAPTERS][READER_CHAPTER_TITLE_MAX];
    int32_t chapter_pages[READER_MAX_CHAPTERS];
} reader_cache_entry_t;

static reader_book_slot_t slots[APP_BOOK_COUNT];
static char page_text[READER_PAGE_TEXT_MAX];
static reader_layout_t layout = {20, 0, 424, 690, 36, 1, 0};
static char cache_directory[READER_BOOK_PATH_MAX];

static int has_txt_extension(const char *name) {
    size_t len;
    if (name == NULL) return 0;
    len = strlen(name);
    return len > 4 && name[len - 4] == '.' &&
           (name[len - 3] == 't' || name[len - 3] == 'T') &&
           (name[len - 2] == 'x' || name[len - 2] == 'X') &&
           (name[len - 1] == 't' || name[len - 1] == 'T');
}

static const char *path_basename(const char *path) {
    const char *base = path == NULL ? "" : path;
    for (const char *p = base; *p != '\0'; p++) {
        if (*p == '/') base = p + 1;
    }
    return base;
}

static void copy_title(char *dest, size_t dest_size, const char *path) {
    const char *base = path_basename(path);
    size_t len = strlen(base);
    if (len > 4 && has_txt_extension(base)) len -= 4;
    if (len >= dest_size) len = dest_size - 1;
    memcpy(dest, base, len);
    dest[len] = '\0';
}

static uint32_t hash_book_identity(const char *path, long size) {
    uint32_t hash = 2166136261u;
    const unsigned char *p = (const unsigned char *)path;
    while (*p != '\0') {
        hash ^= *p++;
        hash *= 16777619u;
    }
    for (unsigned int i = 0; i < sizeof(size); i++) {
        hash ^= (uint32_t)((unsigned long)size >> (i * 8)) & 0xffu;
        hash *= 16777619u;
    }
    return hash != 0 ? hash : 1u;
}

static int read_utf8_character(FILE *file, char bytes[5], uint32_t *codepoint) {
    int first = fgetc(file);
    int length = 1;
    if (first == EOF) return 0;
    bytes[0] = (char)first;
    if ((first & 0xe0) == 0xc0) length = 2;
    else if ((first & 0xf0) == 0xe0) length = 3;
    else if ((first & 0xf8) == 0xf0) length = 4;
    for (int i = 1; i < length; i++) {
        int next = fgetc(file);
        if (next == EOF || (next & 0xc0) != 0x80) {
            if (next != EOF) ungetc(next, file);
            length = i;
            break;
        }
        bytes[i] = (char)next;
    }
    bytes[length] = '\0';
    {
        const unsigned char *cursor = (const unsigned char *)bytes;
        if (!font_decode_utf8(&cursor, codepoint)) *codepoint = (unsigned char)bytes[0];
    }
    return length;
}

static int line_is_chapter(const char *line) {
    const unsigned char *p = (const unsigned char *)line;
    while (*p != '\0' && isspace(*p)) p++;
    if (strncmp((const char *)p, "Chapter ", 8) == 0 ||
        strncmp((const char *)p, "CHAPTER ", 8) == 0) return 1;
    if (strncmp((const char *)p, "序章", strlen("序章")) == 0 ||
        strncmp((const char *)p, "楔子", strlen("楔子")) == 0 ||
        strncmp((const char *)p, "前言", strlen("前言")) == 0 ||
        strncmp((const char *)p, "后记", strlen("后记")) == 0 ||
        strncmp((const char *)p, "尾声", strlen("尾声")) == 0) return 1;
    if (strncmp((const char *)p, "第", strlen("第")) == 0 &&
        (strstr((const char *)p, "章") != NULL || strstr((const char *)p, "节") != NULL ||
         strstr((const char *)p, "卷") != NULL || strstr((const char *)p, "部") != NULL)) return 1;
    return 0;
}

static void add_chapter(reader_book_slot_t *slot, const char *line, int page) {
    size_t len;
    if (!line_is_chapter(line) || slot->chapter_count >= READER_MAX_CHAPTERS) return;
    if (slot->chapter_count == 0 && page > 0) {
        snprintf(slot->chapter_titles[0], READER_CHAPTER_TITLE_MAX, "正文");
        slot->chapter_pages[0] = 0;
        slot->chapter_count = 1;
    }
    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' ||
                       line[len - 1] == ' ' || line[len - 1] == '\t')) len--;
    if (len >= READER_CHAPTER_TITLE_MAX) len = READER_CHAPTER_TITLE_MAX - 1;
    memcpy(slot->chapter_titles[slot->chapter_count], line, len);
    slot->chapter_titles[slot->chapter_count][len] = '\0';
    slot->chapter_pages[slot->chapter_count] = page;
    slot->chapter_count++;
}

static int index_book(reader_book_slot_t *slot) {
    FILE *file = fopen(slot->path, "rb");
    struct stat info;
    char line[READER_CHAPTER_TITLE_MAX] = {0};
    int line_len = 0;
    int line_page = 0;
    int page = 0;
    int line_number = 0;
    int line_width = layout.indent_enabled ? layout.font_size * 2 : 0;
    int max_lines = layout.line_height > 0 ? layout.content_height / layout.line_height : 1;
    long page_start = 0;
    long byte_offset = 0;
    if (file == NULL || stat(slot->path, &info) != 0) {
        if (file != NULL) fclose(file);
        return -1;
    }
    if (max_lines < 1) max_lines = 1;
    {
        unsigned char bom[3];
        size_t read = fread(bom, 1, sizeof(bom), file);
        if (read == 3 && bom[0] == 0xef && bom[1] == 0xbb && bom[2] == 0xbf) {
            page_start = 3;
            byte_offset = 3;
        } else {
            fseek(file, 0, SEEK_SET);
        }
    }
    slot->page_offsets[0] = page_start;
    while (page < READER_MAX_PAGES) {
        long character_offset = byte_offset;
        char bytes[5];
        uint32_t cp;
        int byte_count = read_utf8_character(file, bytes, &cp);
        int advance;
        if (byte_count == 0) break;
        byte_offset += byte_count;
        if (cp == '\f') {
            add_chapter(slot, line, line_page);
            line_len = 0;
            page++;
            slot->page_offsets[page] = byte_offset;
            page_start = byte_offset;
            line_number = 0;
            line_width = layout.indent_enabled ? layout.font_size * 2 : 0;
            line_page = page;
            continue;
        }
        if (cp == '\r') continue;
        if (cp == '\n') {
            line[line_len] = '\0';
            add_chapter(slot, line, line_page);
            line_len = 0;
            line_number++;
            line_width = layout.indent_enabled ? layout.font_size * 2 : 0;
            if (line_number >= max_lines) {
                page++;
                slot->page_offsets[page] = byte_offset;
                page_start = byte_offset;
                line_number = 0;
            }
            line_page = page;
            continue;
        }
        advance = font_measure_text_family(layout.font_size, layout.font_family, bytes);
        if (layout.bold_enabled) advance++;
        if (line_width > 0 && line_width + advance > layout.content_width) {
            line_number++;
            line_width = 0;
            if (line_number >= max_lines) {
                page++;
                slot->page_offsets[page] = character_offset;
                page_start = character_offset;
                line_number = 0;
                if (fseek(file, character_offset, SEEK_SET) != 0) break;
                byte_offset = character_offset;
                continue;
            }
        }
        line_width += advance;
        if (line_len + byte_count < (int)sizeof(line)) {
            memcpy(line + line_len, bytes, (size_t)byte_count);
            line_len += byte_count;
        }
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        add_chapter(slot, line, line_page);
    }
    {
        long end = byte_offset;
        slot->truncated = end < info.st_size;
        if (end > page_start && page < READER_MAX_PAGES) {
            page++;
            slot->page_offsets[page] = end;
        }
    }
    fclose(file);
    slot->page_count = page;
    if (slot->chapter_count == 0) {
        snprintf(slot->chapter_titles[0], READER_CHAPTER_TITLE_MAX, "%.*s",
                 READER_CHAPTER_TITLE_MAX - 1, slot->title);
        slot->chapter_pages[0] = 0;
        slot->chapter_count = 1;
    }
    return page > 0 ? 0 : -1;
}

static void clear_slot(reader_book_slot_t *slot) { memset(slot, 0, sizeof(*slot)); }

static int load_slot(int book_index, const char *path) {
    reader_book_slot_t *slot;
    struct stat info;
    if (book_index < 0 || book_index >= APP_BOOK_COUNT || path == NULL ||
        !has_txt_extension(path) || stat(path, &info) != 0 || !S_ISREG(info.st_mode)) return -1;
    slot = &slots[book_index];
    clear_slot(slot);
    if (snprintf(slot->path, sizeof(slot->path), "%s", path) >= (int)sizeof(slot->path)) return -1;
    copy_title(slot->title, sizeof(slot->title), path);
    snprintf(slot->author, sizeof(slot->author), "SD 卡文件");
    if (info.st_size >= 1024 * 1024) snprintf(slot->size_label, sizeof(slot->size_label), "%.1f MB", (double)info.st_size / (1024.0 * 1024.0));
    else snprintf(slot->size_label, sizeof(slot->size_label), "%u KB", (unsigned int)((info.st_size + 1023) / 1024));
    snprintf(slot->file_type, sizeof(slot->file_type), "TXT");
    slot->id = hash_book_identity(slot->path, (long)info.st_size);
    slot->info.title = slot->title;
    slot->info.author = slot->author;
    slot->info.size_label = slot->size_label;
    slot->info.file_type = slot->file_type;
    slot->info.chapter_title = slot->title;
    if (index_book(slot) != 0) { clear_slot(slot); return -1; }
    slot->loaded = 1;
    return 0;
}

static int cache_path(char *dest, size_t dest_size, const char *directory,
                      const char *filename) {
    return directory != NULL && filename != NULL &&
           snprintf(dest, dest_size, "%s/%s", directory, filename) < (int)dest_size
               ? 0 : -1;
}

static int cache_is_enabled_for(const char *directory) {
    return directory != NULL &&
           (strcmp(directory, "/sdcard") == 0 || strncmp(directory, "/sdcard/", 8) == 0);
}

static int cache_header_is_valid(const reader_cache_header_t *header) {
    static const char magic[8] = "AIMIDX1";
    return header != NULL && memcmp(header->magic, magic, sizeof(magic)) == 0 &&
           header->version == READER_CACHE_VERSION &&
           header->entry_count <= APP_BOOK_COUNT &&
           memcmp(&header->layout, &layout, sizeof(layout)) == 0;
}

static uint32_t cache_checksum_update(uint32_t checksum, const void *data, size_t size) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < size; i++) {
        checksum ^= bytes[i];
        checksum *= 16777619u;
    }
    return checksum;
}

static reader_cache_entry_t *load_cache_entries(const char *directory, int *entry_count) {
    char path[READER_BOOK_PATH_MAX + 32];
    reader_cache_header_t header;
    reader_cache_entry_t *entries;
    FILE *file;
    if (entry_count == NULL || cache_path(path, sizeof(path), directory,
                                          READER_CACHE_FILE_NAME) != 0) return NULL;
    *entry_count = 0;
    file = fopen(path, "rb");
    if (file == NULL) return NULL;
    if (fread(&header, sizeof(header), 1, file) != 1 || !cache_header_is_valid(&header)) {
        fclose(file);
        return NULL;
    }
    entries = calloc(header.entry_count, sizeof(*entries));
    if (entries == NULL) {
        fclose(file);
        return NULL;
    }
    if (header.entry_count > 0 &&
        fread(entries, sizeof(*entries), header.entry_count, file) != header.entry_count) {
        free(entries);
        fclose(file);
        return NULL;
    }
    fclose(file);
    if (cache_checksum_update(2166136261u, entries,
                              header.entry_count * sizeof(*entries)) != header.checksum) {
        free(entries);
        return NULL;
    }
    *entry_count = (int)header.entry_count;
    return entries;
}

static int restore_cached_slot(int book_index, const char *path,
                               const reader_cache_entry_t *entries, int entry_count) {
    struct stat info;
    reader_book_slot_t *slot;
    const reader_cache_entry_t *entry = NULL;
    if (book_index < 0 || book_index >= APP_BOOK_COUNT || path == NULL ||
        entries == NULL || stat(path, &info) != 0) return -1;
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].path[READER_BOOK_PATH_MAX - 1] == '\0' &&
            strcmp(entries[i].path, path) == 0) {
            entry = &entries[i];
            break;
        }
    }
    if (entry == NULL || entry->file_size != (int64_t)info.st_size ||
        entry->modified_time != (int64_t)info.st_mtime ||
        entry->page_count <= 0 || entry->page_count > READER_MAX_PAGES ||
        entry->chapter_count <= 0 || entry->chapter_count > READER_MAX_CHAPTERS) return -1;
    for (int i = 0; i <= entry->page_count; i++) {
        if (entry->page_offsets[i] > (uint64_t)info.st_size ||
            (i > 0 && entry->page_offsets[i] < entry->page_offsets[i - 1])) return -1;
    }
    slot = &slots[book_index];
    clear_slot(slot);
    snprintf(slot->path, sizeof(slot->path), "%s", path);
    copy_title(slot->title, sizeof(slot->title), path);
    snprintf(slot->author, sizeof(slot->author), "SD card file");
    if (info.st_size >= 1024 * 1024) {
        snprintf(slot->size_label, sizeof(slot->size_label), "%.1f MB",
                 (double)info.st_size / (1024.0 * 1024.0));
    } else {
        snprintf(slot->size_label, sizeof(slot->size_label), "%u KB",
                 (unsigned int)((info.st_size + 1023) / 1024));
    }
    snprintf(slot->file_type, sizeof(slot->file_type), "TXT");
    slot->id = entry->id;
    slot->page_count = entry->page_count;
    slot->chapter_count = entry->chapter_count;
    slot->truncated = entry->truncated;
    for (int i = 0; i <= slot->page_count; i++) {
        slot->page_offsets[i] = (long)entry->page_offsets[i];
    }
    memcpy(slot->chapter_titles, entry->chapter_titles, sizeof(slot->chapter_titles));
    memcpy(slot->chapter_pages, entry->chapter_pages, sizeof(slot->chapter_pages));
    slot->info.title = slot->title;
    slot->info.author = slot->author;
    slot->info.size_label = slot->size_label;
    slot->info.file_type = slot->file_type;
    slot->info.chapter_title = slot->chapter_titles[0];
    slot->loaded = 1;
    return 0;
}

static int save_cache(const char *directory) {
    static const char magic[8] = "AIMIDX1";
    char path[READER_BOOK_PATH_MAX + 32];
    char temp_path[READER_BOOK_PATH_MAX + 32];
    char backup_path[READER_BOOK_PATH_MAX + 32];
    reader_cache_header_t header = {{0}, READER_CACHE_VERSION, {0}, 0, 2166136261u};
    reader_cache_entry_t *entry;
    FILE *file;
    int book_count = reader_library_book_count();
    if (!cache_is_enabled_for(directory) ||
        cache_path(path, sizeof(path), directory, READER_CACHE_FILE_NAME) != 0 ||
        cache_path(temp_path, sizeof(temp_path), directory, READER_CACHE_TEMP_NAME) != 0 ||
        cache_path(backup_path, sizeof(backup_path), directory, READER_CACHE_BACKUP_NAME) != 0) return -1;
    memcpy(header.magic, magic, sizeof(magic));
    header.layout = layout;
    header.entry_count = (uint32_t)book_count;
    entry = calloc(1, sizeof(*entry));
    if (entry == NULL) return -1;
    file = fopen(temp_path, "wb");
    if (file == NULL) {
        free(entry);
        return -1;
    }
    if (fwrite(&header, sizeof(header), 1, file) != 1) goto fail;
    for (int i = 0; i < book_count; i++) {
        struct stat info;
        reader_book_slot_t *slot = &slots[i];
        memset(entry, 0, sizeof(*entry));
        if (stat(slot->path, &info) != 0) goto fail;
        snprintf(entry->path, sizeof(entry->path), "%s", slot->path);
        entry->file_size = (int64_t)info.st_size;
        entry->modified_time = (int64_t)info.st_mtime;
        entry->id = slot->id;
        entry->page_count = slot->page_count;
        entry->chapter_count = slot->chapter_count;
        entry->truncated = slot->truncated;
        for (int page = 0; page <= slot->page_count; page++) {
            entry->page_offsets[page] = (uint32_t)slot->page_offsets[page];
        }
        memcpy(entry->chapter_titles, slot->chapter_titles, sizeof(entry->chapter_titles));
        memcpy(entry->chapter_pages, slot->chapter_pages, sizeof(entry->chapter_pages));
        header.checksum = cache_checksum_update(header.checksum, entry, sizeof(*entry));
        if (fwrite(entry, sizeof(*entry), 1, file) != 1) goto fail;
    }
    if (fseek(file, 0, SEEK_SET) != 0 || fwrite(&header, sizeof(header), 1, file) != 1) goto fail;
    if (fclose(file) != 0) {
        free(entry);
        remove(temp_path);
        return -1;
    }
    free(entry);
    remove(backup_path);
    if (rename(path, backup_path) != 0) remove(path);
    if (rename(temp_path, path) != 0) {
        rename(backup_path, path);
        remove(temp_path);
        return -1;
    }
    remove(backup_path);
    return 0;

fail:
    fclose(file);
    free(entry);
    remove(temp_path);
    return -1;
}

static void clear_library(void) { memset(slots, 0, sizeof(slots)); }

static void insert_sorted_path(char paths[][READER_BOOK_PATH_MAX], int *count, const char *path) {
    int pos;
    if (*count >= APP_BOOK_COUNT && strcmp(path, paths[APP_BOOK_COUNT - 1]) >= 0) return;
    pos = *count < APP_BOOK_COUNT ? *count : APP_BOOK_COUNT - 1;
    while (pos > 0 && strcmp(path, paths[pos - 1]) < 0) {
        memmove(paths[pos], paths[pos - 1], READER_BOOK_PATH_MAX);
        pos--;
    }
    snprintf(paths[pos], READER_BOOK_PATH_MAX, "%s", path);
    if (*count < APP_BOOK_COUNT) (*count)++;
}

static int collect_txt_paths(const char *directory, char paths[][READER_BOOK_PATH_MAX]) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    if (directory == NULL || (dir = opendir(directory)) == NULL) return 0;
    while ((entry = readdir(dir)) != NULL) {
        char path[READER_BOOK_PATH_MAX];
        if (!has_txt_extension(entry->d_name) ||
            snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) >= (int)sizeof(path)) continue;
        insert_sorted_path(paths, &count, path);
    }
    closedir(dir);
    return count;
}

int reader_library_book_count(void) {
    int count = 0;
    while (count < APP_BOOK_COUNT && slots[count].loaded) count++;
    return count;
}

uint32_t reader_library_book_id(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count() ? slots[book_index].id : 0;
}

const char *reader_library_book_path(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count() ? slots[book_index].path : "";
}

const reader_book_t *reader_library_book(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count() ? &slots[book_index].info : NULL;
}

int reader_library_page_count(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count() ? slots[book_index].page_count : 0;
}

const char *reader_library_source_text(int book_index) { return reader_library_book_path(book_index); }

int reader_library_load_book_file(int book_index, const char *path) { return load_slot(book_index, path); }

int reader_library_open_book_file(int book_index, const char *path) {
    (void)book_index;
    clear_library();
    return load_slot(0, path);
}

int reader_library_load_directory(const char *directory) {
    char paths[APP_BOOK_COUNT][READER_BOOK_PATH_MAX] = {{0}};
    int count = collect_txt_paths(directory, paths);
    int loaded = 0;
    int cache_entry_count = 0;
    reader_cache_entry_t *cache_entries = NULL;
    clear_library();
    cache_directory[0] = '\0';
    if (cache_is_enabled_for(directory)) {
        snprintf(cache_directory, sizeof(cache_directory), "%s", directory);
        cache_entries = load_cache_entries(directory, &cache_entry_count);
    }
    for (int i = 0; i < count; i++) {
        if (restore_cached_slot(loaded, paths[i], cache_entries, cache_entry_count) == 0) {
            printf("reader_library: cache hit %s (%d pages)\n",
                   paths[i], slots[loaded].page_count);
            fflush(stdout);
            loaded++;
            continue;
        }
        printf("reader_library: indexing %s\n", paths[i]);
        fflush(stdout);
        if (load_slot(loaded, paths[i]) == 0) {
            printf("reader_library: indexed %s (%d pages)\n",
                   paths[i], slots[loaded].page_count);
            fflush(stdout);
            loaded++;
        } else {
            printf("reader_library: failed to index %s\n", paths[i]);
            fflush(stdout);
        }
    }
    free(cache_entries);
    if (cache_directory[0] != '\0') {
        if (save_cache(cache_directory) == 0) {
            printf("reader_library: index cache updated\n");
        } else {
            printf("reader_library: failed to update index cache\n");
        }
        fflush(stdout);
    }
    return loaded;
}

int reader_library_load_external_books(void) { return reader_library_load_directory("assets/books/realbook"); }

int reader_library_configure_layout(int font_size, int font_family, int content_width,
                                    int content_height, int line_height,
                                    int indent_enabled, int bold_enabled) {
    reader_layout_t next = {font_size, font_family, content_width, content_height, line_height,
                            indent_enabled ? 1 : 0, bold_enabled ? 1 : 0};
    if (memcmp(&layout, &next, sizeof(layout)) == 0) return 0;
    layout = next;
    for (int i = 0; i < reader_library_book_count(); i++) {
        if (index_book(&slots[i]) != 0) return -1;
    }
    if (cache_directory[0] != '\0' && save_cache(cache_directory) != 0) {
        printf("reader_library: failed to update cache after layout change\n");
        fflush(stdout);
    }
    return 1;
}

int reader_library_is_truncated(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count() ? slots[book_index].truncated : 0;
}

const char *reader_library_page_text(int book_index, int page_index) {
    reader_book_slot_t *slot;
    FILE *file = NULL;
    long length;
    size_t read;
    if (book_index < 0 || book_index >= reader_library_book_count()) return "";
    slot = &slots[book_index];
    if (page_index < 0 || page_index >= slot->page_count) return "";
    length = slot->page_offsets[page_index + 1] - slot->page_offsets[page_index];
    if (length <= 0 || length >= READER_PAGE_TEXT_MAX ||
        (file = fopen(slot->path, "rb")) == NULL ||
        fseek(file, slot->page_offsets[page_index], SEEK_SET) != 0) {
        if (file != NULL) fclose(file);
        return "";
    }
    read = fread(page_text, 1, (size_t)length, file);
    fclose(file);
    while (read > 0 && page_text[read - 1] == '\f') read--;
    page_text[read] = '\0';
    return page_text;
}

const char *reader_library_chapter_title(int book_index, int chapter_index) {
    return book_index >= 0 && book_index < reader_library_book_count() &&
           chapter_index >= 0 && chapter_index < slots[book_index].chapter_count
               ? slots[book_index].chapter_titles[chapter_index] : "";
}

int reader_library_chapter_page(int book_index, int chapter_index) {
    return book_index >= 0 && book_index < reader_library_book_count() &&
           chapter_index >= 0 && chapter_index < slots[book_index].chapter_count
               ? slots[book_index].chapter_pages[chapter_index] : 0;
}

int reader_library_chapter_count(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count() ? slots[book_index].chapter_count : 0;
}
