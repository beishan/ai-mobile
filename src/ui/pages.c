#include "ui/pages.h"

#include <stdio.h>

static void title_bar(gfx_framebuffer_t *fb, const font_t *font, const char *left, const char *right) {
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, 24, GFX_BLACK);
    font_draw_text(font, fb, 8, 6, left, GFX_WHITE);
    if (right != NULL) {
        font_draw_text(font, fb, 304, 6, right, GFX_WHITE);
    }
}

static void label_box(gfx_framebuffer_t *fb, const font_t *font, int x, int y, int w, int h, const char *label, int selected) {
    gfx_color_t border = selected ? GFX_RED : GFX_BLACK;
    if (selected) {
        gfx_fill_rect(fb, x, y, w, h, GFX_RED);
        gfx_fill_rect(fb, x + 3, y + 3, w - 6, h - 6, GFX_WHITE);
    }
    gfx_draw_rect(fb, x, y, w, h, border);
    font_draw_text(font, fb, x + 18, y + 20, label, GFX_BLACK);
}

static void render_home(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const char *items[] = {"阅读", "天气", "日历", "游戏", "英语", "设置", "关于"};
    const int xs[] = {20, 145, 270, 20, 145, 270, 145};
    const int ys[] = {68, 68, 68, 158, 158, 158, 235};

    title_bar(fb, font, "14:35", "WiFi 78%");
    font_draw_text(font, fb, 96, 34, "晴 26C 北京", GFX_BLACK);
    gfx_fill_rect(fb, 0, 24, GFX_WIDTH, 18, GFX_RED);

    for (int i = 0; i < 7; i++) {
        label_box(fb, font, xs[i], ys[i], 105, 58, items[i], app->home_selection == i);
    }
}

static void render_bookshelf(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const char *titles[] = {"三体", "百年孤独", "活着"};
    const char *meta[] = {"刘慈欣 1.2MB 45%", "马尔克斯 0.8MB 12%", "余华 0.4MB 0%"};

    title_bar(fb, font, "书架", "3本");
    for (int i = 0; i < 3; i++) {
        int y = 44 + i * 72;
        if (app->bookshelf_selection == i) {
            gfx_fill_rect(fb, 12, y, 376, 58, GFX_RED);
            gfx_fill_rect(fb, 15, y + 3, 370, 52, GFX_WHITE);
        }
        gfx_draw_rect(fb, 12, y, 376, 58, GFX_BLACK);
        font_draw_text(font, fb, 25, y + 10, titles[i], GFX_BLACK);
        font_draw_text(font, fb, 25, y + 32, meta[i], GFX_BLACK);
        gfx_fill_rect(fb, 302, y + 18, 52, 12, GFX_BLACK);
        gfx_fill_rect(fb, 354, y + 18, 22, 12, GFX_WHITE);
        gfx_draw_rect(fb, 302, y + 18, 74, 12, GFX_BLACK);
    }
}

static void render_reader(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    char page[24];
    snprintf(page, sizeof(page), "%d/5", app->reader_page + 1);
    title_bar(fb, font, "三体 | 第三章", page);

    font_draw_text(font, fb, 24, 46, "这是一个宁静的夜晚。", GFX_BLACK);
    font_draw_text(font, fb, 24, 70, "城市的灯光在远处闪烁，", GFX_BLACK);
    font_draw_text(font, fb, 24, 94, "像一片低垂的星空。", GFX_BLACK);
    font_draw_text(font, fb, 24, 130, "他翻到下一页，等待墨水", GFX_BLACK);
    font_draw_text(font, fb, 24, 154, "屏缓慢刷新完成。", GFX_BLACK);

    font_draw_text(font, fb, 80, 236, "上键上一页  下键下一页", GFX_BLACK);
    gfx_draw_rect(fb, 20, 268, 360, 8, GFX_BLACK);
    gfx_fill_rect(fb, 20, 268, 72 + app->reader_page * 54, 8, GFX_RED);
}

static void render_weather(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    char refresh[24];
    snprintf(refresh, sizeof(refresh), "R%d", app->weather_refreshes);
    title_bar(fb, font, "天气", refresh);
    font_draw_text(font, fb, 170, 46, "北京", GFX_BLACK);
    font_draw_text(font, fb, 154, 74, "26C", GFX_BLACK);
    font_draw_text(font, fb, 104, 110, "晴转多云 湿度55%", GFX_BLACK);

    label_box(fb, font, 32, 150, 92, 60, "今天26", 0);
    label_box(fb, font, 154, 150, 92, 60, "明天23", 0);
    label_box(fb, font, 276, 150, 92, 60, "后天19", 0);
    font_draw_text(font, fb, 310, 180, "19", GFX_RED);

    gfx_draw_rect(fb, 42, 238, 316, 24, GFX_BLACK);
    gfx_fill_rect(fb, 42, 238, 180, 24, GFX_RED);
    font_draw_text(font, fb, 138, 244, "空气质量 良", GFX_BLACK);
}

static void render_calendar(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    char title[32];
    snprintf(title, sizeof(title), "2025年6月%+d", app->calendar_month_offset);
    title_bar(fb, font, title, "返回");

    const char *week[] = {"日", "一", "二", "三", "四", "五", "六"};
    for (int i = 0; i < 7; i++) {
        font_draw_text(font, fb, 42 + i * 48, 42, week[i], i == 0 || i == 6 ? GFX_RED : GFX_BLACK);
    }
    for (int day = 1; day <= 30; day++) {
        int col = (day + 5) % 7;
        int row = (day + 5) / 7;
        int x = 35 + col * 48;
        int y = 70 + row * 34;
        char label[4];
        snprintf(label, sizeof(label), "%d", day);
        if (day == 15) {
            gfx_fill_rect(fb, x - 8, y - 8, 30, 24, GFX_RED);
        }
        font_draw_text(font, fb, x, y, label, (col == 0 || col == 6) ? GFX_RED : GFX_BLACK);
    }
    font_draw_text(font, fb, 116, 262, "农历 五月十九", GFX_BLACK);
}

static void render_english(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const char *words[] = {"SERENDIPITY", "KINDLE", "PAPER"};
    title_bar(fb, font, "英语学习", "12/50");
    gfx_draw_rect(fb, 42, 66, 316, 92, GFX_RED);
    font_draw_text(font, fb, 92, 92, words[app->english_word], GFX_BLACK);
    font_draw_text(font, fb, 132, 132, "/sound/", GFX_RED);
    if (app->english_show_back) {
        font_draw_text(font, fb, 72, 190, "释义 机缘巧合", GFX_BLACK);
    } else {
        font_draw_text(font, fb, 96, 190, "HOME 翻转查看释义", GFX_BLACK);
    }
    gfx_fill_rect(fb, 165, 232, 10, 10, GFX_RED);
    gfx_fill_rect(fb, 188, 232, 10, 10, GFX_BLACK);
    gfx_fill_rect(fb, 211, 232, 10, 10, GFX_BLACK);
}

static void render_settings(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const char *rows[] = {"字体大小 20", "字体 宋体", "行间距 1.5", "WiFi 已连接", "城市 北京", "省电模式"};
    title_bar(fb, font, "设置", "");
    for (int i = 0; i < 6; i++) {
        int y = 38 + i * 39;
        if (app->settings_selection == i) {
            gfx_fill_rect(fb, 0, y, GFX_WIDTH, 32, GFX_RED);
            gfx_fill_rect(fb, 0, y + 2, GFX_WIDTH, 28, GFX_WHITE);
        }
        font_draw_text(font, fb, 22, y + 8, rows[i], GFX_BLACK);
        if (i == 5 && app->power_saving_enabled) {
            gfx_fill_rect(fb, 330, y + 6, 36, 20, GFX_RED);
        }
    }
}

static void render_snake(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    title_bar(fb, font, "贪吃蛇", "分:12");
    gfx_draw_rect(fb, 12, 34, 376, 220, GFX_BLACK);
    for (int i = 0; i < 5; i++) {
        gfx_fill_rect(fb, app->snake_x * 10 - i * 10, app->snake_y * 8, 10, 10, GFX_BLACK);
    }
    gfx_fill_rect(fb, 282, 124, 14, 14, GFX_RED);
    font_draw_text(font, fb, 54, 268, "上/下键移动 HOME转向", GFX_BLACK);
}

static void render_about(gfx_framebuffer_t *fb, const font_t *font) {
    title_bar(fb, font, "关于", "");
    font_draw_text(font, fb, 70, 70, "ESP32 墨水屏阅读器", GFX_BLACK);
    font_draw_text(font, fb, 70, 110, "固件版本 SIM V0", GFX_BLACK);
    font_draw_text(font, fb, 70, 150, "400X300 三色显示", GFX_RED);
}

void ui_render_page(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    if (fb == NULL || app == NULL || font == NULL) {
        return;
    }

    gfx_clear(fb, GFX_WHITE);
    switch (app->page) {
        case APP_PAGE_HOME:
            render_home(fb, app, font);
            break;
        case APP_PAGE_BOOKSHELF:
            render_bookshelf(fb, app, font);
            break;
        case APP_PAGE_READER:
            render_reader(fb, app, font);
            break;
        case APP_PAGE_WEATHER:
            render_weather(fb, app, font);
            break;
        case APP_PAGE_CALENDAR:
            render_calendar(fb, app, font);
            break;
        case APP_PAGE_ENGLISH:
            render_english(fb, app, font);
            break;
        case APP_PAGE_SETTINGS:
            render_settings(fb, app, font);
            break;
        case APP_PAGE_SNAKE:
            render_snake(fb, app, font);
            break;
        case APP_PAGE_ABOUT:
        default:
            render_about(fb, font);
            break;
    }
}
