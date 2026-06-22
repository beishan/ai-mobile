#include "ui/pages.h"
#include "ui/icons.h"

#include <stdio.h>

static void title_bar(gfx_framebuffer_t *fb, const font_t *font, const char *left, const char *right) {
    const font_face_t *small = font_get_face(FONT_SIZE_12);
    (void)font;
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, 24, GFX_BLACK);
    font_draw_text(small, fb, 8, 4, left, GFX_WHITE);
    if (right != NULL) {
        int width = font_measure_text(small, right);
        font_draw_text(small, fb, GFX_WIDTH - width - 8, 4, right, GFX_WHITE);
    }
}

static void draw_wifi_icon(gfx_framebuffer_t *fb, int x, int y, gfx_color_t color) {
    gfx_fill_rect(fb, x, y + 10, 3, 4, color);
    gfx_fill_rect(fb, x + 5, y + 7, 3, 7, color);
    gfx_fill_rect(fb, x + 10, y + 4, 3, 10, color);
}

static void draw_battery_icon(gfx_framebuffer_t *fb, int x, int y, int percent, gfx_color_t color) {
    int fill = percent * 20 / 100;
    gfx_draw_rect(fb, x, y + 3, 24, 12, color);
    gfx_fill_rect(fb, x + 24, y + 7, 2, 4, color);
    gfx_fill_rect(fb, x + 2, y + 5, fill, 8, color);
}

static void home_status_bar(gfx_framebuffer_t *fb, const font_t *font) {
    const font_face_t *small = font_get_face(FONT_SIZE_12);
    (void)font;
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, 24, GFX_BLACK);
    font_draw_text(small, fb, 8, 4, "14:35  晴 26C 北京", GFX_WHITE);
    draw_wifi_icon(fb, 310, 5, GFX_WHITE);
    draw_battery_icon(fb, 334, 4, 78, GFX_WHITE);
    font_draw_text(small, fb, 364, 4, "78%", GFX_WHITE);
}

static void app_tile(gfx_framebuffer_t *fb, const font_t *font, ui_icon_kind_t icon, int x, int y, const char *label, int selected) {
    const font_face_t *label_font = font_get_face(FONT_SIZE_14);
    (void)font;
    if (selected) {
        gfx_fill_rect(fb, x + 24, y + 76, 40, 4, GFX_RED);
        gfx_fill_rect(fb, x + 14, y + 20, 4, 24, GFX_RED);
    }
    ui_draw_icon(fb, icon, x + 20, y + 6, 0);
    font_draw_text_aligned(label_font, fb, x, y + 58, 88, label, FONT_ALIGN_CENTER, GFX_BLACK);
}

static void render_home(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const char *items[] = {"阅读", "天气", "日历", "游戏", "英语", "设置", "关于"};
    const ui_icon_kind_t icons[] = {
        UI_ICON_READER,
        UI_ICON_WEATHER,
        UI_ICON_CALENDAR,
        UI_ICON_GAME,
        UI_ICON_ENGLISH,
        UI_ICON_SETTINGS,
        UI_ICON_ABOUT
    };
    const int xs[] = {4, 103, 202, 301, 4, 103, 202};
    const int ys[] = {42, 42, 42, 42, 142, 142, 142};

    home_status_bar(fb, font);

    for (int i = 0; i < 7; i++) {
        app_tile(fb, font, icons[i], xs[i], ys[i], items[i], app->home_selection == i);
    }
}

static void render_bookshelf(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_16);
    const font_face_t *small = font_get_face(FONT_SIZE_14);
    const char *titles[] = {"三体", "百年孤独", "活着"};
    const char *authors[] = {"刘慈欣 1.2MB", "马尔克斯 0.8MB", "余华 0.4MB"};

    title_bar(fb, font, "书架", "3本");
    for (int i = 0; i < 3; i++) {
        int y = 44 + i * 72;
        int percent = (app->book_current_pages[i] + 1) * 100 / app->book_pages[i];
        int fill = percent * 74 / 100;
        char meta[48];
        snprintf(meta, sizeof(meta), "%s %d%%", authors[i], percent);
        if (app->bookshelf_selection == i) {
            gfx_fill_rect(fb, 12, y, 376, 58, GFX_RED);
            gfx_fill_rect(fb, 15, y + 3, 370, 52, GFX_WHITE);
        }
        gfx_draw_rect(fb, 12, y, 376, 58, GFX_BLACK);
        font_draw_text(normal, fb, 25, y + 8, titles[i], GFX_BLACK);
        font_draw_text(small, fb, 25, y + 32, meta, GFX_BLACK);
        gfx_fill_rect(fb, 302, y + 18, fill, 12, GFX_BLACK);
        gfx_draw_rect(fb, 302, y + 18, 74, 12, GFX_BLACK);
        if (app->recent_book == i) {
            gfx_fill_rect(fb, 302, y + 36, 38, 16, GFX_RED);
            font_draw_text(small, fb, 306, y + 38, "最近", GFX_WHITE);
        }
        if (app->book_bookmark_pages[i] >= 0) {
            gfx_fill_rect(fb, 366, y + 36, 8, 12, GFX_RED);
        }
    }
}

static const font_face_t *reader_body_font(const app_state_t *app) {
    const font_size_t sizes[] = {FONT_SIZE_16, FONT_SIZE_18, FONT_SIZE_20, FONT_SIZE_22, FONT_SIZE_24};
    int index = app->font_size_index;
    if (index < 0 || index > 4) {
        index = 2;
    }
    return font_get_face(sizes[index]);
}

static int reader_line_height(const app_state_t *app, const font_face_t *body) {
    const int extra[] = {2, 5, 8, 14};
    int index = app->line_spacing_index;
    if (index < 0 || index > 3) {
        index = 2;
    }
    return body->size + extra[index];
}

static void render_reader(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    char page[24];
    const font_face_t *body_font = reader_body_font(app);
    const char *titles[] = {"三体 | 第三章", "百年孤独 | 马孔多", "活着 | 田埂"};
    const char *bodies[] = {
        "这是一个宁静的夜晚。城市的灯光在远处闪烁，像一片低垂的星空。他翻到下一页，等待墨水屏缓慢刷新完成。",
        "多年以后，面对远方吹来的热风，他会想起那个潮湿而明亮的下午。小镇的钟声很慢，像从纸页深处传来。",
        "田野安静下来，风从麦穗上掠过。他把书合上又打开，仿佛那些旧日子仍然在黑白之间缓缓移动。"
    };
    snprintf(page, sizeof(page), "%d/%d", app->reader_page + 1, app->book_pages[app->current_book]);
    title_bar(fb, font, titles[app->current_book], page);

    font_draw_text_box_spaced(body_font, fb, 24, 46, 352, 154,
                              bodies[app->current_book],
                              reader_line_height(app, body_font),
                              GFX_BLACK);

    if (app->reader_menu_open) {
        const font_face_t *menu = font_get_face(FONT_SIZE_16);
        int has_bookmark = app->book_bookmark_pages[app->current_book] == app->reader_page;
        const char *items[] = {
            "继续阅读",
            "查看目录",
            has_bookmark ? "已加书签" : "添加书签",
            "退出到书架"
        };
        gfx_fill_rect(fb, 88, 64, 224, 138, GFX_WHITE);
        gfx_draw_rect(fb, 88, 64, 224, 138, GFX_BLACK);
        gfx_fill_rect(fb, 88, 64, 224, 4, GFX_RED);
        for (int i = 0; i < 4; i++) {
            int y = 82 + i * 28;
            gfx_color_t color = GFX_BLACK;
            if (app->reader_menu_selection == i) {
                gfx_fill_rect(fb, 104, y - 5, 192, 24, GFX_RED);
                color = GFX_WHITE;
            }
            font_draw_text(menu, fb, 124, y, items[i], color);
        }
        if (app->reader_catalog_open) {
            const char *chapters[] = {"第一章  开端", "第二章  转折", "第三章  回声"};
            gfx_fill_rect(fb, 66, 52, 268, 164, GFX_WHITE);
            gfx_draw_rect(fb, 66, 52, 268, 164, GFX_BLACK);
            gfx_fill_rect(fb, 66, 52, 268, 4, GFX_RED);
            font_draw_text(font_get_face(FONT_SIZE_14), fb, 86, 68, "目录", GFX_BLACK);
            for (int i = 0; i < 3; i++) {
                int y = 98 + i * 32;
                gfx_color_t color = GFX_BLACK;
                if (app->reader_catalog_selection == i) {
                    gfx_fill_rect(fb, 82, y - 5, 236, 24, GFX_RED);
                    color = GFX_WHITE;
                }
                font_draw_text(menu, fb, 98, y, chapters[i], color);
            }
        }
    }
}

static void render_weather(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_16);
    const font_face_t *small = font_get_face(FONT_SIZE_14);
    const font_face_t *big = font_get_face(FONT_SIZE_20);
    const char *cities[] = {"北京", "上海", "广州"};
    const char *conditions[] = {"晴转多云", "小雨", "多云"};
    const int temps[] = {26, 22, 29};
    const int humidities[] = {55, 72, 61};
    const int lows[] = {19, 17, 23};
    char refresh[24];
    char temp[12];
    char summary[48];
    char cache[48];
    int city = app->weather_city_index;
    if (city < 0 || city > 2) {
        city = 0;
    }
    snprintf(refresh, sizeof(refresh), "R%d", app->weather_refreshes);
    title_bar(fb, font, "天气", refresh);
    snprintf(temp, sizeof(temp), "%dC", temps[city]);
    snprintf(summary, sizeof(summary), "%s  湿度%d%%", conditions[city], humidities[city]);
    snprintf(cache, sizeof(cache), "%s %d分钟前", app->weather_stale ? "缓存" : "更新", app->weather_last_updated_minutes);
    font_draw_text_aligned(normal, fb, 0, 38, GFX_WIDTH, cities[city], FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(big, fb, 0, 64, GFX_WIDTH, temp, FONT_ALIGN_CENTER, app->weather_stale ? GFX_RED : GFX_BLACK);
    font_draw_text_aligned(small, fb, 0, 102, GFX_WIDTH, summary, FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(small, fb, 0, 126, GFX_WIDTH, cache, FONT_ALIGN_CENTER, app->weather_stale ? GFX_RED : GFX_BLACK);

    gfx_draw_rect(fb, 32, 150, 92, 60, GFX_BLACK);
    gfx_draw_rect(fb, 154, 150, 92, 60, GFX_BLACK);
    gfx_draw_rect(fb, 276, 150, 92, 60, GFX_BLACK);
    snprintf(summary, sizeof(summary), "今天%d", temps[city]);
    font_draw_text_aligned(small, fb, 32, 172, 92, summary, FONT_ALIGN_CENTER, GFX_BLACK);
    snprintf(summary, sizeof(summary), "明天%d", temps[city] - 2);
    font_draw_text_aligned(small, fb, 154, 172, 92, summary, FONT_ALIGN_CENTER, GFX_BLACK);
    snprintf(summary, sizeof(summary), "后天%d", lows[city]);
    font_draw_text_aligned(small, fb, 276, 172, 92, summary, FONT_ALIGN_CENTER, lows[city] < 20 ? GFX_RED : GFX_BLACK);

    font_draw_text_aligned(small, fb, 42, 232, 316, "空气质量 良", FONT_ALIGN_CENTER, GFX_BLACK);
    gfx_draw_rect(fb, 42, 260, 316, 12, GFX_BLACK);
    gfx_fill_rect(fb, 42, 260, app->weather_stale ? 96 : 180, 12, app->weather_stale ? GFX_BLACK : GFX_RED);
}

static void render_calendar(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *small = font_get_face(FONT_SIZE_14);
    const font_face_t *normal = font_get_face(FONT_SIZE_16);
    char title[32];
    int month = 6 + app->calendar_month_offset;
    int year = 2025;
    while (month > 12) {
        month -= 12;
        year++;
    }
    while (month < 1) {
        month += 12;
        year--;
    }
    snprintf(title, sizeof(title), "%d年%d月", year, month);
    title_bar(fb, font, title, app->calendar_detail_open ? "详情" : "月历");

    const char *week[] = {"日", "一", "二", "三", "四", "五", "六"};
    for (int i = 0; i < 7; i++) {
        font_draw_text_aligned(small, fb, 30 + i * 49, 42, 28, week[i], FONT_ALIGN_CENTER, i == 0 || i == 6 ? GFX_RED : GFX_BLACK);
    }
    for (int day = 1; day <= 30; day++) {
        int col = (day + 5) % 7;
        int row = (day + 5) / 7;
        int x = 35 + col * 48;
        int y = 70 + row * 34;
        char label[4];
        snprintf(label, sizeof(label), "%d", day);
        if (day == app->calendar_selected_day) {
            gfx_draw_rect(fb, x - 11, y - 10, 36, 28, GFX_RED);
            gfx_draw_rect(fb, x - 10, y - 9, 34, 26, GFX_RED);
        }
        if (day == 15 && app->calendar_month_offset == 0) {
            gfx_fill_rect(fb, x - 8, y - 8, 30, 24, GFX_RED);
        }
        font_draw_text_aligned(small, fb, x - 8, y, 32, label, FONT_ALIGN_CENTER, day == 15 && app->calendar_month_offset == 0 ? GFX_WHITE : ((col == 0 || col == 6) ? GFX_RED : GFX_BLACK));
    }
    if (app->calendar_detail_open) {
        char selected[32];
        snprintf(selected, sizeof(selected), "%d月%d日", month, app->calendar_selected_day);
        gfx_fill_rect(fb, 28, 250, 344, 44, GFX_WHITE);
        gfx_draw_rect(fb, 28, 250, 344, 44, GFX_BLACK);
        gfx_fill_rect(fb, 28, 250, 344, 3, GFX_RED);
        font_draw_text_aligned(normal, fb, 34, 258, 116, selected, FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, 152, 258, 208, "农历五月十九 宜阅读", FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, 34, 276, 326, app->calendar_selected_day == 21 ? "节气 夏至" : "无日程提醒", FONT_ALIGN_CENTER, app->calendar_selected_day == 21 ? GFX_RED : GFX_BLACK);
    } else {
        font_draw_text_aligned(small, fb, 0, 262, GFX_WIDTH, "农历 五月十九  夏至 6月21日", FONT_ALIGN_CENTER, GFX_BLACK);
    }
}

static void render_english(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_16);
    const font_face_t *small = font_get_face(FONT_SIZE_14);
    const font_face_t *big = font_get_face(FONT_SIZE_20);
    const char *words[] = {"SERENDIPITY", "KINDLE", "PAPER"};
    const char *sounds[] = {"/sound/", "/kindl/", "/peiper/"};
    const char *meanings[] = {"释义 机缘巧合", "释义 电子阅读器", "释义 纸张"};
    const char *examples[] = {"例句 A happy discovery.", "例句 Read on Kindle.", "例句 Turn the paper."};
    char progress[24];
    char stats[48];
    snprintf(progress, sizeof(progress), "%d/3", app->english_word + 1);
    title_bar(fb, font, "英语学习", progress);
    gfx_draw_rect(fb, 42, 56, 316, 92, GFX_RED);
    font_draw_text_aligned(big, fb, 42, 80, 316, words[app->english_word], FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(small, fb, 42, 120, 316, sounds[app->english_word], FONT_ALIGN_CENTER, GFX_RED);
    snprintf(stats, sizeof(stats), "认识%d 复习%d", app->english_known_count, app->english_review_count);
    font_draw_text_aligned(small, fb, 0, 162, GFX_WIDTH, stats, FONT_ALIGN_CENTER, GFX_BLACK);
    if (app->english_show_back) {
        font_draw_text_aligned(normal, fb, 0, 190, GFX_WIDTH, meanings[app->english_word], FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, 0, 216, GFX_WIDTH, examples[app->english_word], FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, 0, 252, GFX_WIDTH, "上键不认识  下键认识", FONT_ALIGN_CENTER, GFX_RED);
    } else {
        font_draw_text_aligned(small, fb, 0, 210, GFX_WIDTH, "HOME 翻转查看释义", FONT_ALIGN_CENTER, GFX_BLACK);
    }
    for (int i = 0; i < 3; i++) {
        gfx_color_t color = GFX_BLACK;
        if (app->english_answer_state[i] == 1) {
            color = GFX_RED;
        } else if (i == app->english_word) {
            color = GFX_RED;
        }
        gfx_fill_rect(fb, 165 + i * 23, 276, 10, 10, color);
        if (app->english_answer_state[i] == 2) {
            gfx_draw_rect(fb, 164 + i * 23, 275, 12, 12, GFX_RED);
        }
    }
}

static void render_settings(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_16);
    const int font_sizes[] = {16, 18, 20, 22, 24};
    const char *line_spacing[] = {"紧凑", "标准", "舒适", "宽松"};
    const char *cities[] = {"北京", "上海", "广州"};
    char rows[6][32];

    snprintf(rows[0], sizeof(rows[0]), "字体大小 %d", font_sizes[app->font_size_index]);
    snprintf(rows[1], sizeof(rows[1]), "字体 宋体");
    snprintf(rows[2], sizeof(rows[2]), "行间距 %s", line_spacing[app->line_spacing_index]);
    snprintf(rows[3], sizeof(rows[3]), "WiFi %s", app->wifi_connected ? "已连接" : "未连接");
    snprintf(rows[4], sizeof(rows[4]), "城市 %s", cities[app->weather_city_index]);
    snprintf(rows[5], sizeof(rows[5]), "省电模式 %s", app->power_saving_enabled ? "开" : "关");
    title_bar(fb, font, "设置", "");
    for (int i = 0; i < 6; i++) {
        int y = 38 + i * 39;
        if (app->settings_selection == i) {
            gfx_fill_rect(fb, 0, y, GFX_WIDTH, 32, GFX_RED);
            gfx_fill_rect(fb, 0, y + 2, GFX_WIDTH, 28, GFX_WHITE);
        }
        font_draw_text(normal, fb, 22, y + 7, rows[i], GFX_BLACK);
        if (i == 5 && app->power_saving_enabled) {
            gfx_fill_rect(fb, 330, y + 6, 36, 20, GFX_RED);
        }
    }
}

static void render_snake(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_16);
    const font_face_t *small = font_get_face(FONT_SIZE_14);
    const char *games[] = {"贪吃蛇  最高分:48", "推箱子  已完成:3/20", "数独    中级完成"};
    char score[16];
    const int board_x = 20;
    const int board_y = 132;
    const int cell = 10;
    snprintf(score, sizeof(score), "分:%d", app->snake_score);
    title_bar(fb, font, "游戏", score);
    for (int i = 0; i < 3; i++) {
        int y = 34 + i * 28;
        if (app->game_selection == i) {
            gfx_fill_rect(fb, 16, y, 360, 24, GFX_RED);
            gfx_fill_rect(fb, 18, y + 2, 356, 20, GFX_WHITE);
        }
        font_draw_text(normal, fb, 28, y + 4, games[i], GFX_BLACK);
    }

    gfx_draw_rect(fb, 16, 126, 368, 112, GFX_BLACK);
    for (int x = board_x; x < board_x + 360; x += cell) {
        gfx_set_pixel(fb, x, 128, GFX_BLACK);
    }
    for (int y = board_y; y < board_y + 100; y += cell) {
        gfx_set_pixel(fb, 18, y, GFX_BLACK);
    }

    gfx_fill_rect(fb, board_x + app->snake_food_x * cell + 2, board_y + app->snake_food_y * cell + 2, 6, 6, GFX_RED);
    for (int i = 3; i >= 0; i--) {
        int segment_x = app->snake_x - i;
        if (segment_x >= 0) {
            gfx_fill_rect(fb, board_x + segment_x * cell + 1, board_y + app->snake_y * cell + 1, 8, 8, GFX_BLACK);
        }
    }
    gfx_draw_rect(fb, board_x + app->snake_x * cell, board_y + app->snake_y * cell, 10, 10, GFX_RED);

    if (app->snake_game_over) {
        font_draw_text_aligned(normal, fb, 0, 252, GFX_WIDTH, "游戏结束  HOME重新开始", FONT_ALIGN_CENTER, GFX_BLACK);
    } else if (app->snake_running) {
        font_draw_text_aligned(small, fb, 0, 254, GFX_WIDTH, "上/下转向  HOME前进", FONT_ALIGN_CENTER, GFX_BLACK);
    } else if (app->game_selection == 0) {
        font_draw_text_aligned(small, fb, 0, 254, GFX_WIDTH, "HOME开始", FONT_ALIGN_CENTER, GFX_BLACK);
    } else {
        font_draw_text_aligned(small, fb, 0, 254, GFX_WIDTH, "预览功能", FONT_ALIGN_CENTER, GFX_BLACK);
    }
}

static void render_about(gfx_framebuffer_t *fb, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_16);
    const font_face_t *small = font_get_face(FONT_SIZE_14);
    title_bar(fb, font, "关于", "");
    font_draw_text_aligned(normal, fb, 0, 62, GFX_WIDTH, "ESP32 墨水屏阅读器", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text(small, fb, 70, 108, "固件版本 SIM V0", GFX_BLACK);
    font_draw_text(small, fb, 70, 146, "芯片型号 ESP32 N16R8", GFX_BLACK);
    font_draw_text(small, fb, 70, 184, "Flash 16MB  PSRAM 8MB", GFX_BLACK);
    font_draw_text_aligned(normal, fb, 0, 230, GFX_WIDTH, "400X300 三色显示", FONT_ALIGN_CENTER, GFX_RED);
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
