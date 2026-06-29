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

/* Integer square root (Newton's method) — avoids floating-point on embedded targets */
static int isqrt_int(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    int x = n;
    int y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

void gfx_fill_rounded_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, int radius, gfx_color_t color) {
    if (w <= 0 || h <= 0) return;

    int max_radius = (w < h ? w : h) / 2;
    if (radius > max_radius) radius = max_radius;
    if (radius < 0) radius = 0;

    if (radius == 0) {
        gfx_fill_rect(fb, x, y, w, h, color);
        return;
    }

    int r = radius;

    /* Fill the middle section (straight sides, full width) */
    int mid_y0 = y + r;
    int mid_y1 = y + h - r;
    if (mid_y1 > mid_y0) {
        gfx_fill_rect(fb, x, mid_y0, w, mid_y1 - mid_y0, color);
    }

    /*
     * Fill corner rows using the circle equation.
     * For a corner at offset dy from the top/bottom edge, the horizontal
     * half-width from the corner centre is: dx = sqrt(r^2 - (r-dy)^2).
     * The span on that row goes from (x + r - dx) to (x + w-1 - r + dx).
     */
    for (int dy = 0; dy < r; dy++) {
        int dist_y = r - dy;
        int dx = isqrt_int(r * r - dist_y * dist_y);
        int left  = x + r - dx;
        int right = x + w - 1 - r + dx;
        int span_w = right - left + 1;
        if (span_w > 0) {
            gfx_fill_rect(fb, left, y + dy, span_w, 1, color);
            gfx_fill_rect(fb, left, y + h - 1 - dy, span_w, 1, color);
        }
    }
}

void gfx_draw_rounded_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, int radius, gfx_color_t color) {
    if (w <= 0 || h <= 0) return;

    int max_radius = (w < h ? w : h) / 2;
    if (radius > max_radius) radius = max_radius;
    if (radius < 0) radius = 0;

    /* Straight edges */
    gfx_fill_rect(fb, x + radius, y, w - 2 * radius, 1, color);
    gfx_fill_rect(fb, x + radius, y + h - 1, w - 2 * radius, 1, color);
    gfx_fill_rect(fb, x, y + radius, 1, h - 2 * radius, color);
    gfx_fill_rect(fb, x + w - 1, y + radius, 1, h - 2 * radius, color);

    /* Corner arcs via integer circle equation */
    if (radius > 0) {
        int r = radius;
        for (int dy = 0; dy < r; dy++) {
            int dist_y = r - dy;
            int dx = isqrt_int(r * r - dist_y * dist_y);
            /* Top-left */
            gfx_set_pixel(fb, x + r - dx, y + dy, color);
            /* Top-right */
            gfx_set_pixel(fb, x + w - 1 - r + dx, y + dy, color);
            /* Bottom-left */
            gfx_set_pixel(fb, x + r - dx, y + h - 1 - dy, color);
            /* Bottom-right */
            gfx_set_pixel(fb, x + w - 1 - r + dx, y + h - 1 - dy, color);
        }
    }
}

void gfx_draw_rounded_rect_thick(gfx_framebuffer_t *fb, int x, int y, int w, int h, int radius, int thickness, gfx_color_t color) {
    if (w <= 0 || h <= 0 || thickness <= 0) return;

    int max_radius = (w < h ? w : h) / 2;
    if (radius > max_radius) radius = max_radius;
    if (radius < 0) radius = 0;

    /*
     * Fill-and-subtract approach:
     * 1. Fill the entire outer rounded rect with the border colour.
     * 2. Fill the inner rounded rect with GFX_WHITE to carve out the centre.
     * This produces a gap-free thick border with perfectly smooth arcs,
     * unlike the old concentric-arc method which left holes in the corners.
     */
    gfx_fill_rounded_rect(fb, x, y, w, h, radius, color);

    int inner_x = x + thickness;
    int inner_y = y + thickness;
    int inner_w = w - 2 * thickness;
    int inner_h = h - 2 * thickness;
    int inner_radius = radius - thickness;
    if (inner_radius < 0) inner_radius = 0;

    if (inner_w > 0 && inner_h > 0) {
        gfx_fill_rounded_rect(fb, inner_x, inner_y, inner_w, inner_h, inner_radius, GFX_WHITE);
    }
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
