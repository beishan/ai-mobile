#include "platform/epd_frame.h"

#include <string.h>

int epd_frame_pack(const gfx_framebuffer_t *fb, epd_frame_t *frame) {
    if (fb == NULL || frame == NULL) {
        return -1;
    }

    memset(frame->black, 0xff, sizeof(frame->black));
    memset(frame->red, 0xff, sizeof(frame->red));

    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x++) {
            int offset = y * GFX_WIDTH + x;
            int byte_index = offset / 8;
            unsigned char mask = (unsigned char)(0x80u >> (offset % 8));
            gfx_color_t pixel = gfx_get_pixel(fb, x, y);

            if (pixel == GFX_BLACK) {
                frame->black[byte_index] &= (unsigned char)~mask;
            } else if (pixel == GFX_RED) {
                frame->red[byte_index] &= (unsigned char)~mask;
            }
        }
    }

    return 0;
}
