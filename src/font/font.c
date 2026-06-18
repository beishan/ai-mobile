#include "font/font.h"

#include <stddef.h>
#include <string.h>

#include "fonts/sim_zh12.h"
#include "fonts/sim_zh14.h"
#include "fonts/sim_zh16.h"
#include "fonts/sim_zh18.h"
#include "fonts/sim_zh20.h"
#include "fonts/sim_zh22.h"
#include "fonts/sim_zh24.h"

static const font_face_t face12 = {12, 15, SIM_ZH12_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh12_glyphs, sim_zh12_glyph_count};
static const font_face_t face14 = {14, 17, SIM_ZH14_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh14_glyphs, sim_zh14_glyph_count};
static const font_face_t face16 = {16, 20, SIM_ZH16_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh16_glyphs, sim_zh16_glyph_count};
static const font_face_t face18 = {18, 23, SIM_ZH18_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh18_glyphs, sim_zh18_glyph_count};
static const font_face_t face20 = {20, 25, SIM_ZH20_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh20_glyphs, sim_zh20_glyph_count};
static const font_face_t face22 = {22, 28, SIM_ZH22_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh22_glyphs, sim_zh22_glyph_count};
static const font_face_t face24 = {24, 30, SIM_ZH24_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh24_glyphs, sim_zh24_glyph_count};

const font_face_t *font_get_face(font_size_t size) {
    switch (size) {
        case FONT_SIZE_12:
            return &face12;
        case FONT_SIZE_14:
            return &face14;
        case FONT_SIZE_18:
            return &face18;
        case FONT_SIZE_20:
            return &face20;
        case FONT_SIZE_22:
            return &face22;
        case FONT_SIZE_24:
            return &face24;
        case FONT_SIZE_16:
        default:
            return &face16;
    }
}

int font_load_default(font_t *font) {
    if (font == NULL) {
        return 0;
    }
    *font = *font_get_face(FONT_SIZE_16);
    return 1;
}

void font_free(font_t *font) {
    if (font != NULL) {
        memset(font, 0, sizeof(*font));
    }
}

int font_decode_utf8(const unsigned char **cursor, uint32_t *codepoint) {
    const unsigned char *p;
    if (cursor == NULL || *cursor == NULL || codepoint == NULL) {
        return 0;
    }
    p = *cursor;
    if (*p == '\0') {
        return 0;
    }
    if (p[0] < 0x80) {
        *codepoint = p[0];
        *cursor = p + 1;
        return 1;
    }
    if ((p[0] & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
        *codepoint = ((uint32_t)(p[0] & 0x1f) << 6) | (uint32_t)(p[1] & 0x3f);
        *cursor = p + 2;
        return 1;
    }
    if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
        *codepoint = ((uint32_t)(p[0] & 0x0f) << 12) |
                     ((uint32_t)(p[1] & 0x3f) << 6) |
                     (uint32_t)(p[2] & 0x3f);
        *cursor = p + 3;
        return 1;
    }
    if ((p[0] & 0xf8) == 0xf0 &&
        (p[1] & 0xc0) == 0x80 &&
        (p[2] & 0xc0) == 0x80 &&
        (p[3] & 0xc0) == 0x80) {
        *codepoint = ((uint32_t)(p[0] & 0x07) << 18) |
                     ((uint32_t)(p[1] & 0x3f) << 12) |
                     ((uint32_t)(p[2] & 0x3f) << 6) |
                     (uint32_t)(p[3] & 0x3f);
        *cursor = p + 4;
        return 1;
    }
    *codepoint = 0xfffd;
    *cursor = p + 1;
    return 1;
}

const font_glyph_t *font_find_glyph(const font_face_t *font, uint32_t codepoint) {
    if (font == NULL || font->glyphs == NULL) {
        return NULL;
    }
    for (int i = 0; i < font->glyph_count; i++) {
        if (font->glyphs[i].codepoint == codepoint) {
            return &font->glyphs[i];
        }
    }
    return NULL;
}

int font_measure_text(const font_face_t *font, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    int width = 0;
    while (cursor != NULL && *cursor != '\0') {
        uint32_t cp;
        if (!font_decode_utf8(&cursor, &cp)) {
            break;
        }
        if (cp == '\n') {
            break;
        }
        const font_glyph_t *glyph = font_find_glyph(font, cp);
        width += glyph != NULL ? glyph->advance : (font != NULL ? font->size : 16);
    }
    return width;
}

static void draw_replacement(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, gfx_color_t color) {
    int size = font != NULL ? font->size : 16;
    gfx_draw_rect(fb, x, y, size, size, color);
}

static void draw_glyph_bitmap(const font_face_t *font, gfx_framebuffer_t *fb, const font_glyph_t *glyph, int x, int y, gfx_color_t color) {
    int bytes_per_row = (font->size + 7) / 8;
    for (int row = 0; row < font->size; row++) {
        for (int col = 0; col < font->size; col++) {
            uint8_t byte = glyph->bitmap[row * bytes_per_row + col / 8];
            if ((byte & (uint8_t)(1u << (7 - (col % 8)))) != 0) {
                gfx_set_pixel(fb, x + col, y + row, color);
            }
        }
    }
}

void font_draw_text(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color) {
    const unsigned char *cursor = (const unsigned char *)text;
    int draw_x = x;
    if (font == NULL || fb == NULL || text == NULL) {
        return;
    }
    while (*cursor != '\0') {
        uint32_t cp;
        if (!font_decode_utf8(&cursor, &cp)) {
            break;
        }
        if (cp == '\n') {
            draw_x = x;
            y += font->line_height;
            continue;
        }
        const font_glyph_t *glyph = font_find_glyph(font, cp);
        if (glyph != NULL) {
            draw_glyph_bitmap(font, fb, glyph, draw_x, y, color);
            draw_x += glyph->advance;
        } else {
            draw_replacement(font, fb, draw_x, y, color);
            draw_x += font->size;
        }
    }
}

void font_draw_text_aligned(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, font_align_t align, gfx_color_t color) {
    int text_width = font_measure_text(font, text);
    int draw_x = x;
    if (align == FONT_ALIGN_CENTER) {
        draw_x = x + (width - text_width) / 2;
    } else if (align == FONT_ALIGN_RIGHT) {
        draw_x = x + width - text_width;
    }
    font_draw_text(font, fb, draw_x, y, text, color);
}

void font_draw_ellipsis(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, gfx_color_t color) {
    const char *ellipsis = "...";
    const unsigned char *cursor = (const unsigned char *)text;
    int used = 0;
    int ellipsis_width = font_measure_text(font, ellipsis);
    if (font_measure_text(font, text) <= width) {
        font_draw_text(font, fb, x, y, text, color);
        return;
    }
    while (*cursor != '\0') {
        const unsigned char *before = cursor;
        uint32_t cp;
        font_decode_utf8(&cursor, &cp);
        const font_glyph_t *glyph = font_find_glyph(font, cp);
        int advance = glyph != NULL ? glyph->advance : font->size;
        if (used + advance + ellipsis_width > width) {
            break;
        }
        char tmp[5] = {0};
        int len = (int)(cursor - before);
        memcpy(tmp, before, (size_t)len);
        font_draw_text(font, fb, x + used, y, tmp, color);
        used += advance;
    }
    font_draw_text(font, fb, x + used, y, ellipsis, color);
}

void font_draw_text_box_spaced(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, int line_height, gfx_color_t color) {
    const unsigned char *cursor = (const unsigned char *)text;
    int line_x = x;
    int line_y = y;
    if (font == NULL || fb == NULL || text == NULL) {
        return;
    }
    if (line_height < font->size) {
        line_height = font->size;
    }
    while (*cursor != '\0' && line_y + font->size <= y + height) {
        const unsigned char *before = cursor;
        uint32_t cp;
        font_decode_utf8(&cursor, &cp);
        if (cp == '\n') {
            line_x = x;
            line_y += line_height;
            continue;
        }
        const font_glyph_t *glyph = font_find_glyph(font, cp);
        int advance = glyph != NULL ? glyph->advance : font->size;
        if (line_x > x && line_x + advance > x + width) {
            line_x = x;
            line_y += line_height;
        }
        if (line_y + font->size > y + height) {
            break;
        }
        char tmp[5] = {0};
        int len = (int)(cursor - before);
        memcpy(tmp, before, (size_t)len);
        font_draw_text(font, fb, line_x, line_y, tmp, color);
        line_x += advance;
    }
}

void font_draw_text_box(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, gfx_color_t color) {
    font_draw_text_box_spaced(font, fb, x, y, width, height, text, font != NULL ? font->line_height : 16, color);
}
