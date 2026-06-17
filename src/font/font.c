#include "font/font.h"

#include <stddef.h>

#include "fonts/sim_zh16.h"

static void draw_replacement(gfx_framebuffer_t *fb, int x, int y, int w, int h, gfx_color_t color) {
    gfx_draw_rect(fb, x, y, w, h, color);
    gfx_fill_rect(fb, x + 3, y + 3, w - 6, 2, color);
    gfx_fill_rect(fb, x + 3, y + h - 5, w - 6, 2, color);
}

int font_load_default(font_t *font) {
    if (font == NULL) {
        return 0;
    }

    font->width = SIM_ZH16_WIDTH;
    font->height = SIM_ZH16_HEIGHT;
    font->glyphs = (const font_glyph_t *)sim_zh16_glyphs;
    font->glyph_count = sim_zh16_glyph_count;
    return 1;
}

void font_free(font_t *font) {
    if (font == NULL) {
        return;
    }

    font->width = 0;
    font->height = 0;
    font->glyphs = NULL;
    font->glyph_count = 0;
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

const font_glyph_t *font_find_glyph(const font_t *font, uint32_t codepoint) {
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

static void draw_glyph_bitmap(gfx_framebuffer_t *fb, const font_glyph_t *glyph, int x, int y, int w, int h, gfx_color_t color) {
    const uint8_t *bitmap = glyph->bitmap;
    int bytes_per_row = w / 8;

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint8_t byte = bitmap[row * bytes_per_row + col / 8];
            if ((byte & (uint8_t)(1u << (7 - (col % 8)))) != 0) {
                gfx_set_pixel(fb, x + col, y + row, color);
            }
        }
    }
}

int font_measure_text(const font_t *font, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    int width = 0;
    int glyph_width = font != NULL && font->width > 0 ? font->width : 16;

    while (cursor != NULL && *cursor != '\0') {
        uint32_t cp;
        if (!font_decode_utf8(&cursor, &cp)) {
            break;
        }
        (void)cp;
        width += glyph_width;
    }

    return width;
}

void font_draw_text(const font_t *font, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color) {
    const unsigned char *cursor = (const unsigned char *)text;
    int glyph_width = font != NULL && font->width > 0 ? font->width : 16;
    int glyph_height = font != NULL && font->height > 0 ? font->height : 16;
    int draw_x = x;

    if (fb == NULL || text == NULL) {
        return;
    }

    while (*cursor != '\0') {
        uint32_t cp;
        if (!font_decode_utf8(&cursor, &cp)) {
            break;
        }
        if (cp == '\n') {
            draw_x = x;
            y += glyph_height + 2;
            continue;
        }
        if (cp == ' ') {
            draw_x += glyph_width / 2;
            continue;
        }
        const font_glyph_t *glyph = font_find_glyph(font, cp);
        if (glyph != NULL) {
            draw_glyph_bitmap(fb, glyph, draw_x, y, glyph_width, glyph_height, color);
        } else {
            draw_replacement(fb, draw_x, y, glyph_width, glyph_height, color);
        }
        draw_x += glyph_width;
    }
}
