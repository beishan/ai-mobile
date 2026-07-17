#ifndef EPD_FRAME_H
#define EPD_FRAME_H

#include "gfx/gfx.h"
#include "platform/ssd1677.h"

#define EPD_FRAME_BYTES SSD1677_FRAME_BYTES

typedef struct {
    unsigned char bw[EPD_FRAME_BYTES];
} epd_frame_t;

int epd_frame_pack(const gfx_framebuffer_t *fb, epd_frame_t *frame);

#endif
