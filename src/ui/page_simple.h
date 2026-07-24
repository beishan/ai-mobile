#ifndef UI_PAGE_SIMPLE_H
#define UI_PAGE_SIMPLE_H

#include "app/app_state.h"
#include "font/font.h"
#include "gfx/gfx.h"

typedef void (*ui_page_title_renderer_t)(gfx_framebuffer_t *fb,
                                         const font_t *font,
                                         const char *left,
                                         const char *right);

void ui_render_english_page(gfx_framebuffer_t *fb, const app_state_t *app,
                            const font_t *font,
                            ui_page_title_renderer_t render_title);
void ui_render_about_page(gfx_framebuffer_t *fb, const font_t *font,
                          ui_page_title_renderer_t render_title);

#endif
