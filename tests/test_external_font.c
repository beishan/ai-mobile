#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font/font.h"
#include "gfx/gfx.h"
#include "platform/sim_display.h"

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ_INT(expected, actual) ASSERT_TRUE((expected) == (actual))

static int count_black_pixels(gfx_framebuffer_t *fb) {
    int count = 0;
    for (int y = 0; y < gfx_height(fb); y++) {
        for (int x = 0; x < gfx_width(fb); x++) {
            if (gfx_get_pixel(fb, x, y) == GFX_BLACK) {
                count++;
            }
        }
    }
    return count;
}

static void test_load_hanyi_tangmei(void) {
    external_font_t *efont = external_font_create();
    ASSERT_TRUE(efont != NULL);

    int result = external_font_load_file(efont, "assets/fonts/external/20 21×27 汉仪唐美人.bin");
    ASSERT_EQ_INT(0, result);
    ASSERT_TRUE(efont->loaded == 1);
    ASSERT_EQ_INT(21, efont->width);
    ASSERT_EQ_INT(27, efont->height);
    ASSERT_TRUE(efont->data != NULL);
    ASSERT_TRUE(efont->bytes_per_glyph > 0);

    printf("  Loaded: %s (%dx%d, %d bytes/glyph)\n",
           efont->name, efont->width, efont->height, efont->bytes_per_glyph);

    external_font_free(efont);
    printf("  test_load_hanyi_tangmei: PASS\n");
}

static void test_load_all_fonts(void) {
    const char *fonts[] = {
        "assets/fonts/external/20 21×27 汉仪唐美人.bin",
        "assets/fonts/external/20 24×27 方正大黑.bin",
        "assets/fonts/external/22 22×29 汉仪正圆.bin",
        "assets/fonts/external/24 26×32 更纱黑体.bin",
        "assets/fonts/external/32 38×43 中宋.bin",
        "assets/fonts/external/34 39×45 方正兰亭圆.bin",
        "assets/fonts/external/36 43×48 方正兰亭圆.bin",
    };
    int count = sizeof(fonts) / sizeof(fonts[0]);

    for (int i = 0; i < count; i++) {
        external_font_t *efont = external_font_create();
        int result = external_font_load_file(efont, fonts[i]);
        ASSERT_EQ_INT(0, result);
        ASSERT_TRUE(efont->loaded == 1);
        ASSERT_TRUE(efont->width > 0);
        ASSERT_TRUE(efont->height > 0);
        ASSERT_TRUE(efont->data != NULL);
        printf("  [%d] %s (%dx%d)\n", i, efont->name, efont->width, efont->height);
        external_font_free(efont);
    }
    printf("  test_load_all_fonts: PASS\n");
}

static void test_render_chinese_text(void) {
    external_font_t *efont = external_font_create();
    int result = external_font_load_file(efont, "assets/fonts/external/20 21×27 汉仪唐美人.bin");
    ASSERT_EQ_INT(0, result);

    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);

    /* Draw Chinese text */
    external_font_draw_text(efont, &fb, 50, 100, "三体 百年孤独 活着", GFX_BLACK);

    int black_count = count_black_pixels(&fb);
    ASSERT_TRUE(black_count > 100);
    printf("  Rendered text with %d black pixels\n", black_count);

    /* Save for visual inspection */
    sim_display_t display;
    sim_display_init(&display, "out/test_external_font_render.ppm");
    sim_display_commit(&display, &fb);

    external_font_free(efont);
    printf("  test_render_chinese_text: PASS\n");
}

static void test_render_multiple_fonts(void) {
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);
    sim_display_t display;

    const char *fonts[] = {
        "assets/fonts/external/20 21×27 汉仪唐美人.bin",
        "assets/fonts/external/20 24×27 方正大黑.bin",
        "assets/fonts/external/22 22×29 汉仪正圆.bin",
        "assets/fonts/external/24 26×32 更纱黑体.bin",
    };

    int y = 50;
    for (int i = 0; i < 4; i++) {
        external_font_t *efont = external_font_create();
        int result = external_font_load_file(efont, fonts[i]);
        ASSERT_EQ_INT(0, result);

        external_font_draw_text(efont, &fb, 50, y, "汉仪唐美人 方正大黑 汉仪正圆 更纱黑体", GFX_BLACK);
        y += efont->height + 20;

        external_font_free(efont);
    }

    int black_count = count_black_pixels(&fb);
    ASSERT_TRUE(black_count > 200);
    printf("  Rendered %d black pixels across 4 fonts\n", black_count);

    sim_display_init(&display, "out/test_external_font_multi.ppm");
    sim_display_commit(&display, &fb);

    printf("  test_render_multiple_fonts: PASS\n");
}

static void test_measure_text(void) {
    external_font_t *efont = external_font_create();
    int result = external_font_load_file(efont, "assets/fonts/external/20 21×27 汉仪唐美人.bin");
    ASSERT_EQ_INT(0, result);

    /* "三体" = 2 chars, each 21px wide = 42px */
    int width = external_font_measure_text(efont, "三体");
    ASSERT_EQ_INT(42, width);

    /* "三体 百年孤独" = 7 chars (including space) = 147px */
    width = external_font_measure_text(efont, "三体 百年孤独");
    ASSERT_EQ_INT(147, width);

    /* Empty string */
    width = external_font_measure_text(efont, "");
    ASSERT_EQ_INT(0, width);

    external_font_free(efont);
    printf("  test_measure_text: PASS\n");
}

static void test_aligned_text(void) {
    external_font_t *efont = external_font_create();
    int result = external_font_load_file(efont, "assets/fonts/external/20 21×27 汉仪唐美人.bin");
    ASSERT_EQ_INT(0, result);

    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);

    /* Center-aligned text in a 432px wide area */
    external_font_draw_text_aligned(efont, &fb, 24, 100, 432, "三体", FONT_ALIGN_CENTER, GFX_BLACK);

    /* "三体" = 42px wide, centered in 432px: x = 24 + (432-42)/2 = 24 + 195 = 219 */
    /* Check that pixels are drawn around x=219 */
    int found_pixel = 0;
    for (int x = 210; x < 270; x++) {
        if (gfx_get_pixel(&fb, x, 108) == GFX_BLACK) {
            found_pixel = 1;
            break;
        }
    }
    ASSERT_TRUE(found_pixel);

    external_font_free(efont);
    printf("  test_aligned_text: PASS\n");
}

static void test_font_manager(void) {
    /* Load all fonts from the external directory */
    int count = font_manager_load_dir("assets/fonts/external");
    ASSERT_TRUE(count > 0);
    ASSERT_TRUE(font_manager_has_external(20) || font_manager_has_external(22) || font_manager_has_external(24));

    /* Test getting a font by size */
    const external_font_t *efont = font_manager_get(20);
    ASSERT_TRUE(efont != NULL);
    ASSERT_TRUE(efont->loaded == 1);
    printf("  Font manager loaded %d fonts\n", count);
    printf("  Font for size 20: %s (%dx%d)\n", efont->name, efont->width, efont->height);

    /* Test unified auto-drawing */
    gfx_framebuffer_t fb;
    gfx_init(&fb);
    gfx_clear(&fb, GFX_WHITE);

    font_draw_text_auto(20, &fb, 50, 50, "三体", GFX_BLACK);
    font_draw_text_aligned_auto(20, &fb, 0, 100, 480, "百年孤独", FONT_ALIGN_CENTER, GFX_BLACK);

    int black_count = count_black_pixels(&fb);
    ASSERT_TRUE(black_count > 50);

    /* Test measure text auto */
    int w = font_measure_text_auto(20, "三体");
    ASSERT_TRUE(w > 0);

    /* Save for visual inspection */
    sim_display_t display;
    sim_display_init(&display, "out/test_font_manager.ppm");
    sim_display_commit(&display, &fb);

    font_manager_free_all();
    printf("  test_font_manager: PASS\n");
}

int main(void) {
    printf("=== External Font Tests ===\n\n");

    test_load_hanyi_tangmei();
    test_load_all_fonts();
    test_render_chinese_text();
    test_render_multiple_fonts();
    test_measure_text();
    test_aligned_text();
    test_font_manager();

    printf("\nAll external font tests passed!\n");
    return 0;
}
