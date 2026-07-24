#include "ui/pages.h"

#include <stddef.h>

int ui_home_tile_bounds(int index, int *x, int *y, int *width, int *height) {
    const int body_top = 40;
    const int body_bottom = GFX_HEIGHT - 40;
    const int cols = 3;
    const int rows = 2;
    const int gap = 18;
    const int tile_w = 108;
    const int tile_h = 96;
    const int card_y = body_top + 8;
    const int grid_top = card_y + 120 + 24;
    const int total_w = cols * tile_w + (cols - 1) * gap;
    const int total_h = rows * tile_h + (rows - 1) * gap;
    const int start_x = (GFX_WIDTH - total_w) / 2;
    const int start_y = grid_top + (body_bottom - grid_top - total_h) / 2;
    int col;
    int row;

    if (index < 0 || index >= 6 || x == NULL || y == NULL ||
        width == NULL || height == NULL) {
        return -1;
    }
    col = index % cols;
    row = index / cols;
    *x = start_x + col * (tile_w + gap);
    *y = start_y + row * (tile_h + gap);
    *width = tile_w;
    *height = tile_h;
    return 0;
}
