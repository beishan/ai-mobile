#include "ui/pages.h"
#include "ui/icons.h"
#include "app/reader_library.h"

#include <stdio.h>
#include <stddef.h>

#define STATUS_BAR_HEIGHT 24

/* Shared 480x800 portrait layout constants.
 * Content area sits below the status bar and stays inside the side margins. */
#define PAGE_MARGIN_X 24
#define CONTENT_WIDTH (GFX_WIDTH - 2 * PAGE_MARGIN_X)
#define BODY_TOP 40
#define BODY_BOTTOM (GFX_HEIGHT - 40)
#define BODY_HEIGHT (BODY_BOTTOM - BODY_TOP)

static void title_bar(gfx_framebuffer_t *fb, const font_t *font, const char *left, const char *right) {
    const font_face_t *small = font_get_face(FONT_SIZE_12);
    int text_y = (STATUS_BAR_HEIGHT - small->size) / 2;
    (void)font;
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, STATUS_BAR_HEIGHT, GFX_BLACK);
    font_draw_text(small, fb, 8, text_y, left, GFX_WHITE);
    if (right != NULL) {
        int width = font_measure_text(small, right);
        font_draw_text(small, fb, GFX_WIDTH - width - 8, text_y, right, GFX_WHITE);
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

static void draw_rounded_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, gfx_color_t color);

static void home_status_bar(gfx_framebuffer_t *fb, const font_t *font) {
    const font_face_t *small = font_get_face(FONT_SIZE_12);
    int text_y = (STATUS_BAR_HEIGHT - small->size) / 2;
    int wifi_y = (STATUS_BAR_HEIGHT - 18) / 2;
    int battery_y = (STATUS_BAR_HEIGHT - 18) / 2;
    (void)font;
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, STATUS_BAR_HEIGHT, GFX_BLACK);
    font_draw_text(small, fb, 8, text_y, "14:35  晴 26C 北京", GFX_WHITE);
    draw_wifi_icon(fb, GFX_WIDTH - 90, wifi_y, GFX_WHITE);
    draw_battery_icon(fb, GFX_WIDTH - 66, battery_y, 78, GFX_WHITE);
    font_draw_text(small, fb, GFX_WIDTH - 36, text_y, "78%", GFX_WHITE);
}

/* Info card shown above the app grid on the home screen.
 * Bordered card split by a vertical divider: left = weather,
 * right = large clock. */
static void home_info_card(gfx_framebuffer_t *fb, const app_state_t *app, int x, int y, int w, int h) {
    const font_face_t *small = font_get_face(FONT_SIZE_14);
    const font_face_t *city_font = font_get_face(FONT_SIZE_16);
    const font_face_t *temp_font = font_get_face(FONT_SIZE_22);
    const font_face_t *clock_font = font_get_face(FONT_SIZE_24);

    const char *cities[] = {"北京", "上海", "广州"};
    const char *conditions[] = {"晴", "雨", "云"};
    const int temps[] = {26, 22, 29};
    int city = app->weather_city_index;
    if (city < 0 || city > 2) {
        city = 0;
    }

    /* Outer border */
    gfx_draw_rect(fb, x, y, w, h, GFX_BLACK);

    /* Vertical divider — left ~60%, right ~40% */
    int divider_x = x + w * 3 / 5;
    gfx_fill_rect(fb, divider_x, y + 8, 1, h - 16, GFX_BLACK);

    /* --- Left: weather --- */
    /* Sun icon: hollow disc outline drawn pixel-by-pixel (Bresenham circle, r=8) + rays. */
    int icon_cx = x + 30;
    int icon_cy = y + h / 2;
    {
        int r = 8;
        int cx = 0, cy = r;
        int d = 3 - 2 * r;
        while (cx <= cy) {
            int pts[][2] = {
                {icon_cx + cx, icon_cy + cy}, {icon_cx - cx, icon_cy + cy},
                {icon_cx + cx, icon_cy - cy}, {icon_cx - cx, icon_cy - cy},
                {icon_cx + cy, icon_cy + cx}, {icon_cx - cy, icon_cy + cx},
                {icon_cx + cy, icon_cy - cx}, {icon_cx - cy, icon_cy - cx},
            };
            for (size_t k = 0; k < sizeof(pts) / sizeof(pts[0]); k++) {
                gfx_set_pixel(fb, pts[k][0], pts[k][1], GFX_BLACK);
            }
            if (d < 0) {
                d += 4 * cx + 6;
            } else {
                d += 4 * (cx - cy) + 10;
                cy--;
            }
            cx++;
        }
        /* Rays (4 cardinal + 4 diagonal) */
        gfx_fill_rect(fb, icon_cx - 1, icon_cy - 12, 2, 2, GFX_BLACK);
        gfx_fill_rect(fb, icon_cx - 1, icon_cy + 10, 2, 2, GFX_BLACK);
        gfx_fill_rect(fb, icon_cx - 12, icon_cy - 1, 2, 2, GFX_BLACK);
        gfx_fill_rect(fb, icon_cx + 10, icon_cy - 1, 2, 2, GFX_BLACK);
    }

    /* Weather text to the right of the icon */
    int text_x = icon_cx + 16;
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%dC %s", temps[city], conditions[city]);
    font_draw_text(temp_font, fb, text_x, icon_cy - temp_font->size + 2, temp_str, GFX_BLACK);
    font_draw_text(city_font, fb, text_x, icon_cy + 6, cities[city], GFX_BLACK);

    /* --- Right: large clock --- */
    const char *clock_text = "14:35";
    font_draw_text_aligned(clock_font, fb, divider_x, y + (h - clock_font->size) / 2 - 4,
                            w - (divider_x - x), clock_text, FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(small, fb, divider_x, y + (h - clock_font->size) / 2 + clock_font->size,
                            w - (divider_x - x), "星期三", FONT_ALIGN_CENTER, GFX_BLACK);
}

static void app_tile(gfx_framebuffer_t *fb, const font_t *font, ui_icon_kind_t icon, int x, int y, int w, int h, const char *label, int selected) {
    const font_face_t *label_font = font_get_face(FONT_SIZE_18);
    int icon_size = 48;
    int icon_x = x + (w - icon_size) / 2;
    int icon_y = y + (h - icon_size - 28) / 2;
    (void)font;
    if (selected) {
        draw_rounded_rect(fb, x, y, w, h, GFX_BLACK);
    }
    ui_draw_icon(fb, icon, icon_x, icon_y, 0);
    font_draw_text_aligned(label_font, fb, x, icon_y + icon_size + 8, w, label, FONT_ALIGN_CENTER, GFX_BLACK);
}

static void render_home(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const char *items[] = {"阅读", "天气", "日历", "英语", "设置", "关于"};
    const ui_icon_kind_t icons[] = {
        UI_ICON_READER,
        UI_ICON_WEATHER,
        UI_ICON_CALENDAR,
        UI_ICON_ENGLISH,
        UI_ICON_SETTINGS,
        UI_ICON_ABOUT
    };

    home_status_bar(fb, font);

    /* Info card above the grid: weather + clock */
    const int card_margin = PAGE_MARGIN_X;
    const int card_w = GFX_WIDTH - 2 * card_margin;
    const int card_h = 96;
    const int card_x = card_margin;
    const int card_y = BODY_TOP + 8;
    home_info_card(fb, app, card_x, card_y, card_w, card_h);

    /* 3 columns x 2 rows grid, centered below the info card, NOT filling the screen. */
    const int cols = 3;
    const int rows = 2;
    const int gap = 24;
    const int tile_w = 108;
    const int tile_h = 96;
    const int total_w = cols * tile_w + (cols - 1) * gap;
    const int total_h = rows * tile_h + (rows - 1) * gap;
    const int grid_top = card_y + card_h + 24;
    const int grid_bottom = BODY_BOTTOM;
    const int grid_avail = grid_bottom - grid_top;
    const int start_x = (GFX_WIDTH - total_w) / 2;
    const int start_y = grid_top + (grid_avail - total_h) / 2;

    for (int i = 0; i < 6; i++) {
        int col = i % cols;
        int row = i / cols;
        int x = start_x + col * (tile_w + gap);
        int y = start_y + row * (tile_h + gap);
        app_tile(fb, font, icons[i], x, y, tile_w, tile_h, items[i], app->home_selection == i);
    }
}

static void draw_rounded_rect(gfx_framebuffer_t *fb, int x, int y, int w, int h, gfx_color_t color) {
    gfx_fill_rect(fb, x + 6, y, w - 12, 1, color);
    gfx_fill_rect(fb, x + 6, y + h - 1, w - 12, 1, color);
    gfx_fill_rect(fb, x, y + 6, 1, h - 12, color);
    gfx_fill_rect(fb, x + w - 1, y + 6, 1, h - 12, color);
    gfx_set_pixel(fb, x + 3, y + 2, color);
    gfx_set_pixel(fb, x + 2, y + 3, color);
    gfx_set_pixel(fb, x + w - 4, y + 2, color);
    gfx_set_pixel(fb, x + w - 3, y + 3, color);
    gfx_set_pixel(fb, x + 3, y + h - 3, color);
    gfx_set_pixel(fb, x + 2, y + h - 4, color);
    gfx_set_pixel(fb, x + w - 4, y + h - 3, color);
    gfx_set_pixel(fb, x + w - 3, y + h - 4, color);
}

static void draw_file_type_icon(gfx_framebuffer_t *fb, const font_face_t *font, int x, int y, const char *type) {
    char mark[2] = {'?', '\0'};
    if (type != NULL && type[0] != '\0') {
        mark[0] = type[0];
    }

    gfx_draw_rect(fb, x, y, 18, 22, GFX_BLACK);
    gfx_fill_rect(fb, x + 13, y, 5, 5, GFX_WHITE);
    gfx_set_pixel(fb, x + 14, y + 1, GFX_BLACK);
    gfx_set_pixel(fb, x + 15, y + 2, GFX_BLACK);
    gfx_set_pixel(fb, x + 16, y + 3, GFX_BLACK);
    font_draw_text_aligned(font, fb, x + 1, y + 8, 16, mark, FONT_ALIGN_CENTER, GFX_BLACK);
}

static void render_bookshelf(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_16);
    const font_face_t *small = font_get_face(FONT_SIZE_14);

    title_bar(fb, font, "书架", "3本");
    for (int i = 0; i < reader_library_book_count(); i++) {
        const int row_height = 34;
        const int icon_height = 22;
        int row_top = 38 + i * 40;
        int icon_y = row_top + (row_height - icon_height) / 2;
        int title_y = row_top + (row_height - normal->size) / 2;
        int small_y = row_top + (row_height - small->size) / 2;
        int percent = (app->book_current_pages[i] + 1) * 100 / app->book_pages[i];
        char progress[12];
        const reader_book_t *book = reader_library_book(i);

        snprintf(progress, sizeof(progress), "%d%%", percent);
        if (app->bookshelf_selection == i) {
            draw_rounded_rect(fb, 12, row_top, GFX_WIDTH - 24, row_height, GFX_BLACK);
        }
        draw_file_type_icon(fb, small, 24, icon_y, book->file_type);
        font_draw_text(normal, fb, 54, title_y, book->title, GFX_BLACK);
        if (app->recent_book == i) {
            font_draw_text(small, fb, 170, small_y, "最近", GFX_BLACK);
        }
        font_draw_text_aligned(normal, fb, GFX_WIDTH - 112, title_y, 88, progress, FONT_ALIGN_RIGHT, GFX_BLACK);
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
    char title[64];
    const font_face_t *body_font = reader_body_font(app);
    const reader_book_t *book = reader_library_book(app->current_book);
    snprintf(page, sizeof(page), "%d/%d", app->reader_page + 1, app->book_pages[app->current_book]);
    snprintf(title, sizeof(title), "%s | %s", book->title, book->chapter_title);
    title_bar(fb, font, title, page);

    font_draw_text_box_spaced(body_font, fb, 24, 46, 352, 154,
                              reader_library_page_text(app->current_book, app->reader_page),
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
        gfx_fill_rect(fb, 88, 64, 224, 4, GFX_BLACK);
        for (int i = 0; i < 4; i++) {
            int y = 82 + i * 28;
            gfx_color_t color = GFX_BLACK;
            if (app->reader_menu_selection == i) {
                gfx_fill_rect(fb, 104, y - 5, 192, 24, GFX_BLACK);
                color = GFX_WHITE;
            }
            font_draw_text(menu, fb, 124, y, items[i], color);
        }
        if (app->reader_catalog_open) {
            int chapter_count = reader_library_chapter_count(app->current_book);
            gfx_fill_rect(fb, 66, 52, 268, 164, GFX_WHITE);
            gfx_draw_rect(fb, 66, 52, 268, 164, GFX_BLACK);
            gfx_fill_rect(fb, 66, 52, 268, 4, GFX_BLACK);
            font_draw_text(font_get_face(FONT_SIZE_14), fb, 86, 68, "目录", GFX_BLACK);
            for (int i = 0; i < chapter_count; i++) {
                int y = 98 + i * 32;
                gfx_color_t color = GFX_BLACK;
                if (app->reader_catalog_selection == i) {
                    gfx_fill_rect(fb, 82, y - 5, 236, 24, GFX_BLACK);
                    color = GFX_WHITE;
                }
                font_draw_text(menu, fb, 98, y, reader_library_chapter_title(app->current_book, i), color);
            }
        }
    }
}

static void render_weather(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_22);
    const font_face_t *small = font_get_face(FONT_SIZE_18);
    const font_face_t *huge = font_get_face(FONT_SIZE_24);
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

    /* hero block: city + big temperature */
    font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 30, CONTENT_WIDTH, cities[city], FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(huge, fb, PAGE_MARGIN_X, BODY_TOP + 90, CONTENT_WIDTH, temp, FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 200, CONTENT_WIDTH, summary, FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 240, CONTENT_WIDTH, cache, FONT_ALIGN_CENTER, GFX_BLACK);

    /* three forecast cards spread across content width */
    {
        int card_y = BODY_TOP + 320;
        int card_h = 120;
        int gap = 20;
        int card_w = (CONTENT_WIDTH - gap * 2) / 3;
        const char *labels[] = {"今天", "明天", "后天"};
        int card_temps[] = {temps[city], temps[city] - 2, lows[city]};
        for (int i = 0; i < 3; i++) {
            int cx = PAGE_MARGIN_X + i * (card_w + gap);
            gfx_draw_rect(fb, cx, card_y, card_w, card_h, GFX_BLACK);
            font_draw_text_aligned(small, fb, cx, card_y + 18, card_w, labels[i], FONT_ALIGN_CENTER, GFX_BLACK);
            char t[12];
            snprintf(t, sizeof(t), "%dC", card_temps[i]);
            font_draw_text_aligned(normal, fb, cx, card_y + 56, card_w, t, FONT_ALIGN_CENTER, GFX_BLACK);
        }
    }

    /* air quality bar across full content width */
    {
        int aq_y = BODY_TOP + 480;
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, aq_y, CONTENT_WIDTH, "空气质量 良", FONT_ALIGN_CENTER, GFX_BLACK);
        gfx_draw_rect(fb, PAGE_MARGIN_X, aq_y + 40, CONTENT_WIDTH, 16, GFX_BLACK);
        gfx_fill_rect(fb, PAGE_MARGIN_X, aq_y + 40, app->weather_stale ? CONTENT_WIDTH / 3 : CONTENT_WIDTH * 2 / 3, 16, GFX_BLACK);
    }
}

static void render_calendar(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *small = font_get_face(FONT_SIZE_18);
    const font_face_t *normal = font_get_face(FONT_SIZE_22);
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

    /* calendar grid sized to fill the content width */
    {
        int grid_x = PAGE_MARGIN_X;
        int grid_w = CONTENT_WIDTH;
        int col_w = grid_w / 7;
        int week_y = BODY_TOP + 10;
        int cell_h = 80;
        int day_top = week_y + 36;
        const char *week[] = {"日", "一", "二", "三", "四", "五", "六"};
        for (int i = 0; i < 7; i++) {
            font_draw_text_aligned(small, fb, grid_x + i * col_w, week_y, col_w, week[i], FONT_ALIGN_CENTER, GFX_BLACK);
        }
        for (int day = 1; day <= 30; day++) {
            int col = (day + 5) % 7;
            int row = (day + 5) / 7;
            int cx = grid_x + col * col_w;
            int cy = day_top + row * cell_h;
            char label[4];
            snprintf(label, sizeof(label), "%d", day);
            if (day == app->calendar_selected_day) {
                gfx_draw_rect(fb, cx + 8, cy + 8, col_w - 16, cell_h - 16, GFX_BLACK);
                gfx_draw_rect(fb, cx + 10, cy + 10, col_w - 20, cell_h - 20, GFX_BLACK);
            }
            if (day == 15 && app->calendar_month_offset == 0) {
                gfx_fill_rect(fb, cx + 10, cy + 10, col_w - 20, cell_h - 20, GFX_BLACK);
            }
            font_draw_text_aligned(normal, fb, cx, cy + 24, col_w, label, FONT_ALIGN_CENTER, day == 15 && app->calendar_month_offset == 0 ? GFX_WHITE : GFX_BLACK);
        }
    }

    if (app->calendar_detail_open) {
        int box_y = BODY_TOP + 560;
        int box_h = 120;
        char selected[32];
        snprintf(selected, sizeof(selected), "%d月%d日", month, app->calendar_selected_day);
        gfx_fill_rect(fb, PAGE_MARGIN_X, box_y, CONTENT_WIDTH, box_h, GFX_WHITE);
        gfx_draw_rect(fb, PAGE_MARGIN_X, box_y, CONTENT_WIDTH, box_h, GFX_BLACK);
        gfx_fill_rect(fb, PAGE_MARGIN_X, box_y, CONTENT_WIDTH, 5, GFX_BLACK);
        font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, box_y + 18, CONTENT_WIDTH / 2, selected, FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, box_y + 60, CONTENT_WIDTH, "农历五月十九 宜阅读", FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, box_y + 90, CONTENT_WIDTH, app->calendar_selected_day == 21 ? "节气 夏至" : "无日程提醒", FONT_ALIGN_CENTER, GFX_BLACK);
    } else {
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 600, CONTENT_WIDTH, "农历 五月十九  夏至 6月21日", FONT_ALIGN_CENTER, GFX_BLACK);
    }
}

static void render_english(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_22);
    const font_face_t *small = font_get_face(FONT_SIZE_18);
    const font_face_t *big = font_get_face(FONT_SIZE_24);
    const char *words[] = {"SERENDIPITY", "KINDLE", "PAPER"};
    const char *sounds[] = {"/sound/", "/kindl/", "/peiper/"};
    const char *meanings[] = {"释义 机缘巧合", "释义 电子阅读器", "释义 纸张"};
    const char *examples[] = {"例句 A happy discovery.", "例句 Read on Kindle.", "例句 Turn the paper."};
    char progress[24];
    char stats[48];
    snprintf(progress, sizeof(progress), "%d/3", app->english_word + 1);
    title_bar(fb, font, "英语学习", progress);

    /* word card filling the top of the content area */
    {
        int card_y = BODY_TOP + 20;
        int card_h = 240;
        gfx_draw_rect(fb, PAGE_MARGIN_X, card_y, CONTENT_WIDTH, card_h, GFX_BLACK);
        font_draw_text_aligned(big, fb, PAGE_MARGIN_X, card_y + 60, CONTENT_WIDTH, words[app->english_word], FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, card_y + 150, CONTENT_WIDTH, sounds[app->english_word], FONT_ALIGN_CENTER, GFX_BLACK);
    }

    snprintf(stats, sizeof(stats), "认识%d 复习%d", app->english_known_count, app->english_review_count);
    font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 300, CONTENT_WIDTH, stats, FONT_ALIGN_CENTER, GFX_BLACK);

    if (app->english_show_back) {
        font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 380, CONTENT_WIDTH, meanings[app->english_word], FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 440, CONTENT_WIDTH, examples[app->english_word], FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 560, CONTENT_WIDTH, "上键不认识  下键认识", FONT_ALIGN_CENTER, GFX_BLACK);
    } else {
        font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 480, CONTENT_WIDTH, "HOME 翻转查看释义", FONT_ALIGN_CENTER, GFX_BLACK);
    }
    /* answer-state dots centered near the bottom */
    {
        int dot_w = 14;
        int dot_gap = 24;
        int total_w = 3 * dot_w + 2 * dot_gap;
        int start_x = (GFX_WIDTH - total_w) / 2;
        int dot_y = BODY_TOP + 640;
        for (int i = 0; i < 3; i++) {
            gfx_fill_rect(fb, start_x + i * (dot_w + dot_gap), dot_y, dot_w, dot_w, GFX_BLACK);
            if (app->english_answer_state[i] == 2) {
                gfx_draw_rect(fb, start_x + i * (dot_w + dot_gap) - 2, dot_y - 2, dot_w + 4, dot_w + 4, GFX_BLACK);
            }
        }
    }
}

static void render_settings(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const font_face_t *normal = font_get_face(FONT_SIZE_22);
    const font_face_t *small = font_get_face(FONT_SIZE_18);
    const int font_sizes[] = {16, 18, 20, 22, 24};
    const char *line_spacing[] = {"紧凑", "标准", "舒适", "宽松"};
    const char *cities[] = {"北京", "上海", "广州"};
    const int row_count = 6;
    const int gap = 12;
    const int row_h = (BODY_HEIGHT - gap * (row_count - 1)) / row_count;
    char rows[6][32];

    snprintf(rows[0], sizeof(rows[0]), "字体大小 %d", font_sizes[app->font_size_index]);
    snprintf(rows[1], sizeof(rows[1]), "字体 宋体");
    snprintf(rows[2], sizeof(rows[2]), "行间距 %s", line_spacing[app->line_spacing_index]);
    snprintf(rows[3], sizeof(rows[3]), "WiFi %s", app->wifi_connected ? "已连接" : "未连接");
    snprintf(rows[4], sizeof(rows[4]), "城市 %s", cities[app->weather_city_index]);
    snprintf(rows[5], sizeof(rows[5]), "省电模式 %s", app->power_saving_enabled ? "开" : "关");
    title_bar(fb, font, "设置", "");
    for (int i = 0; i < row_count; i++) {
        int y = BODY_TOP + i * (row_h + gap);
        int text_y = y + (row_h - normal->size) / 2;
        if (app->settings_selection == i) {
            gfx_fill_rect(fb, PAGE_MARGIN_X, y, CONTENT_WIDTH, row_h, GFX_BLACK);
            font_draw_text(normal, fb, PAGE_MARGIN_X + 20, text_y, rows[i], GFX_WHITE);
            if (i == 5 && app->power_saving_enabled) {
                gfx_fill_rect(fb, GFX_WIDTH - PAGE_MARGIN_X - 60, y + (row_h - 24) / 2, 40, 24, GFX_WHITE);
            }
        } else {
            font_draw_text(normal, fb, PAGE_MARGIN_X + 20, text_y, rows[i], GFX_BLACK);
            if (i == 5 && app->power_saving_enabled) {
                gfx_fill_rect(fb, GFX_WIDTH - PAGE_MARGIN_X - 60, y + (row_h - 24) / 2, 40, 24, GFX_BLACK);
            }
        }
        (void)small;
    }
}

static void render_about(gfx_framebuffer_t *fb, const font_t *font) {
    const font_face_t *title = font_get_face(FONT_SIZE_24);
    const font_face_t *normal = font_get_face(FONT_SIZE_20);
    const font_face_t *small = font_get_face(FONT_SIZE_18);
    title_bar(fb, font, "关于", "");
    font_draw_text_aligned(title, fb, PAGE_MARGIN_X, BODY_TOP + 40, CONTENT_WIDTH, "ESP32 墨水屏阅读器", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 160, CONTENT_WIDTH, "固件版本 SIM V0", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 240, CONTENT_WIDTH, "芯片型号 ESP32 N16R8", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 320, CONTENT_WIDTH, "Flash 16MB  PSRAM 8MB", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(normal, fb, PAGE_MARGIN_X, BODY_TOP + 440, CONTENT_WIDTH, "4.26寸 480X800 黑白高刷", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(small, fb, PAGE_MARGIN_X, BODY_TOP + 520, CONTENT_WIDTH, "SSD677 SPI", FONT_ALIGN_CENTER, GFX_BLACK);
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
        case APP_PAGE_ABOUT:
        default:
            render_about(fb, font);
            break;
    }
}
