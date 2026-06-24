#include "gfx/gfx.h"

#include <stddef.h>

static int in_bounds(int x, int y) {
    return x >= 0 && x < GFX_WIDTH && y >= 0 && y < GFX_HEIGHT;
}

void gfx_init(gfx_framebuffer_t *fb) {
    gfx_clear(fb, GFX_WHITE);
}

void gfx_clear(gfx_framebuffer_t *fb, gfx_color_t color) {
    if (fb == NULL) {
        return;
    }

    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x++) {
            fb->pixels[y][x] = (uint8_t)color;
        }
    }
}

int gfx_width(const gfx_framebuffer_t *fb) {
    (void)fb;
    return GFX_WIDTH;
}

int gfx_height(const gfx_framebuffer_t *fb) {
    (void)fb;
    return GFX_HEIGHT;
}

gfx_color_t gfx_get_pixel(const gfx_framebuffer_t *fb, int x, int y) {
    if (fb == NULL || !in_bounds(x, y)) {
        return GFX_WHITE;
    }

    return (gfx_color_t)fb->pixels[y][x];
}

void gfx_set_pixel(gfx_framebuffer_t *fb, int x, int y, gfx_color_t color) {
    if (fb == NULL || !in_bounds(x, y)) {
        return;
    }

    fb->pixels[y][x] = (uint8_t)color;
}

void gfx_fill_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, gfx_color_t color) {
    if (fb == NULL || w <= 0 || h <= 0) {
        return;
    }

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;

    if (x1 > GFX_WIDTH) {
        x1 = GFX_WIDTH;
    }
    if (y1 > GFX_HEIGHT) {
        y1 = GFX_HEIGHT;
    }

    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            fb->pixels[py][px] = (uint8_t)color;
        }
    }
}

void gfx_draw_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, gfx_color_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    gfx_fill_rect(fb, x, y, w, 1, color);
    gfx_fill_rect(fb, x, y + h - 1, w, 1, color);
    gfx_fill_rect(fb, x, y, 1, h, color);
    gfx_fill_rect(fb, x + w - 1, y, 1, h, color);
}

static void draw_block_char(gfx_framebuffer_t *fb, int x, int y, int scale, gfx_color_t color) {
    int s = scale <= 0 ? 1 : scale;

    gfx_draw_rect(fb, x, y, 5 * s, 7 * s, color);
    gfx_fill_rect(fb, x + s, y + s, 3 * s, s, color);
    gfx_fill_rect(fb, x + s, y + 3 * s, 3 * s, s, color);
    gfx_fill_rect(fb, x + s, y + 5 * s, 3 * s, s, color);
}

void gfx_draw_text(gfx_framebuffer_t *fb, int x, int y, const char *text, int scale, gfx_color_t color) {
    if (fb == NULL || text == NULL) {
        return;
    }

    int s = scale <= 0 ? 1 : scale;
    int cursor_x = x;

    for (const char *p = text; *p != '\0'; p++) {
        if (*p == '\n') {
            cursor_x = x;
            y += 9 * s;
            continue;
        }
        if (*p == ' ') {
            cursor_x += 4 * s;
            continue;
        }

        draw_block_char(fb, cursor_x, y, s, color);
        cursor_x += 7 * s;
    }
}

void gfx_draw_bitmap(gfx_framebuffer_t *fb, const uint8_t *data, int data_len,
                     int stride, int x, int y, int w, int h, gfx_color_t color) {
    if (fb == NULL || data == NULL || w <= 0 || h <= 0 || stride <= 0) {
        return;
    }

    for (int row = 0; row < h; row++) {
        int py = y + row;
        if (py < 0 || py >= GFX_HEIGHT) {
            continue;
        }
        int row_offset = row * stride;
        for (int col = 0; col < w; col++) {
            int px = x + col;
            if (px < 0 || px >= GFX_WIDTH) {
                continue;
            }
            int byte_idx = row_offset + (col / 8);
            if (byte_idx >= data_len) {
                break;
            }
            int bit = 7 - (col % 8);
            if (data[byte_idx] & (1 << bit)) {
                fb->pixels[py][px] = (uint8_t)color;
            }
        }
    }
}
