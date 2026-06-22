#ifndef EPD_FRAME_H
#define EPD_FRAME_H

#include "gfx/gfx.h"

#define EPD_FRAME_BYTES ((GFX_WIDTH * GFX_HEIGHT) / 8)

typedef struct {
    unsigned char bw[EPD_FRAME_BYTES];
} epd_frame_t;

int epd_frame_pack(const gfx_framebuffer_t *fb, epd_frame_t *frame);

#endif
