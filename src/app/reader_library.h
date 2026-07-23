#ifndef READER_LIBRARY_H
#define READER_LIBRARY_H

#include <stdint.h>

typedef struct {
    const char *title;
    const char *author;
    const char *size_label;
    const char *file_type;
    const char *chapter_title;
} reader_book_t;

typedef void (*reader_library_progress_callback_t)(int book_index, int percent, void *context);

int reader_library_book_count(void);
uint32_t reader_library_book_id(int book_index);
const char *reader_library_book_path(int book_index);
const reader_book_t *reader_library_book(int book_index);
int reader_library_page_count(int book_index);
const char *reader_library_source_text(int book_index);
int reader_library_load_book_file(int book_index, const char *path);
int reader_library_open_book_file(int book_index, const char *path);
int reader_library_load_directory(const char *directory);
int reader_library_count_directory(const char *directory);
int reader_library_load_external_books(void);
int reader_library_configure_layout(int font_size, int font_family, int content_width,
                                    int content_height, int line_height,
                                    int indent_enabled, int bold_enabled);
int reader_library_configure_layout_for_book(int font_size, int font_family, int content_width,
                                             int content_height, int line_height,
                                             int indent_enabled, int bold_enabled,
                                             int book_index);
int reader_library_ensure_book_layout(int book_index);
int reader_library_book_layout_complete(int book_index);
int reader_library_build_book_layout_background(int book_index);
int reader_library_commit_background_layout(int *book_index);
int reader_library_background_progress(void);
void reader_library_set_progress_callback(reader_library_progress_callback_t callback,
                                          void *context);
int reader_library_is_truncated(int book_index);
const char *reader_library_page_text(int book_index, int page_index);
void reader_library_prefetch_adjacent_pages(int book_index, int page_index);
const char *reader_library_chapter_title(int book_index, int chapter_index);
int reader_library_chapter_page(int book_index, int chapter_index);
int reader_library_chapter_count(int book_index);

#endif
