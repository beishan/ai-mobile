#include "app/reader_library.h"

#include "app/app_state.h"
#include "app/epub_reader.h"
#include "font/font.h"
#include "platform/storage_io.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

#define READER_MAX_PAGES 2048
#define READER_MAX_CHAPTERS 128
#define READER_PAGE_TEXT_MAX 4096
#define READER_BOOK_PATH_MAX 512
#define READER_META_TEXT_MAX 160
#define READER_CHAPTER_TITLE_MAX 96
#define READER_CACHE_VERSION 3
#define READER_CACHE_FILE_NAME ".ai_mobile_index.bin"
#define READER_CACHE_TEMP_NAME ".ai_mobile_index.tmp"
#define READER_CACHE_BACKUP_NAME ".ai_mobile_index.bak"
#define READER_INITIAL_READY_PAGES 16
#define READER_PAGE_CACHE_SIZE 3

typedef struct {
    reader_book_t info;
    char path[READER_BOOK_PATH_MAX];
    char content_path[READER_BOOK_PATH_MAX];
    char title[READER_META_TEXT_MAX];
    char author[64];
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
    int layout_valid;
    int layout_complete;
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
    char title[READER_META_TEXT_MAX];
    char author[64];
    char file_type[8];
    int32_t page_count;
    int32_t chapter_count;
    int32_t truncated;
    int32_t layout_valid;
    int32_t layout_complete;
    uint32_t page_offsets[READER_MAX_PAGES + 1];
    char chapter_titles[READER_MAX_CHAPTERS][READER_CHAPTER_TITLE_MAX];
    int32_t chapter_pages[READER_MAX_CHAPTERS];
} reader_cache_entry_t;

static reader_book_slot_t *slots;
static char page_text[READER_PAGE_TEXT_MAX];
static FILE *page_text_stream;
static uint32_t page_text_stream_book_id;
typedef struct {
    uint32_t book_id;
    int page_index;
    unsigned int used_at;
    char text[READER_PAGE_TEXT_MAX];
} reader_page_cache_entry_t;
static reader_page_cache_entry_t page_cache[READER_PAGE_CACHE_SIZE];
static unsigned int page_cache_clock;
static void invalidate_page_cache(void);
static reader_layout_t layout = {20, 0, 424, 690, 36, 1, 0};
static char cache_directory[READER_BOOK_PATH_MAX];
static reader_library_progress_callback_t progress_callback;
static void *progress_context;
static reader_book_slot_t *background_result;
static reader_layout_t background_result_layout;
static uint32_t background_result_book_id;
static int background_result_book_index = -1;
static volatile int background_result_ready;
static volatile int background_progress;

static void *reader_large_calloc(size_t count, size_t size) {
#ifdef ESP_PLATFORM
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return calloc(count, size);
#endif
}

static void *reader_large_malloc(size_t size) {
#ifdef ESP_PLATFORM
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif
}

static void reader_large_free(void *memory) {
#ifdef ESP_PLATFORM
    heap_caps_free(memory);
#else
    free(memory);
#endif
}

static int ensure_slots(void) {
    if (slots != NULL) return 0;
    slots = reader_large_calloc(APP_BOOK_COUNT, sizeof(*slots));
    return slots != NULL ? 0 : -1;
}

void reader_library_set_progress_callback(reader_library_progress_callback_t callback,
                                          void *context) {
    progress_callback = callback;
    progress_context = context;
}

static void report_index_progress(reader_book_slot_t *slot, int percent) {
    int book_index;
    if (progress_callback == NULL || slot == NULL || slots == NULL) return;
    book_index = (int)(slot - slots);
    progress_callback(book_index, percent, progress_context);
}

static int has_txt_extension(const char *name) {
    size_t len;
    if (name == NULL) return 0;
    len = strlen(name);
    return len > 4 && name[len - 4] == '.' &&
           (name[len - 3] == 't' || name[len - 3] == 'T') &&
           (name[len - 2] == 'x' || name[len - 2] == 'X') &&
           (name[len - 1] == 't' || name[len - 1] == 'T');
}

static int has_epub_extension(const char *name) {
    size_t len;
    if (name == NULL) return 0;
    len = strlen(name);
    return len > 5 && name[len - 5] == '.' &&
           (name[len - 4] == 'e' || name[len - 4] == 'E') &&
           (name[len - 3] == 'p' || name[len - 3] == 'P') &&
           (name[len - 2] == 'u' || name[len - 2] == 'U') &&
           (name[len - 1] == 'b' || name[len - 1] == 'B');
}

static int has_book_extension(const char *name) {
    return has_txt_extension(name) || has_epub_extension(name);
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
    else if (len > 5 && has_epub_extension(base)) len -= 5;
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

static int epub_content_path(uint32_t id, char *path, size_t path_size) {
#ifdef ESP_PLATFORM
    const char *root = "/sdcard/.ai_epub_cache";
    (void)mkdir(root, 0777);
#else
    const char *root = "out/.epub_cache";
    (void)mkdir("out", 0777);
    (void)mkdir(root, 0777);
#endif
    return snprintf(path, path_size, "%s/%08x.txt", root, (unsigned int)id) < (int)path_size
               ? 0 : -1;
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

static int index_book_with_layout(reader_book_slot_t *slot,
                                  const reader_layout_t *active_layout,
                                  int page_limit, int report_progress) {
    FILE *file;
    struct stat info;
    char line[READER_CHAPTER_TITLE_MAX] = {0};
    int line_len = 0;
    int line_page = 0;
    int page = 0;
    int line_number = 0;
    int line_width = active_layout->indent_enabled ? active_layout->font_size * 2 : 0;
    int max_lines = active_layout->line_height > 0
                        ? active_layout->content_height / active_layout->line_height : 1;
    long next_progress_offset;
    int next_progress_percent = 20;
    long page_start = 0;
    long byte_offset = 0;
    long next_io_yield = 4096;
    storage_io_lock(STORAGE_IO_BACKGROUND);
    file = fopen(slot->content_path, "rb");
    if (file == NULL || stat(slot->content_path, &info) != 0) {
        if (file != NULL) fclose(file);
        storage_io_unlock();
        return -1;
    }
    slot->page_count = 0;
    slot->chapter_count = 0;
    slot->truncated = 0;
    slot->layout_valid = 0;
    memset(slot->page_offsets, 0, sizeof(slot->page_offsets));
    memset(slot->chapter_titles, 0, sizeof(slot->chapter_titles));
    memset(slot->chapter_pages, 0, sizeof(slot->chapter_pages));
    next_progress_offset = info.st_size > 0 ? info.st_size / 5 : 1;
    if (next_progress_offset < 1) next_progress_offset = 1;
    if (report_progress) {
        storage_io_unlock();
        report_index_progress(slot, 0);
        storage_io_lock(STORAGE_IO_BACKGROUND);
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
    if (page_limit < 1 || page_limit > READER_MAX_PAGES) page_limit = READER_MAX_PAGES;
    while (page < page_limit) {
        long character_offset = byte_offset;
        char bytes[5];
        uint32_t cp;
        int byte_count = read_utf8_character(file, bytes, &cp);
        int advance;
        if (byte_count == 0) break;
        byte_offset += byte_count;
        if (byte_offset >= next_io_yield) {
            storage_io_unlock();
            storage_io_lock(STORAGE_IO_BACKGROUND);
            next_io_yield = byte_offset + 4096;
        }
        if (byte_offset >= next_progress_offset && next_progress_percent < 100) {
            if (report_progress) {
                storage_io_unlock();
                report_index_progress(slot, next_progress_percent);
                storage_io_lock(STORAGE_IO_BACKGROUND);
            } else {
                background_progress = next_progress_percent;
            }
            next_progress_percent += 20;
            next_progress_offset = info.st_size * next_progress_percent / 100;
        }
        if (cp == '\f') {
            add_chapter(slot, line, line_page);
            line_len = 0;
            page++;
            slot->page_offsets[page] = byte_offset;
            page_start = byte_offset;
            line_number = 0;
            line_width = active_layout->indent_enabled ? active_layout->font_size * 2 : 0;
            line_page = page;
            continue;
        }
        if (cp == '\r') continue;
        if (cp == '\n') {
            line[line_len] = '\0';
            add_chapter(slot, line, line_page);
            line_len = 0;
            line_number++;
            line_width = active_layout->indent_enabled ? active_layout->font_size * 2 : 0;
            if (line_number >= max_lines) {
                page++;
                slot->page_offsets[page] = byte_offset;
                page_start = byte_offset;
                line_number = 0;
            }
            line_page = page;
            continue;
        }
        advance = font_measure_text_family(active_layout->font_size,
                                           active_layout->font_family, bytes);
        if (active_layout->bold_enabled) advance++;
        if (line_width > 0 && line_width + advance > active_layout->content_width) {
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
    storage_io_unlock();
    slot->page_count = page;
    if (slot->chapter_count == 0) {
        snprintf(slot->chapter_titles[0], READER_CHAPTER_TITLE_MAX, "%.*s",
                 READER_CHAPTER_TITLE_MAX - 1, slot->title);
        slot->chapter_pages[0] = 0;
        slot->chapter_count = 1;
    }
    slot->layout_valid = page > 0;
    slot->layout_complete = byte_offset >= info.st_size || page >= READER_MAX_PAGES;
    if (report_progress) report_index_progress(slot, 100);
    else background_progress = 100;
    return page > 0 ? 0 : -1;
}

static int index_book(reader_book_slot_t *slot) {
    return index_book_with_layout(slot, &layout, READER_MAX_PAGES, 1);
}

static int index_book_initial(reader_book_slot_t *slot) {
    return index_book_with_layout(slot, &layout, READER_INITIAL_READY_PAGES, 1);
}

static void clear_slot(reader_book_slot_t *slot) { memset(slot, 0, sizeof(*slot)); }

static int load_slot_mode(int book_index, const char *path, int initial_only) {
    reader_book_slot_t *slot;
    struct stat info;
    if (ensure_slots() != 0 || book_index < 0 || book_index >= APP_BOOK_COUNT || path == NULL ||
        !has_book_extension(path) || stat(path, &info) != 0 || !S_ISREG(info.st_mode)) return -1;
    slot = &slots[book_index];
    clear_slot(slot);
    if (snprintf(slot->path, sizeof(slot->path), "%s", path) >= (int)sizeof(slot->path)) return -1;
    slot->id = hash_book_identity(slot->path, (long)info.st_size);
    copy_title(slot->title, sizeof(slot->title), path);
    snprintf(slot->author, sizeof(slot->author), "SD 卡文件");
    if (info.st_size >= 1024 * 1024) snprintf(slot->size_label, sizeof(slot->size_label), "%.1f MB", (double)info.st_size / (1024.0 * 1024.0));
    else snprintf(slot->size_label, sizeof(slot->size_label), "%u KB", (unsigned int)((info.st_size + 1023) / 1024));
    if (has_epub_extension(path)) {
        epub_metadata_t metadata;
        if (epub_content_path(slot->id, slot->content_path, sizeof(slot->content_path)) != 0 ||
            epub_extract_text(path, slot->content_path, &metadata) != 0) {
            clear_slot(slot);
            return -1;
        }
        if (metadata.title[0] != '\0') snprintf(slot->title, sizeof(slot->title), "%s", metadata.title);
        if (metadata.author[0] != '\0') snprintf(slot->author, sizeof(slot->author), "%s", metadata.author);
        snprintf(slot->file_type, sizeof(slot->file_type), "EPUB");
    } else {
        snprintf(slot->content_path, sizeof(slot->content_path), "%s", path);
        snprintf(slot->file_type, sizeof(slot->file_type), "TXT");
    }
    slot->info.title = slot->title;
    slot->info.author = slot->author;
    slot->info.size_label = slot->size_label;
    slot->info.file_type = slot->file_type;
    slot->info.chapter_title = slot->title;
    if ((initial_only ? index_book_initial(slot) : index_book(slot)) != 0) {
        clear_slot(slot);
        return -1;
    }
    slot->loaded = 1;
    slot->layout_valid = 1;
    return 0;
}

static int load_slot(int book_index, const char *path) {
    return load_slot_mode(book_index, path, 0);
}

static int load_slot_initial(int book_index, const char *path) {
    return load_slot_mode(book_index, path, 1);
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
    entries = reader_large_calloc(header.entry_count, sizeof(*entries));
    if (entries == NULL) {
        fclose(file);
        return NULL;
    }
    if (header.entry_count > 0 &&
        fread(entries, sizeof(*entries), header.entry_count, file) != header.entry_count) {
        reader_large_free(entries);
        fclose(file);
        return NULL;
    }
    fclose(file);
    if (cache_checksum_update(2166136261u, entries,
                              header.entry_count * sizeof(*entries)) != header.checksum) {
        reader_large_free(entries);
        return NULL;
    }
    *entry_count = (int)header.entry_count;
    return entries;
}

static int restore_cached_slot(int book_index, const char *path,
                               const reader_cache_entry_t *entries, int entry_count) {
    struct stat info;
    struct stat content_info;
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
        !entry->layout_valid || entry->page_count <= 0 || entry->page_count > READER_MAX_PAGES ||
        entry->chapter_count <= 0 || entry->chapter_count > READER_MAX_CHAPTERS) return -1;
    slot = &slots[book_index];
    clear_slot(slot);
    snprintf(slot->path, sizeof(slot->path), "%s", path);
    slot->id = entry->id;
    if (has_epub_extension(path)) {
        if (epub_content_path(slot->id, slot->content_path, sizeof(slot->content_path)) != 0 ||
            stat(slot->content_path, &content_info) != 0 || content_info.st_size <= 0) return -1;
    } else {
        snprintf(slot->content_path, sizeof(slot->content_path), "%s", path);
        content_info = info;
    }
    for (int i = 0; i <= entry->page_count; i++) {
        if (entry->page_offsets[i] > (uint64_t)content_info.st_size ||
            (i > 0 && entry->page_offsets[i] < entry->page_offsets[i - 1])) return -1;
    }
    if (entry->title[0] != '\0') snprintf(slot->title, sizeof(slot->title), "%s", entry->title);
    else copy_title(slot->title, sizeof(slot->title), path);
    if (entry->author[0] != '\0') snprintf(slot->author, sizeof(slot->author), "%s", entry->author);
    else snprintf(slot->author, sizeof(slot->author), "SD 卡文件");
    if (info.st_size >= 1024 * 1024) {
        snprintf(slot->size_label, sizeof(slot->size_label), "%.1f MB",
                 (double)info.st_size / (1024.0 * 1024.0));
    } else {
        snprintf(slot->size_label, sizeof(slot->size_label), "%u KB",
                 (unsigned int)((info.st_size + 1023) / 1024));
    }
    snprintf(slot->file_type, sizeof(slot->file_type), "%s",
             entry->file_type[0] != '\0' ? entry->file_type :
             (has_epub_extension(path) ? "EPUB" : "TXT"));
    slot->page_count = entry->page_count;
    slot->chapter_count = entry->chapter_count;
    slot->truncated = entry->truncated;
    slot->layout_valid = 1;
    slot->layout_complete = entry->layout_complete ? 1 : 0;
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
    slot->layout_valid = 1;
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
    entry = reader_large_calloc(1, sizeof(*entry));
    if (entry == NULL) return -1;
    file = fopen(temp_path, "wb");
    if (file == NULL) {
        reader_large_free(entry);
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
        snprintf(entry->title, sizeof(entry->title), "%s", slot->title);
        snprintf(entry->author, sizeof(entry->author), "%s", slot->author);
        snprintf(entry->file_type, sizeof(entry->file_type), "%s", slot->file_type);
        entry->page_count = slot->page_count;
        entry->chapter_count = slot->chapter_count;
        entry->truncated = slot->truncated;
        entry->layout_valid = slot->layout_valid;
        entry->layout_complete = slot->layout_complete;
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
        reader_large_free(entry);
        remove(temp_path);
        return -1;
    }
    reader_large_free(entry);
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
    reader_large_free(entry);
    remove(temp_path);
    return -1;
}

static reader_cache_entry_t *load_cache_entries_coordinated(const char *directory,
                                                            int *entry_count) {
    reader_cache_entry_t *entries;
    storage_io_lock(STORAGE_IO_BACKGROUND);
    entries = load_cache_entries(directory, entry_count);
    storage_io_unlock();
    return entries;
}

static int save_cache_coordinated(const char *directory) {
    int result;
    storage_io_lock(STORAGE_IO_BACKGROUND);
    result = save_cache(directory);
    storage_io_unlock();
    return result;
}

static int clear_library(void) {
    if (ensure_slots() != 0) return -1;
    storage_io_lock(STORAGE_IO_BACKGROUND);
    if (page_text_stream != NULL) {
        fclose(page_text_stream);
        page_text_stream = NULL;
        page_text_stream_book_id = 0;
    }
    memset(page_cache, 0, sizeof(page_cache));
    memset(slots, 0, sizeof(*slots) * APP_BOOK_COUNT);
    storage_io_unlock();
    return 0;
}

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

static int collect_book_paths(const char *directory, char paths[][READER_BOOK_PATH_MAX]) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    if (directory == NULL || (dir = opendir(directory)) == NULL) return 0;
    while ((entry = readdir(dir)) != NULL) {
        char path[READER_BOOK_PATH_MAX];
        if (!has_book_extension(entry->d_name) ||
            snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) >= (int)sizeof(path)) continue;
        insert_sorted_path(paths, &count, path);
    }
    closedir(dir);
    return count;
}

int reader_library_count_directory(const char *directory) {
    char (*paths)[READER_BOOK_PATH_MAX] =
        reader_large_calloc(APP_BOOK_COUNT, READER_BOOK_PATH_MAX);
    int count;
    if (paths == NULL) return 0;
    count = collect_book_paths(directory, paths);
    reader_large_free(paths);
    return count;
}

int reader_library_book_count(void) {
    int count = 0;
    if (slots == NULL) return 0;
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
    if (clear_library() != 0) return -1;
    return load_slot(0, path);
}

int reader_library_load_directory(const char *directory) {
    char (*paths)[READER_BOOK_PATH_MAX] =
        reader_large_calloc(APP_BOOK_COUNT, READER_BOOK_PATH_MAX);
    int count;
    int loaded = 0;
    int cache_entry_count = 0;
    reader_cache_entry_t *cache_entries = NULL;
    if (paths == NULL) return 0;
    count = collect_book_paths(directory, paths);
    if (clear_library() != 0) {
        reader_large_free(paths);
        return 0;
    }
    cache_directory[0] = '\0';
    if (cache_is_enabled_for(directory)) {
        snprintf(cache_directory, sizeof(cache_directory), "%s", directory);
        cache_entries = load_cache_entries_coordinated(directory, &cache_entry_count);
    }
    for (int i = 0; i < count; i++) {
        if (restore_cached_slot(loaded, paths[i], cache_entries, cache_entry_count) == 0) {
            printf("reader_library: cache hit %s (%d pages)\n",
                   paths[i], slots[loaded].page_count);
            fflush(stdout);
            report_index_progress(&slots[loaded], 100);
            loaded++;
            continue;
        }
        printf("reader_library: indexing %s\n", paths[i]);
        fflush(stdout);
        if (load_slot_initial(loaded, paths[i]) == 0) {
            printf("reader_library: initial pages ready %s (%d pages)\n",
                   paths[i], slots[loaded].page_count);
            fflush(stdout);
            loaded++;
        } else {
            printf("reader_library: failed to index %s\n", paths[i]);
            fflush(stdout);
        }
    }
    reader_large_free(cache_entries);
    if (cache_directory[0] != '\0') {
        if (save_cache_coordinated(cache_directory) == 0) {
            printf("reader_library: index cache updated\n");
        } else {
            printf("reader_library: failed to update index cache\n");
        }
        fflush(stdout);
    }
    reader_large_free(paths);
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
    storage_io_lock(STORAGE_IO_BACKGROUND);
    invalidate_page_cache();
    storage_io_unlock();
    for (int i = 0; i < reader_library_book_count(); i++) {
        if (index_book(&slots[i]) != 0) return -1;
    }
    if (cache_directory[0] != '\0' && save_cache_coordinated(cache_directory) != 0) {
        printf("reader_library: failed to update cache after layout change\n");
        fflush(stdout);
    }
    return 1;
}

int reader_library_ensure_book_layout(int book_index) {
    if (book_index < 0 || book_index >= reader_library_book_count()) return -1;
    if (slots[book_index].layout_valid) return 0;
    printf("reader_library: updating layout for %s\n", slots[book_index].path);
    fflush(stdout);
    if (index_book_initial(&slots[book_index]) != 0) return -1;
    if (cache_directory[0] != '\0' && save_cache_coordinated(cache_directory) != 0) {
        printf("reader_library: failed to update index cache after lazy pagination\n");
        fflush(stdout);
    }
    return 1;
}

int reader_library_book_layout_complete(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count()
               ? slots[book_index].layout_complete : 1;
}

static void fix_slot_info_pointers(reader_book_slot_t *slot) {
    slot->info.title = slot->title;
    slot->info.author = slot->author;
    slot->info.size_label = slot->size_label;
    slot->info.file_type = slot->file_type;
    slot->info.chapter_title = slot->chapter_titles[0];
}

int reader_library_build_book_layout_background(int book_index) {
    reader_book_slot_t *result;
    reader_layout_t result_layout;
    uint32_t result_id;
    if (book_index < 0 || book_index >= reader_library_book_count() ||
        slots[book_index].layout_complete || background_result_ready) return 0;
    result = reader_large_malloc(sizeof(*result));
    if (result == NULL) return -1;
    *result = slots[book_index];
    result_layout = layout;
    result_id = slots[book_index].id;
    background_progress = 0;
    if (index_book_with_layout(result, &result_layout, READER_MAX_PAGES, 0) != 0) {
        reader_large_free(result);
        return -1;
    }
    background_result_layout = result_layout;
    background_result_book_id = result_id;
    background_result_book_index = book_index;
    background_result = result;
    background_result_ready = 1;
    return 1;
}

int reader_library_background_progress(void) {
    return background_progress;
}

int reader_library_commit_background_layout(int *book_index) {
    reader_book_slot_t *result;
    int index;
    if (!background_result_ready) return 0;
    result = background_result;
    index = background_result_book_index;
    background_result_ready = 0;
    background_result = NULL;
    background_result_book_index = -1;
    if (result == NULL || index < 0 || index >= reader_library_book_count() ||
        slots[index].id != background_result_book_id ||
        memcmp(&layout, &background_result_layout, sizeof(layout)) != 0) {
        reader_large_free(result);
        return -1;
    }
    slots[index] = *result;
    fix_slot_info_pointers(&slots[index]);
    storage_io_lock(STORAGE_IO_BACKGROUND);
    invalidate_page_cache();
    storage_io_unlock();
    reader_large_free(result);
    if (book_index != NULL) *book_index = index;
    if (cache_directory[0] != '\0' && save_cache_coordinated(cache_directory) != 0) {
        printf("reader_library: failed to save completed background index\n");
        fflush(stdout);
    }
    return 1;
}

int reader_library_configure_layout_for_book(int font_size, int font_family, int content_width,
                                             int content_height, int line_height,
                                             int indent_enabled, int bold_enabled,
                                             int book_index) {
    reader_layout_t next = {font_size, font_family, content_width, content_height, line_height,
                            indent_enabled ? 1 : 0, bold_enabled ? 1 : 0};
    int changed = memcmp(&layout, &next, sizeof(layout)) != 0;
    if (changed) {
        layout = next;
        storage_io_lock(STORAGE_IO_BACKGROUND);
        invalidate_page_cache();
        storage_io_unlock();
        for (int i = 0; i < reader_library_book_count(); i++) {
            slots[i].layout_valid = 0;
            slots[i].layout_complete = 0;
        }
    }
    if (reader_library_ensure_book_layout(book_index) < 0) return -1;
    return changed ? 1 : 0;
}

int reader_library_is_truncated(int book_index) {
    return book_index >= 0 && book_index < reader_library_book_count() ? slots[book_index].truncated : 0;
}

static void invalidate_page_cache(void) {
    memset(page_cache, 0, sizeof(page_cache));
    page_cache_clock = 0;
}

static reader_page_cache_entry_t *load_page_cache_entry(int book_index,
                                                        int page_index) {
    reader_book_slot_t *slot;
    reader_page_cache_entry_t *entry;
    int replacement = 0;
    long length;
    size_t read;
    if (book_index < 0 || book_index >= reader_library_book_count()) return NULL;
    slot = &slots[book_index];
    if (page_index < 0 || page_index >= slot->page_count) return NULL;
    for (int i = 0; i < READER_PAGE_CACHE_SIZE; i++) {
        if (page_cache[i].book_id == slot->id &&
            page_cache[i].page_index == page_index) {
            page_cache[i].used_at = ++page_cache_clock;
            return &page_cache[i];
        }
        if (page_cache[i].book_id == 0 ||
            page_cache[i].used_at < page_cache[replacement].used_at) {
            replacement = i;
        }
    }
    if (page_text_stream == NULL || page_text_stream_book_id != slot->id) {
        if (page_text_stream != NULL) fclose(page_text_stream);
        page_text_stream = fopen(slot->content_path, "rb");
        page_text_stream_book_id = page_text_stream != NULL ? slot->id : 0;
    }
    length = slot->page_offsets[page_index + 1] - slot->page_offsets[page_index];
    if (length <= 0 || length >= READER_PAGE_TEXT_MAX ||
        page_text_stream == NULL ||
        fseek(page_text_stream, slot->page_offsets[page_index], SEEK_SET) != 0) {
        if (page_text_stream != NULL) fclose(page_text_stream);
        page_text_stream = NULL;
        page_text_stream_book_id = 0;
        return NULL;
    }
    entry = &page_cache[replacement];
    read = fread(entry->text, 1, (size_t)length, page_text_stream);
    while (read > 0 && entry->text[read - 1] == '\f') read--;
    entry->text[read] = '\0';
    entry->book_id = slot->id;
    entry->page_index = page_index;
    entry->used_at = ++page_cache_clock;
    return entry;
}

const char *reader_library_page_text(int book_index, int page_index) {
    reader_page_cache_entry_t *entry;
    storage_io_lock(STORAGE_IO_FOREGROUND);
    entry = load_page_cache_entry(book_index, page_index);
    if (entry == NULL) {
        page_text[0] = '\0';
    } else {
        snprintf(page_text, sizeof(page_text), "%s", entry->text);
    }
    storage_io_unlock();
    return page_text;
}

void reader_library_prefetch_adjacent_pages(int book_index, int page_index) {
    const int adjacent[] = {page_index - 1, page_index + 1};
    for (int i = 0; i < 2; i++) {
        storage_io_lock(STORAGE_IO_BACKGROUND);
        (void)load_page_cache_entry(book_index, adjacent[i]);
        storage_io_unlock();
    }
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
