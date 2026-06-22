#include "ui/icons.h"

static void px(gfx_framebuffer_t *fb, int x, int y, int w, int h, gfx_color_t color) {
    gfx_fill_rect(fb, x, y, w, h, color);
}

static void icon_reader(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x + 5, y + 8, 18, 28, GFX_BLACK);
    gfx_draw_rect(fb, x + 25, y + 8, 18, 28, GFX_BLACK);
    px(fb, x + 23, y + 10, 2, 28, GFX_BLACK);
    px(fb, x + 10, y + 14, 9, 2, GFX_BLACK);
    px(fb, x + 10, y + 20, 9, 2, GFX_BLACK);
    px(fb, x + 30, y + 14, 9, 2, GFX_BLACK);
    px(fb, x + 30, y + 20, 9, 2, GFX_BLACK);
    px(fb, x + 34, y + 8, 5, 12, GFX_BLACK);
}

static void icon_weather(gfx_framebuffer_t *fb, int x, int y) {
    px(fb, x + 9, y + 10, 12, 12, GFX_BLACK);
    px(fb, x + 13, y + 6, 4, 4, GFX_BLACK);
    px(fb, x + 4, y + 14, 4, 4, GFX_BLACK);
    px(fb, x + 22, y + 14, 4, 4, GFX_BLACK);
    px(fb, x + 15, y + 24, 25, 10, GFX_BLACK);
    px(fb, x + 21, y + 18, 14, 16, GFX_BLACK);
    px(fb, x + 28, y + 21, 13, 13, GFX_BLACK);
}

static void icon_calendar(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x + 8, y + 6, 32, 36, GFX_BLACK);
    px(fb, x + 8, y + 6, 32, 8, GFX_BLACK);
    px(fb, x + 14, y + 22, 5, 5, GFX_BLACK);
    px(fb, x + 24, y + 22, 5, 5, GFX_BLACK);
    px(fb, x + 34, y + 22, 5, 5, GFX_BLACK);
    px(fb, x + 24, y + 32, 8, 8, GFX_BLACK);
}

static void icon_english(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x + 8, y + 8, 32, 32, GFX_BLACK);
    px(fb, x + 16, y + 28, 16, 4, GFX_BLACK);
    px(fb, x + 19, y + 16, 4, 16, GFX_BLACK);
    px(fb, x + 27, y + 16, 4, 16, GFX_BLACK);
    px(fb, x + 20, y + 16, 10, 4, GFX_BLACK);
    px(fb, x + 32, y + 8, 8, 10, GFX_BLACK);
}

static void icon_settings(gfx_framebuffer_t *fb, int x, int y) {
    px(fb, x + 21, y + 7, 6, 34, GFX_BLACK);
    px(fb, x + 7, y + 21, 34, 6, GFX_BLACK);
    px(fb, x + 13, y + 13, 22, 22, GFX_BLACK);
    px(fb, x + 18, y + 18, 12, 12, GFX_WHITE);
    gfx_draw_rect(fb, x + 17, y + 17, 14, 14, GFX_BLACK);
}

static void icon_about(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x + 10, y + 8, 28, 32, GFX_BLACK);
    px(fb, x + 22, y + 14, 5, 5, GFX_BLACK);
    px(fb, x + 22, y + 23, 5, 12, GFX_BLACK);
    px(fb, x + 18, y + 35, 13, 3, GFX_BLACK);
}

void ui_draw_icon(gfx_framebuffer_t *fb, ui_icon_kind_t kind, int x, int y, int selected) {
    if (selected) {
        gfx_fill_rect(fb, x - 3, y - 3, 54, 54, GFX_BLACK);
        gfx_fill_rect(fb, x, y, 48, 48, GFX_WHITE);
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
