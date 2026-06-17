#include "platform/sdl_display.h"

#include <stdlib.h>

static Uint32 color_to_argb(gfx_color_t color) {
    switch (color) {
        case GFX_BLACK:
            return 0xff232320u;
        case GFX_RED:
            return 0xffe64840u;
        case GFX_WHITE:
        default:
            return 0xfff5f4ecu;
    }
}

app_button_t sdl_display_button_from_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP:
        case SDLK_w:
            return APP_BUTTON_UP;
        case SDLK_DOWN:
        case SDLK_s:
            return APP_BUTTON_DOWN;
        case SDLK_RETURN:
        case SDLK_SPACE:
        case SDLK_h:
            return APP_BUTTON_HOME;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
        case SDLK_p:
            return APP_BUTTON_POWER;
        default:
            return (app_button_t)-1;
    }
}

int sdl_display_init(sdl_display_t *display, const char *title, int scale) {
    if (display == NULL) {
        return 0;
    }

    display->window = NULL;
    display->renderer = NULL;
    display->texture = NULL;
    display->scale = scale > 0 ? scale : 3;
    display->refresh_count = 0;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 0;
    }

    display->window = SDL_CreateWindow(title,
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       GFX_WIDTH * display->scale,
                                       GFX_HEIGHT * display->scale,
                                       SDL_WINDOW_SHOWN);
    if (display->window == NULL) {
        sdl_display_shutdown(display);
        return 0;
    }

    display->renderer = SDL_CreateRenderer(display->window, -1, SDL_RENDERER_ACCELERATED);
    if (display->renderer == NULL) {
        display->renderer = SDL_CreateRenderer(display->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (display->renderer == NULL) {
        sdl_display_shutdown(display);
        return 0;
    }

    display->texture = SDL_CreateTexture(display->renderer,
                                         SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         GFX_WIDTH,
                                         GFX_HEIGHT);
    if (display->texture == NULL) {
        sdl_display_shutdown(display);
        return 0;
    }

    SDL_RenderSetLogicalSize(display->renderer, GFX_WIDTH, GFX_HEIGHT);
    return 1;
}

void sdl_display_shutdown(sdl_display_t *display) {
    if (display == NULL) {
        return;
    }

    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }
    if (display->renderer != NULL) {
        SDL_DestroyRenderer(display->renderer);
        display->renderer = NULL;
    }
    if (display->window != NULL) {
        SDL_DestroyWindow(display->window);
        display->window = NULL;
    }
    SDL_Quit();
}

int sdl_display_present(sdl_display_t *display, const gfx_framebuffer_t *fb) {
    void *pixels = NULL;
    int pitch = 0;

    if (display == NULL || fb == NULL || display->texture == NULL) {
        return 0;
    }

    if (SDL_LockTexture(display->texture, NULL, &pixels, &pitch) != 0) {
        return 0;
    }

    for (int y = 0; y < GFX_HEIGHT; y++) {
        Uint32 *row = (Uint32 *)((unsigned char *)pixels + y * pitch);
        for (int x = 0; x < GFX_WIDTH; x++) {
            row[x] = color_to_argb(gfx_get_pixel(fb, x, y));
        }
    }

    SDL_UnlockTexture(display->texture);
    SDL_RenderClear(display->renderer);
    SDL_RenderCopy(display->renderer, display->texture, NULL, NULL);
    SDL_RenderPresent(display->renderer);
    display->refresh_count++;
    return 1;
}

int sdl_display_poll_button(app_button_t *button, int *quit) {
    SDL_Event event;

    if (quit != NULL) {
        *quit = 0;
    }

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            if (quit != NULL) {
                *quit = 1;
            }
            return 1;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_q) {
                if (quit != NULL) {
                    *quit = 1;
                }
                return 1;
            }

            app_button_t mapped = sdl_display_button_from_key(event.key.keysym.sym);
            if ((int)mapped >= 0) {
                if (button != NULL) {
                    *button = mapped;
                }
                return 1;
            }
        }
    }

    return 0;
}
