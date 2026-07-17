#include "platform/epd_frame.h"

#include <string.h>

int epd_frame_pack(const gfx_framebuffer_t *fb, epd_frame_t *frame) {
    if (fb == NULL || frame == NULL) {
        return -1;
    }

    memset(frame->bw, 0xff, sizeof(frame->bw));

    /*
     * The UI is 480x800 portrait while SSD1677 RAM is 800x480 landscape.
     * The physical panel is mounted 180 degrees from the original mapping.
     * Use the equivalent of Adafruit_GFX rotation=3:
     * physical_x=logical_y, physical_y=479-logical_x.
     * SSD1677's reversed gate entry mode performs the panel-side reversal,
     * so bytes stay in native framebuffer row order.
     */
    for (int output_row = 0; output_row < SSD1677_PANEL_HEIGHT; output_row++) {
        int physical_y = output_row;
        for (int physical_x = 0; physical_x < SSD1677_PANEL_WIDTH; physical_x++) {
            int logical_x = SSD1677_PANEL_HEIGHT - 1 - physical_y;
            int logical_y = physical_x;
            int offset = output_row * SSD1677_PANEL_WIDTH + physical_x;
            int byte_index = offset / 8;
            unsigned char mask = (unsigned char)(0x80u >> (offset % 8));
            gfx_color_t pixel = gfx_get_pixel(fb, logical_x, logical_y);

            if (pixel == GFX_BLACK) {
                frame->bw[byte_index] &= (unsigned char)~mask;
            }
        }
    }

    return 0;
}
