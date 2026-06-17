#include <stdio.h>
#include <string.h>

#include "app/app_state.h"
#include "gfx/gfx.h"
#include "platform/sim_display.h"
#include "ui/pages.h"

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

static int render_and_commit(gfx_framebuffer_t *fb, sim_display_t *display, const app_state_t *app) {
    ui_render_page(fb, app);
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
    char line[32];

    app_init(&app);
    gfx_init(&fb);
    sim_display_init(&display, "out/frame.ppm");

    puts("reader_sim controls: w=up s=down h/enter=home p=power q=quit");
    if (render_and_commit(&fb, &display, &app) != 0) {
        return 1;
    }

    while (fgets(line, sizeof(line), stdin) != NULL) {
        app_button_t button;
        if (line[0] == 'q') {
            puts("bye");
            return 0;
        }

        if (!parse_button(line, &button)) {
            puts("ignored input");
            continue;
        }

        app_handle_button(&app, button);
        if (render_and_commit(&fb, &display, &app) != 0) {
            return 1;
        }
    }

    return 0;
}
