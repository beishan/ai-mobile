#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "gfx/gfx.h"

typedef enum {
    UI_ICON_READER = 0,
    UI_ICON_WEATHER,
    UI_ICON_CALENDAR,
    UI_ICON_GAME,
    UI_ICON_ENGLISH,
    UI_ICON_SETTINGS,
    UI_ICON_ABOUT
} ui_icon_kind_t;

void ui_draw_icon(gfx_framebuffer_t *fb, ui_icon_kind_t kind, int x, int y, int selected);

#endif
