#include "ui/page_simple.h"

#include <stdio.h>

#define PAGE_MARGIN_X 24
#define CONTENT_WIDTH (GFX_WIDTH - 2 * PAGE_MARGIN_X)
#define BODY_TOP 40

void ui_render_english_page(gfx_framebuffer_t *fb, const app_state_t *app,
                            const font_t *font,
                            ui_page_title_renderer_t render_title) {
    const font_face_t *normal = font_get_face(FONT_SIZE_22);
    const font_face_t *small = font_get_face(FONT_SIZE_18);
    const font_face_t *big = font_get_face(FONT_SIZE_24);
    const char *words[] = {"SERENDIPITY", "KINDLE", "PAPER"};
    const char *sounds[] = {"/sound/", "/kindl/", "/peiper/"};
    const char *meanings[] = {"释义 机缘巧合", "释义 电子阅读器", "释义 纸张"};
    const char *examples[] = {"例句 A happy discovery.", "例句 Read on Kindle.", "例句 Turn the paper."};
    char progress[24];
    char stats[48];

    if (fb == NULL || app == NULL || font == NULL || render_title == NULL) {
        return;
    }
    snprintf(progress, sizeof(progress), "%d/3", app->english_word + 1);
    render_title(fb, font, "英语学习", progress);

    {
        int card_y = BODY_TOP + 20;
        int card_h = 240;
        gfx_draw_rounded_rect_thick(fb, PAGE_MARGIN_X, card_y,
                                    CONTENT_WIDTH, card_h, 10, 2, GFX_BLACK);
        font_draw_text_aligned(big, fb, PAGE_MARGIN_X, card_y + 60,
                               CONTENT_WIDTH, words[app->english_word],
                               FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, card_y + 150,
                               CONTENT_WIDTH, sounds[app->english_word],
                               FONT_ALIGN_CENTER, GFX_BLACK);
    }

    snprintf(stats, sizeof(stats), "认识%d 复习%d",
             app->english_known_count, app->english_review_count);
    font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 300,
                           CONTENT_WIDTH, stats, FONT_ALIGN_CENTER, GFX_BLACK);

    if (app->english_show_back) {
        font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 380,
                               CONTENT_WIDTH, meanings[app->english_word],
                               FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 440,
                               CONTENT_WIDTH, examples[app->english_word],
                               FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 560,
                               CONTENT_WIDTH, "上键不认识  下键认识",
                               FONT_ALIGN_CENTER, GFX_BLACK);
    } else {
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 480,
                               CONTENT_WIDTH, "HOME 翻转查看释义",
                               FONT_ALIGN_CENTER, GFX_BLACK);
    }

    {
        const int dot_w = 14;
        const int dot_gap = 24;
        const int total_w = 3 * dot_w + 2 * dot_gap;
        const int start_x = (GFX_WIDTH - total_w) / 2;
        const int dot_y = BODY_TOP + 640;
        for (int i = 0; i < 3; i++) {
            int x = start_x + i * (dot_w + dot_gap);
            gfx_fill_rect(fb, x, dot_y, dot_w, dot_w, GFX_BLACK);
            if (app->english_answer_state[i] == 2) {
                gfx_draw_rect(fb, x - 2, dot_y - 2,
                              dot_w + 4, dot_w + 4, GFX_BLACK);
            }
        }
    }
}

void ui_render_about_page(gfx_framebuffer_t *fb, const font_t *font,
                          ui_page_title_renderer_t render_title) {
    const font_face_t *title = font_get_face(FONT_SIZE_24);
    const font_face_t *normal = font_get_face(FONT_SIZE_20);
    const font_face_t *small = font_get_face(FONT_SIZE_18);

    if (fb == NULL || font == NULL || render_title == NULL) {
        return;
    }
    render_title(fb, font, "关于", "");
    font_draw_text_aligned(title, fb, PAGE_MARGIN_X, BODY_TOP + 40,
                           CONTENT_WIDTH, "ESP32 墨水屏阅读器",
                           FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 160,
                           CONTENT_WIDTH, "固件版本 SIM V0",
                           FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 240,
                           CONTENT_WIDTH, "芯片型号 ESP32-S3 N16R8",
                           FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 320,
                           CONTENT_WIDTH, "Flash 16MB  PSRAM 8MB",
                           FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 440,
                           CONTENT_WIDTH, "4.26寸 480X800 黑白高刷",
                           FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 520,
                           CONTENT_WIDTH, "SSD1677 SPI",
                           FONT_ALIGN_CENTER, GFX_BLACK);
}
