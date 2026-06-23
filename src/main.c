#include <stdio.h>
#include <string.h>

#include "app/app_persistence.h"
#include "app/app_state.h"
#include "app/reader_library.h"
#include "font/font.h"
#include "gfx/gfx.h"
#include "platform/sim_display.h"
#include "ui/pages.h"

#define APP_STATE_PATH "out/app_state.txt"

static int parse_button(const char *line, app_button_t *button) {
    if (line == NULL || button == NULL) {
        return 0;
    }

    if (line[0] == '\n' || line[0] == 'h') {
        *button = APP_BUTTON_HOME;
        return 1;
    }
    if (line[0] == 'w') {
        *button = APP_BUTTON_UP;
        return 1;
    }
    if (line[0] == 's') {
        *button = APP_BUTTON_DOWN;
        return 1;
    }
    if (line[0] == 'p') {
        *button = APP_BUTTON_POWER;
        return 1;
    }

    return 0;
}

static int render_and_commit(gfx_framebuffer_t *fb, sim_display_t *display, const app_state_t *app, const font_t *font) {
    ui_render_page(fb, app, font);
    if (sim_display_commit(display, fb) != 0) {
        fputs("failed to write out/frame.ppm\n", stderr);
        return 1;
    }

    printf("page=%s refresh=%d frame=out/frame.ppm\n",
           app_page_name(app->page),
           sim_display_refresh_count(display));
    return 0;
}

int main(void) {
    app_state_t app;
    gfx_framebuffer_t fb;
    sim_display_t display;
    font_t font;
    char line[32];

    (void)reader_library_load_book_file(0, "assets/books/santi.txt");
    app_init(&app);
    (void)app_persistence_load_app_file(APP_STATE_PATH, &app);
    gfx_init(&fb);
    if (!font_load_default(&font)) {
        fputs("failed to load default font\n", stderr);
        return 1;
    }
    sim_display_init(&display, "out/frame.ppm");

    puts("reader_sim controls: w=up s=down h/enter=home p=power q=quit");
    if (render_and_commit(&fb, &display, &app, &font) != 0) {
        font_free(&font);
        return 1;
    }

    while (fgets(line, sizeof(line), stdin) != NULL) {
        app_button_t button;
        if (line[0] == 'q') {
            (void)app_persistence_save_app_file(APP_STATE_PATH, &app);
            puts("bye");
            font_free(&font);
            return 0;
        }

        if (!parse_button(line, &button)) {
            puts("ignored input");
            continue;
        }

        app_handle_button(&app, button);
        (void)app_persistence_save_app_file(APP_STATE_PATH, &app);
        if (render_and_commit(&fb, &display, &app, &font) != 0) {
            font_free(&font);
            return 1;
        }
    }

    (void)app_persistence_save_app_file(APP_STATE_PATH, &app);
    font_free(&font);
    return 0;
}
