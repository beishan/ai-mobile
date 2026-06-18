#ifndef FONT_H
#define FONT_H

#include <stdint.h>

#include "gfx/gfx.h"

#define FONT_MAX_BITMAP_BYTES 96

typedef enum {
    FONT_SIZE_12 = 12,
    FONT_SIZE_14 = 14,
    FONT_SIZE_16 = 16,
    FONT_SIZE_18 = 18,
    FONT_SIZE_20 = 20,
    FONT_SIZE_22 = 22,
    FONT_SIZE_24 = 24
} font_size_t;

typedef enum {
    FONT_ALIGN_LEFT = 0,
    FONT_ALIGN_CENTER,
    FONT_ALIGN_RIGHT
} font_align_t;

typedef struct {
    uint32_t codepoint;
    uint8_t width;
    uint8_t height;
    uint8_t advance;
    uint8_t bitmap[FONT_MAX_BITMAP_BYTES];
} font_glyph_t;

typedef struct {
    int size;
    int line_height;
    int bytes_per_glyph;
    const font_glyph_t *glyphs;
    int glyph_count;
} font_face_t;

typedef font_face_t font_t;

int font_load_default(font_t *font);
void font_free(font_t *font);
const font_face_t *font_get_face(font_size_t size);
int font_decode_utf8(const unsigned char **cursor, uint32_t *codepoint);
const font_glyph_t *font_find_glyph(const font_face_t *font, uint32_t codepoint);
int font_measure_text(const font_face_t *font, const char *text);
void font_draw_text(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color);
void font_draw_text_aligned(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, font_align_t align, gfx_color_t color);
void font_draw_text_box(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, gfx_color_t color);
void font_draw_text_box_spaced(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, int line_height, gfx_color_t color);
void font_draw_ellipsis(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, gfx_color_t color);

#endif
