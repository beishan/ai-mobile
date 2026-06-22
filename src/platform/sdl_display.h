#ifndef SDL_DISPLAY_H
#define SDL_DISPLAY_H

#include <SDL.h>

#include "app/app_state.h"
#include "gfx/gfx.h"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int scale;
    int refresh_count;
} sdl_display_t;

int sdl_display_init(sdl_display_t *display, const char *title, int scale);
void sdl_display_shutdown(sdl_display_t *display);
int sdl_display_present(sdl_display_t *display, const gfx_framebuffer_t *fb);
int sdl_display_poll_button(app_button_t *button, int *quit);
app_button_t sdl_display_button_from_key(SDL_Keycode key);

#endif
