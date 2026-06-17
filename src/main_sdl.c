#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "app/app_state.h"
#include "font/font.h"
#include "gfx/gfx.h"
#include "platform/sdl_display.h"
#include "ui/pages.h"

static int render_frame(sdl_display_t *display, gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    ui_render_page(fb, app, font);
    if (!sdl_display_present(display, fb)) {
        fputs("failed to present SDL frame\n", stderr);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    int smoke = argc > 1 && strcmp(argv[1], "--smoke") == 0;
    app_state_t app;
    gfx_framebuffer_t fb;
    font_t font;
    sdl_display_t display;

    app_init(&app);
    gfx_init(&fb);
    if (!font_load_default(&font)) {
        fputs("failed to load default font\n", stderr);
        return 1;
    }

    if (!sdl_display_init(&display, "ESP32 三色墨水屏阅读器", 3)) {
        fputs("failed to initialize SDL2 display\n", stderr);
        font_free(&font);
        return 1;
    }

    if (!render_frame(&display, &fb, &app, &font)) {
        sdl_display_shutdown(&display);
        font_free(&font);
        return 1;
    }

    if (smoke) {
        sdl_display_shutdown(&display);
        font_free(&font);
        return 0;
    }

    puts("SDL2 controls: Up/w Down/s Enter/Space/h Backspace/Esc/p q");
    for (;;) {
        app_button_t button = APP_BUTTON_HOME;
        int quit = 0;

        if (sdl_display_poll_button(&button, &quit)) {
            if (quit) {
                break;
            }
            app_handle_button(&app, button);
            if (!render_frame(&display, &fb, &app, &font)) {
                break;
            }
        }
        SDL_Delay(16);
    }

    sdl_display_shutdown(&display);
    font_free(&font);
    return 0;
}
