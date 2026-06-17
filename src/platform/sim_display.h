#ifndef SIM_DISPLAY_H
#define SIM_DISPLAY_H

#include "gfx/gfx.h"

typedef struct {
    const char *path;
    int refresh_count;
} sim_display_t;

void sim_display_init(sim_display_t *display, const char *path);
int sim_display_commit(sim_display_t *display, const gfx_framebuffer_t *fb);
int sim_display_refresh_count(const sim_display_t *display);

#endif
