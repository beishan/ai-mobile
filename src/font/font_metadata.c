#include "font/font_metadata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void font_metadata_parse_filename(external_font_t *font, const char *filepath) {
    const char *filename = strrchr(filepath, '/');
    const char *cursor;
    const char *family;
    char *end;
    size_t family_len;
    filename = filename != NULL ? filename + 1 : filepath;
    snprintf(font->name, sizeof(font->name), "%s", filename);
    snprintf(font->path, sizeof(font->path), "%s", filepath);
    cursor = filename;
    font->nominal_size = (int)strtol(cursor, &end, 10);
    if (end != cursor) cursor = end;
    while (*cursor == ' ' || *cursor == '_' || *cursor == '-') cursor++;
    {
        int width = (int)strtol(cursor, &end, 10);
        if (end != cursor) {
            cursor = end;
            if (*cursor == 'x' || *cursor == 'X') cursor++;
            else if ((unsigned char)cursor[0] == 0xc3 &&
                     (unsigned char)cursor[1] == 0x97) cursor += 2;
            else width = 0;
            if (width > 0) {
                int height = (int)strtol(cursor, &end, 10);
                if (end != cursor && height > 0) {
                    font->width = width;
                    font->height = height;
                    cursor = end;
                }
            }
        }
    }
    while (*cursor == ' ' || *cursor == '_' || *cursor == '-') cursor++;
    family = *cursor != '\0' ? cursor : filename;
    family_len = strlen(family);
    if (family_len > 4 && strcmp(family + family_len - 4, ".bin") == 0) family_len -= 4;
    if (family_len >= sizeof(font->family)) family_len = sizeof(font->family) - 1;
    memcpy(font->family, family, family_len);
    font->family[family_len] = '\0';
}
