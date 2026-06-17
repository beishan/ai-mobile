#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#define GFX_WIDTH 400
#define GFX_HEIGHT 300

typedef enum {
    GFX_WHITE = 0,
    GFX_BLACK = 1,
    GFX_RED = 2
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
void gfx_draw_text(gfx_framebuffer_t *fb, int x, int y, const char *text, int scale, gfx_color_t color);

#endif
