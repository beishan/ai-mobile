#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#define GFX_WIDTH 480
#define GFX_HEIGHT 800

typedef enum {
    GFX_WHITE = 0,
    GFX_BLACK = 1
} gfx_color_t;

typedef struct {
    uint8_t pixels[GFX_HEIGHT][GFX_WIDTH];
} gfx_framebuffer_t;

void gfx_init(gfx_framebuffer_t *fb);
void gfx_clear(gfx_framebuffer_t *fb, gfx_color_t color);
int gfx_width(const gfx_framebuffer_t *fb);
int gfx_height(const gfx_framebuffer_t *fb);
gfx_color_t gfx_get_pixel(const gfx_framebuffer_t *fb, int x, int y);
void gfx_set_pixel(gfx_framebuffer_t *fb, int x, int y, gfx_color_t color);
void gfx_fill_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, gfx_color_t color);
void gfx_draw_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, gfx_color_t color);
void gfx_fill_rounded_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, int radius, gfx_color_t color);
void gfx_draw_rounded_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, int radius, gfx_color_t color);
void gfx_draw_rounded_rect_thick(gfx_framebuffer_t *fb, int x, int y, int w, int h, int radius, int thickness, gfx_color_t color);
void gfx_draw_text(gfx_framebuffer_t *fb, int x, int y, const char *text, int scale, gfx_color_t color);

/**
 * Draw a 1-bpp bitmap onto the framebuffer.
 *
 * @param fb       The framebuffer to draw into.
 * @param data     Pointer to 1-bpp bitmap data (MSB first within each byte).
 * @param data_len Total number of bytes in the bitmap data.
 * @param stride   Number of bytes per row in the bitmap data.
 * @param x        Top-left X position on the framebuffer.
 * @param y        Top-left Y position on the framebuffer.
 * @param w        Width of the bitmap in pixels.
 * @param h        Height of the bitmap in pixels.
 * @param color    Color for set bits (unset bits are left unchanged / transparent).
 */
void gfx_draw_bitmap(gfx_framebuffer_t *fb, const uint8_t *data, int data_len,
                     int stride, int x, int y, int w, int h, gfx_color_t color);

#endif
