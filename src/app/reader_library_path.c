#include "app/reader_library_path.h"

#include <string.h>

int reader_path_has_txt_extension(const char *name) {
    size_t len;
    if (name == NULL) return 0;
    len = strlen(name);
    return len > 4 && name[len - 4] == '.' &&
           (name[len - 3] == 't' || name[len - 3] == 'T') &&
           (name[len - 2] == 'x' || name[len - 2] == 'X') &&
           (name[len - 1] == 't' || name[len - 1] == 'T');
}

int reader_path_has_epub_extension(const char *name) {
    size_t len;
    if (name == NULL) return 0;
    len = strlen(name);
    return len > 5 && name[len - 5] == '.' &&
           (name[len - 4] == 'e' || name[len - 4] == 'E') &&
           (name[len - 3] == 'p' || name[len - 3] == 'P') &&
           (name[len - 2] == 'u' || name[len - 2] == 'U') &&
           (name[len - 1] == 'b' || name[len - 1] == 'B');
}

int reader_path_has_book_extension(const char *name) {
    return reader_path_has_txt_extension(name) || reader_path_has_epub_extension(name);
}

const char *reader_path_basename(const char *path) {
    const char *base = path == NULL ? "" : path;
    for (const char *cursor = base; *cursor != '\0'; cursor++) {
        if (*cursor == '/') base = cursor + 1;
    }
    return base;
}

void reader_path_copy_title(char *dest, size_t dest_size, const char *path) {
    const char *base;
    size_t len;
    if (dest == NULL || dest_size == 0) return;
    base = reader_path_basename(path);
    len = strlen(base);
    if (len > 4 && reader_path_has_txt_extension(base)) len -= 4;
    else if (len > 5 && reader_path_has_epub_extension(base)) len -= 5;
    if (len >= dest_size) len = dest_size - 1;
    memcpy(dest, base, len);
    dest[len] = '\0';
}

uint32_t reader_path_identity(const char *path, long size) {
    uint32_t hash = 2166136261u;
    const unsigned char *cursor = (const unsigned char *)(path == NULL ? "" : path);
    while (*cursor != '\0') {
        hash ^= *cursor++;
        hash *= 16777619u;
    }
    for (unsigned int i = 0; i < sizeof(size); i++) {
        hash ^= (uint32_t)((unsigned long)size >> (i * 8)) & 0xffu;
        hash *= 16777619u;
    }
    return hash != 0 ? hash : 1u;
}
