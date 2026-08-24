#include "platform/epd_frame.h"

int epd_frame_pack(const gfx_framebuffer_t *fb, epd_frame_t *frame) {
    if (fb == NULL || frame == NULL) {
        return -1;
    }

#if EPD_NATIVE_PORTRAIT
    /* HINK is natively portrait.  The application keeps its original
     * 480x800 logical canvas; sample it into the 240x416 panel so every page
     * remains visible without maintaining a second set of UI layouts. */
    for (int native_y = 0; native_y < EPD_NATIVE_HEIGHT; native_y++) {
        int logical_y = native_y * GFX_HEIGHT / EPD_NATIVE_HEIGHT;
        int row_byte_offset = native_y * EPD_NATIVE_WIDTH / 8;
        for (int native_x = 0; native_x < EPD_NATIVE_WIDTH; native_x += 8) {
            unsigned char packed = 0xff;
            for (int bit = 0; bit < 8; bit++) {
                int logical_x = (native_x + bit) * GFX_WIDTH / EPD_NATIVE_WIDTH;
                if ((gfx_color_t)fb->pixels[logical_y][logical_x] == GFX_BLACK) {
                    packed &= (unsigned char)~(0x80u >> bit);
                }
            }
            frame->bw[row_byte_offset + native_x / 8] = packed;
        }
    }
#else
    /*
     * The UI is 480x800 portrait while SSD1677 RAM is 800x480 landscape.
     * The physical panel is mounted 180 degrees from the original mapping.
     * Use the equivalent of Adafruit_GFX rotation=3:
     * physical_x=logical_y, physical_y=479-logical_x.
     * SSD1677's reversed gate entry mode performs the panel-side reversal,
     * so bytes stay in native framebuffer row order.
     */
    for (int output_row = 0; output_row < EPD_NATIVE_HEIGHT; output_row++) {
        int physical_y = output_row;
        int row_byte_offset = output_row * EPD_NATIVE_WIDTH / 8;
        for (int physical_x = 0; physical_x < EPD_NATIVE_WIDTH; physical_x += 8) {
            int logical_x = EPD_NATIVE_HEIGHT - 1 - physical_y;
            unsigned char packed = 0xff;
            for (int bit = 0; bit < 8; bit++) {
                if ((gfx_color_t)fb->pixels[physical_x + bit][logical_x] == GFX_BLACK) {
                    packed &= (unsigned char)~(0x80u >> bit);
                }
            }
            frame->bw[row_byte_offset + physical_x / 8] = packed;
        }
    }
#endif

    return 0;
}

int epd_frame_pack_partial(const gfx_framebuffer_t *fb, epd_frame_t *frame,
                           int native_x, int native_y,
                           int native_width, int native_height) {
    if (fb == NULL || frame == NULL ||
        native_x < 0 || native_y < 0 ||
        native_width <= 0 || native_height <= 0 ||
        native_x + native_width > EPD_NATIVE_WIDTH ||
        native_y + native_height > EPD_NATIVE_HEIGHT ||
        (native_x & 7) != 0 || (native_width & 7) != 0) {
        return -1;
    }

#if EPD_NATIVE_PORTRAIT
    for (int physical_y = native_y;
         physical_y < native_y + native_height;
         physical_y++) {
        int logical_y = physical_y * GFX_HEIGHT / EPD_NATIVE_HEIGHT;
        int row_byte_offset = physical_y * EPD_NATIVE_WIDTH / 8;
        for (int physical_x = native_x;
             physical_x < native_x + native_width;
             physical_x += 8) {
            unsigned char packed = 0xff;
            for (int bit = 0; bit < 8; bit++) {
                int logical_x = (physical_x + bit) * GFX_WIDTH / EPD_NATIVE_WIDTH;
                if ((gfx_color_t)fb->pixels[logical_y][logical_x] == GFX_BLACK) {
                    packed &= (unsigned char)~(0x80u >> bit);
                }
            }
            frame->bw[row_byte_offset + physical_x / 8] = packed;
        }
    }
#else
    for (int physical_y = native_y;
         physical_y < native_y + native_height;
         physical_y++) {
        int logical_x = EPD_NATIVE_HEIGHT - 1 - physical_y;
        int row_byte_offset = physical_y * EPD_NATIVE_WIDTH / 8;
        for (int physical_x = native_x;
             physical_x < native_x + native_width;
             physical_x += 8) {
            unsigned char packed = 0xff;
            for (int bit = 0; bit < 8; bit++) {
                if ((gfx_color_t)fb->pixels[physical_x + bit][logical_x] == GFX_BLACK) {
                    packed &= (unsigned char)~(0x80u >> bit);
                }
            }
            frame->bw[row_byte_offset + physical_x / 8] = packed;
        }
    }
#endif
    return 0;
}

int epd_frame_diff_bounds(const epd_frame_t *previous, const epd_frame_t *current,
                          int *native_x, int *native_y,
                          int *native_width, int *native_height,
                          size_t *changed_bytes) {
    const int row_bytes = EPD_NATIVE_WIDTH / 8;
    int min_byte_x = row_bytes;
    int max_byte_x = -1;
    int min_y = EPD_NATIVE_HEIGHT;
    int max_y = -1;
    size_t changed = 0;

    if (previous == NULL || current == NULL ||
        native_x == NULL || native_y == NULL ||
        native_width == NULL || native_height == NULL) {
        return -1;
    }

    for (int y = 0; y < EPD_NATIVE_HEIGHT; y++) {
        int row_offset = y * row_bytes;
        for (int byte_x = 0; byte_x < row_bytes; byte_x++) {
            int offset = row_offset + byte_x;
            if (previous->bw[offset] == current->bw[offset]) {
                continue;
            }
            changed++;
            if (byte_x < min_byte_x) min_byte_x = byte_x;
            if (byte_x > max_byte_x) max_byte_x = byte_x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }

    if (changed_bytes != NULL) {
        *changed_bytes = changed;
    }
    if (changed == 0) {
        *native_x = 0;
        *native_y = 0;
        *native_width = 0;
        *native_height = 0;
        return 0;
    }

    *native_x = min_byte_x * 8;
    *native_y = min_y;
    *native_width = (max_byte_x - min_byte_x + 1) * 8;
    *native_height = max_y - min_y + 1;
    return 1;
}
