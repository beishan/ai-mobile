#ifndef EPUB_READER_H
#define EPUB_READER_H

#include <stddef.h>

typedef struct {
    char title[160];
    char author[64];
} epub_metadata_t;

int epub_extract_text(const char *epub_path, const char *text_path,
                      epub_metadata_t *metadata);

#endif
