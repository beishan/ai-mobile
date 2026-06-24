#include "ui/icons.h"
#include "icons/icons_bitmap.h"

static void px(gfx_framebuffer_t *fb, int x, int y, int w, int h, gfx_color_t color) {
    gfx_fill_rect(fb, x, y, w, h, color);
}

static void icon_reader(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x + 6, y + 12, 24, 40, GFX_BLACK);
    gfx_draw_rect(fb, x + 34, y + 12, 24, 40, GFX_BLACK);
    px(fb, x + 31, y + 14, 3, 40, GFX_BLACK);
    px(fb, x + 12, y + 20, 13, 3, GFX_BLACK);
    px(fb, x + 12, y + 28, 13, 3, GFX_BLACK);
    px(fb, x + 12, y + 36, 10, 3, GFX_BLACK);
    px(fb, x + 40, y + 20, 13, 3, GFX_BLACK);
    px(fb, x + 40, y + 28, 13, 3, GFX_BLACK);
    px(fb, x + 44, y + 12, 7, 18, GFX_BLACK);
}

static void icon_weather(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x + 9, y + 11, 20, 20, GFX_BLACK);
    px(fb, x + 18, y + 4, 3, 7, GFX_BLACK);
    px(fb, x + 18, y + 31, 3, 7, GFX_BLACK);
    px(fb, x + 2, y + 20, 7, 3, GFX_BLACK);
    px(fb, x + 29, y + 20, 7, 3, GFX_BLACK);
    px(fb, x + 6, y + 8, 5, 3, GFX_BLACK);
    px(fb, x + 28, y + 8, 5, 3, GFX_BLACK);
    px(fb, x + 13, y + 44, 38, 4, GFX_BLACK);
    px(fb, x + 13, y + 36, 4, 10, GFX_BLACK);
    px(fb, x + 18, y + 32, 9, 4, GFX_BLACK);
    px(fb, x + 27, y + 28, 16, 4, GFX_BLACK);
    px(fb, x + 43, y + 32, 11, 4, GFX_BLACK);
    px(fb, x + 51, y + 36, 4, 10, GFX_BLACK);
    px(fb, x + 19, y + 48, 26, 3, GFX_BLACK);
}

static void icon_calendar(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x + 9, y + 7, 46, 50, GFX_BLACK);
    px(fb, x + 9, y + 16, 46, 3, GFX_BLACK);
    px(fb, x + 17, y + 3, 5, 12, GFX_BLACK);
    px(fb, x + 42, y + 3, 5, 12, GFX_BLACK);
    px(fb, x + 17, y + 27, 7, 7, GFX_BLACK);
    px(fb, x + 29, y + 27, 7, 7, GFX_BLACK);
    px(fb, x + 41, y + 27, 7, 7, GFX_BLACK);
    px(fb, x + 29, y + 40, 10, 10, GFX_BLACK);
}

static void icon_english(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x + 9, y + 8, 46, 46, GFX_BLACK);
    px(fb, x + 19, y + 39, 26, 5, GFX_BLACK);
    px(fb, x + 23, y + 20, 6, 24, GFX_BLACK);
    px(fb, x + 35, y + 20, 6, 24, GFX_BLACK);
    px(fb, x + 25, y + 20, 16, 5, GFX_BLACK);
    px(fb, x + 27, y + 30, 12, 4, GFX_BLACK);
    px(fb, x + 43, y + 8, 12, 14, GFX_BLACK);
}

static void icon_settings(gfx_framebuffer_t *fb, int x, int y) {
    px(fb, x + 11, y + 16, 42, 4, GFX_BLACK);
    px(fb, x + 11, y + 31, 42, 4, GFX_BLACK);
    px(fb, x + 11, y + 46, 42, 4, GFX_BLACK);
    gfx_draw_rect(fb, x + 20, y + 10, 12, 16, GFX_BLACK);
    gfx_draw_rect(fb, x + 36, y + 25, 12, 16, GFX_BLACK);
    gfx_draw_rect(fb, x + 16, y + 40, 12, 16, GFX_BLACK);
    px(fb, x + 23, y + 13, 6, 10, GFX_BLACK);
    px(fb, x + 39, y + 28, 6, 10, GFX_BLACK);
    px(fb, x + 19, y + 43, 6, 10, GFX_BLACK);
}

static void icon_about(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x + 14, y + 8, 36, 48, GFX_BLACK);
    px(fb, x + 29, y + 16, 7, 7, GFX_BLACK);
    px(fb, x + 29, y + 28, 7, 18, GFX_BLACK);
    px(fb, x + 23, y + 48, 19, 4, GFX_BLACK);
}

void ui_draw_icon(gfx_framebuffer_t *fb, ui_icon_kind_t kind, int x, int y, int selected) {
    if (selected) {
        gfx_draw_rect(fb, x - 3, y - 3, 54, 54, GFX_BLACK);
    }

    /* Prefer bitmap icon if available, fall back to procedural drawing. */
    if (ICON_HAS_BITMAP(kind)) {
        gfx_draw_bitmap(fb,
                         icon_bitmaps[(int)kind].bitmap,
                         ICON_BITMAP_TOTAL_BYTES,
                         ICON_BITMAP_BYTES_PER_ROW,
                         x, y,
                         ICON_BITMAP_SIZE, ICON_BITMAP_SIZE,
                         GFX_BLACK);
        return;
    }

    switch (kind) {
        case UI_ICON_READER:
            icon_reader(fb, x, y);
            break;
        case UI_ICON_WEATHER:
            icon_weather(fb, x, y);
            break;
        case UI_ICON_CALENDAR:
            icon_calendar(fb, x, y);
            break;
        case UI_ICON_ENGLISH:
            icon_english(fb, x, y);
            break;
        case UI_ICON_SETTINGS:
            icon_settings(fb, x, y);
            break;
        case UI_ICON_ABOUT:
        default:
            icon_about(fb, x, y);
            break;
    }
}
