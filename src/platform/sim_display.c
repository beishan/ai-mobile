#include "platform/sim_display.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static int ensure_parent_dir(const char *path) {
    const char *slash = strrchr(path, '/');

    if (slash == NULL) {
        return 0;
    }

    char dir[256];
    size_t len = (size_t)(slash - path);
    if (len >= sizeof(dir)) {
        return -1;
    }

    memcpy(dir, path, len);
    dir[len] = '\0';

    if (mkdir(dir, 0777) == 0 || errno == EEXIST) {
        return 0;
    }

    return -1;
}

static void color_to_rgb(gfx_color_t color, unsigned char rgb[3]) {
    switch (color) {
        case GFX_BLACK:
            rgb[0] = 35;
            rgb[1] = 35;
            rgb[2] = 32;
            break;
        case GFX_WHITE:
        default:
            rgb[0] = 245;
            rgb[1] = 244;
            rgb[2] = 236;
            break;
    }
}

void sim_display_init(sim_display_t *display, const char *path) {
    if (display == NULL) {
        return;
    }

    display->path = path;
    display->refresh_count = 0;
}

int sim_display_commit(sim_display_t *display, const gfx_framebuffer_t *fb) {
    if (display == NULL || fb == NULL || display->path == NULL) {
        return -1;
    }

    if (ensure_parent_dir(display->path) != 0) {
        return -1;
    }

    FILE *file = fopen(display->path, "wb");
    if (file == NULL) {
        return -1;
    }

    if (fprintf(file, "P6\n%d %d\n255\n", GFX_WIDTH, GFX_HEIGHT) < 0) {
        fclose(file);
        return -1;
    }

    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x++) {
            unsigned char rgb[3];
            color_to_rgb(gfx_get_pixel(fb, x, y), rgb);
            if (fwrite(rgb, 1, sizeof(rgb), file) != sizeof(rgb)) {
                fclose(file);
                return -1;
            }
        }
    }

    if (fclose(file) != 0) {
        return -1;
    }

    display->refresh_count++;
    return 0;
}

int sim_display_refresh_count(const sim_display_t *display) {
    if (display == NULL) {
        return 0;
    }

    return display->refresh_count;
}
