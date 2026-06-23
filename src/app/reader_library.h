#ifndef READER_LIBRARY_H
#define READER_LIBRARY_H

typedef struct {
    const char *title;
    const char *author;
    const char *size_label;
    const char *file_type;
    const char *chapter_title;
} reader_book_t;

int reader_library_book_count(void);
const reader_book_t *reader_library_book(int book_index);
int reader_library_page_count(int book_index);
const char *reader_library_source_text(int book_index);
int reader_library_load_book_file(int book_index, const char *path);
const char *reader_library_page_text(int book_index, int page_index);
const char *reader_library_chapter_title(int book_index, int chapter_index);
int reader_library_chapter_page(int book_index, int chapter_index);
int reader_library_chapter_count(int book_index);

#endif
