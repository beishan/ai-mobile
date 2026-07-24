#ifndef READER_LIBRARY_PATH_H
#define READER_LIBRARY_PATH_H

#include <stddef.h>
#include <stdint.h>

int reader_path_has_txt_extension(const char *name);
int reader_path_has_epub_extension(const char *name);
int reader_path_has_book_extension(const char *name);
const char *reader_path_basename(const char *path);
void reader_path_copy_title(char *dest, size_t dest_size, const char *path);
uint32_t reader_path_identity(const char *path, long size);

#endif
