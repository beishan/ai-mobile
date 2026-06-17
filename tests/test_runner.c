#include <stdio.h>
#include <stdlib.h>
#include "gfx/gfx.h"

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ_INT(expected, actual) ASSERT_TRUE((expected) == (actual))

static void test_framebuffer_has_fixed_eink_size(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    ASSERT_EQ_INT(400, gfx_width(&fb));
    ASSERT_EQ_INT(300, gfx_height(&fb));
}

static void test_set_pixel_clips_out_of_bounds(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);
    gfx_set_pixel(&fb, -1, -1, GFX_RED);
    gfx_set_pixel(&fb, 400, 300, GFX_RED);
    gfx_set_pixel(&fb, 20, 30, GFX_BLACK);
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 0, 0));
    ASSERT_EQ_INT(GFX_BLACK, gfx_get_pixel(&fb, 20, 30));
}

static void test_rectangles_clip_and_place_red_pixels(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);
    gfx_fill_rect(&fb, 398, 298, 10, 10, GFX_RED);
    ASSERT_EQ_INT(GFX_RED, gfx_get_pixel(&fb, 399, 299));
    ASSERT_EQ_INT(GFX_WHITE, gfx_get_pixel(&fb, 397, 297));
}

int main(void) {
    test_framebuffer_has_fixed_eink_size();
    test_set_pixel_clips_out_of_bounds();
    test_rectangles_clip_and_place_red_pixels();
    puts("tests passed");
    return 0;
}
