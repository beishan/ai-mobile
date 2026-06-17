#ifndef FONT_H
#define FONT_H

#include <stdint.h>

#include "gfx/gfx.h"

typedef struct {
    uint32_t codepoint;
    const uint8_t *bitmap;
} font_glyph_t;

typedef struct {
    int width;
    int height;
    const font_glyph_t *glyphs;
    int glyph_count;
} font_t;

int font_load_default(font_t *font);
void font_free(font_t *font);
int font_decode_utf8(const unsigned char **cursor, uint32_t *codepoint);
const font_glyph_t *font_find_glyph(const font_t *font, uint32_t codepoint);
int font_measure_text(const font_t *font, const char *text);
void font_draw_text(const font_t *font, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color);

#endif
