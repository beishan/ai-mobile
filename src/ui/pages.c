#include "ui/pages.h"
#include "ui/icons.h"
#include "app/reader_library.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#define STATUS_BAR_HEIGHT 32

/* Shared 480x800 portrait layout constants.
 * Content area sits below the status bar and stays inside the side margins. */
#define PAGE_MARGIN_X 24
#define CONTENT_WIDTH (GFX_WIDTH - 2 * PAGE_MARGIN_X)
#define BODY_TOP 40
#define BODY_BOTTOM (GFX_HEIGHT - 40)
#define BODY_HEIGHT (BODY_BOTTOM - BODY_TOP)

static void settings_status_bar(gfx_framebuffer_t *fb);
static void home_status_bar(gfx_framebuffer_t *fb, const font_t *font);

typedef enum {
    SETTINGS_ICON_WIFI = 0,
    SETTINGS_ICON_BLUETOOTH,
    SETTINGS_ICON_WEATHER,
    SETTINGS_ICON_CLOCK,
    SETTINGS_ICON_LEAF,
    SETTINGS_ICON_STORAGE,
    SETTINGS_ICON_DICT,
    SETTINGS_ICON_INFO,
    SETTINGS_ICON_UPDATE
} settings_icon_t;

static void settings_fill_circle(gfx_framebuffer_t *fb, int cx, int cy, int r, gfx_color_t color);
static void settings_draw_circle(gfx_framebuffer_t *fb, int cx, int cy, int r, int thickness, gfx_color_t color);
static void settings_draw_line(gfx_framebuffer_t *fb, int x0, int y0, int x1, int y1, int thickness, gfx_color_t color);
static void settings_draw_arc(gfx_framebuffer_t *fb, int cx, int cy, int r, int q1, int q2, int q3, int q4);
static void settings_draw_icon(gfx_framebuffer_t *fb, settings_icon_t icon, int x, int y);
static void settings_draw_arrow(gfx_framebuffer_t *fb, int x, int y);
static void bookshelf_status_bar(gfx_framebuffer_t *fb);

static void ui_now(struct tm *out) {
    time_t now = time(NULL);
    if (out == NULL) {
        return;
    }
    if (localtime_r(&now, out) == NULL) {
        memset(out, 0, sizeof(*out));
        out->tm_year = 126;
        out->tm_mon = 0;
        out->tm_mday = 1;
    }
}

static void ui_format_time(char *buffer, size_t size) {
    struct tm now_tm;
    ui_now(&now_tm);
    snprintf(buffer, size, "%02d:%02d", now_tm.tm_hour, now_tm.tm_min);
}

static const char *ui_weekday_name(int wday) {
    static const char *names[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    if (wday < 0 || wday > 6) {
        return "星期日";
    }
    return names[wday];
}

static int ui_days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    if (month < 1 || month > 12) {
        return 30;
    }
    return days[month - 1];
}

static int ui_month_first_weekday_monday(int year, int month) {
    struct tm date_tm;
    memset(&date_tm, 0, sizeof(date_tm));
    date_tm.tm_year = year - 1900;
    date_tm.tm_mon = month - 1;
    date_tm.tm_mday = 1;
    date_tm.tm_hour = 12;
    date_tm.tm_isdst = -1;
    if (mktime(&date_tm) == (time_t)-1) {
        return 0;
    }
    return (date_tm.tm_wday + 6) % 7;
}

static void ui_add_month_offset(int *year, int *month, int offset) {
    if (year == NULL || month == NULL) {
        return;
    }
    *month += offset;
    while (*month > 12) {
        *month -= 12;
        (*year)++;
    }
    while (*month < 1) {
        *month += 12;
        (*year)--;
    }
}

static void title_bar(gfx_framebuffer_t *fb, const font_t *font, const char *left, const char *right) {
    (void)left;
    (void)right;
    home_status_bar(fb, font);
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
    const font_face_t *small = font_get_face(FONT_SIZE_14);
    int text_y = (STATUS_BAR_HEIGHT - small->size) / 2;
    int wifi_y = (STATUS_BAR_HEIGHT - 18) / 2;
    int battery_y = (STATUS_BAR_HEIGHT - 18) / 2;
    char time_text[32];
    char status_text[64];
    (void)font;
    ui_format_time(time_text, sizeof(time_text));
    snprintf(status_text, sizeof(status_text), "%s  晴 26C 北京", time_text);
    font_draw_text(small, fb, 8, text_y, status_text, GFX_BLACK);
    draw_wifi_icon(fb, GFX_WIDTH - 90, wifi_y, GFX_BLACK);
    draw_battery_icon(fb, GFX_WIDTH - 66, battery_y, 78, GFX_BLACK);
    font_draw_text(small, fb, GFX_WIDTH - 36, text_y, "78%", GFX_BLACK);
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
    const char *conditions[] = {"晴", "多云", "雨", "雪"};
    const int temps[] = {26, 22, 29};
    int city = app->weather_city_index;
    if (city < 0 || city > 2) {
        city = 0;
    }

    gfx_draw_rounded_rect_thick(fb, x, y, w, h, 12, 3, GFX_BLACK);

    int divider_x = x + w * 3 / 5;
    gfx_fill_rect(fb, divider_x, y + 8, 1, h - 16, GFX_BLACK);

    ui_icon_kind_t weather_icon;
    switch (app->weather_type) {
        case 0:
            weather_icon = UI_ICON_SUNNY;
            break;
        case 1:
            weather_icon = UI_ICON_CLOUDY;
            break;
        case 2:
            weather_icon = UI_ICON_RAINY;
            break;
        case 3:
            weather_icon = UI_ICON_SNOWY;
            break;
        default:
            weather_icon = UI_ICON_SUNNY;
            break;
    }

    int icon_size = 64;
    int icon_x = x + 16;
    int icon_y = y + (h - icon_size) / 2;
    ui_draw_icon(fb, weather_icon, icon_x, icon_y, 0);

    int text_x = icon_x + icon_size + 8;
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%d C %s", temps[city], conditions[app->weather_type]);
    font_draw_text(temp_font, fb, text_x, y + h / 2 - temp_font->size / 2 - 6, temp_str, GFX_BLACK);
    font_draw_text(city_font, fb, text_x, y + h / 2 + 6, cities[city], GFX_BLACK);

    char clock_text[8];
    char date_text[32];
    struct tm now_tm;
    ui_now(&now_tm);
    snprintf(clock_text, sizeof(clock_text), "%02d:%02d", now_tm.tm_hour, now_tm.tm_min);
    snprintf(date_text, sizeof(date_text), "%d年%d月%d日", now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday);
    font_draw_text_aligned(clock_font, fb, divider_x, y + h / 2 - clock_font->size / 2 - 8,
                            w - (divider_x - x), clock_text, FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_aligned(small, fb, divider_x, y + h / 2 + small->size / 2 + 2,
                            w - (divider_x - x), date_text, FONT_ALIGN_CENTER, GFX_BLACK);
}

static void app_tile(gfx_framebuffer_t *fb, const font_t *font, ui_icon_kind_t icon, int x, int y, int w, int h, const char *label, int selected) {
    const font_face_t *label_font = font_get_face(FONT_SIZE_18);
    int icon_bbox_size = 52;
    int label_height = label_font->size;
    int total_content_height = icon_bbox_size + 8 + label_height;
    int content_start_y = y + (h - total_content_height) / 2;
    int icon_x = x + (w - icon_bbox_size) / 2;
    int icon_y = content_start_y;

    (void)font;
    if (selected) {
        gfx_draw_rounded_rect_thick(fb, x, y, w, h, 12, 3, GFX_BLACK);
    }
    ui_draw_icon(fb, icon, icon_x, icon_y, 0);

    int text_y = icon_y + icon_bbox_size + 8;
    font_draw_text_aligned(label_font, fb, x, text_y, w, label, FONT_ALIGN_CENTER, GFX_BLACK);
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
    const int card_h = 120;
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

typedef enum {
    BOOKSHELF_COVER_INK = 0,
    BOOKSHELF_COVER_MOON,
    BOOKSHELF_COVER_FLOWER,
    BOOKSHELF_COVER_FILE,
    BOOKSHELF_COVER_RIVER
} bookshelf_cover_style_t;

typedef struct {
    char title[96];
    char author[64];
    char file_type[12];
    bookshelf_cover_style_t style;
} bookshelf_display_book_t;

static const bookshelf_display_book_t bookshelf_books[] = {
    {"活着", "余华", "TXT", BOOKSHELF_COVER_INK},
    {"人间值得", "中村恒子", "EPUB", BOOKSHELF_COVER_INK},
    {"月亮与六便士", "[英] 毛姆", "EPUB", BOOKSHELF_COVER_MOON},
    {"浮生六记", "沈复", "TXT", BOOKSHELF_COVER_INK},
    {"我与地坛", "史铁生", "EPUB", BOOKSHELF_COVER_FLOWER},
    {"小王子", "Antoine de Saint-Exupery", "EPUB", BOOKSHELF_COVER_FILE},
    {"计算机网络（第7版）", "谢希仁", "PDF", BOOKSHELF_COVER_FILE},
    {"三体（全三册）", "刘慈欣", "TXT", BOOKSHELF_COVER_RIVER},
    {"读书笔记合集", "2024-01-10", "TXT", BOOKSHELF_COVER_FILE}
};

static size_t utf8_clip_bytes(const char *text, size_t max_bytes) {
    size_t len;
    if (text == NULL) {
        return 0;
    }
    len = strlen(text);
    if (len <= max_bytes) {
        return len;
    }
    len = max_bytes;
    while (len > 0 && (((unsigned char)text[len]) & 0xc0u) == 0x80u) {
        len--;
    }
    return len > 0 ? len : max_bytes;
}

static void copy_short_text(char *dest, size_t dest_size, const char *text, size_t max_bytes) {
    size_t len;
    if (dest == NULL || dest_size == 0) {
        return;
    }
    len = utf8_clip_bytes(text, max_bytes);
    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, text != NULL ? text : "", len);
    dest[len] = '\0';
}

static bookshelf_display_book_t bookshelf_book_for_index(int index) {
    bookshelf_display_book_t display = bookshelf_books[index];
    if (index < APP_BOOK_COUNT) {
        const reader_book_t *book = reader_library_book(index);
        if (book != NULL) {
            copy_short_text(display.title, sizeof(display.title), book->title, 24);
            copy_short_text(display.author, sizeof(display.author), book->author, 18);
            copy_short_text(display.file_type, sizeof(display.file_type), book->file_type, 8);
            display.style = BOOKSHELF_COVER_FILE;
        }
    }
    return display;
}

static void bookshelf_status_bar(gfx_framebuffer_t *fb) {
    home_status_bar(fb, NULL);
}

static void draw_bookshelf_file_cover(gfx_framebuffer_t *fb, int x, int y, int w, int h, const char *type) {
    int doc_x = x + 34;
    int doc_y = y + 32;
    int doc_w = 56;
    int doc_h = 88;
    (void)h;
    gfx_draw_rect(fb, doc_x, doc_y, doc_w, doc_h, GFX_BLACK);
    settings_draw_line(fb, doc_x + doc_w - 18, doc_y, doc_x + doc_w, doc_y + 18, 1, GFX_BLACK);
    gfx_fill_rect(fb, doc_x + doc_w - 17, doc_y + 18, 17, 1, GFX_BLACK);
    font_draw_text_aligned_builtin(18, fb, x, y + 72, w, type, FONT_ALIGN_CENTER, GFX_BLACK);
    gfx_fill_rect(fb, doc_x + 22, doc_y + 112, 34, 1, GFX_BLACK);
    gfx_fill_rect(fb, doc_x + 22, doc_y + 121, 26, 1, GFX_BLACK);
}

static void draw_bookshelf_landscape(gfx_framebuffer_t *fb, int x, int y, int w, int h, int dense) {
    int base = y + h - 48;
    for (int i = 0; i < 7; i++) {
        int yy = base + i * 6;
        settings_draw_line(fb, x + 8, yy, x + 34 + i * 7, yy - 12, 1, GFX_BLACK);
        settings_draw_line(fb, x + 34 + i * 7, yy - 12, x + w - 8, yy - 5 + (i % 3) * 4, 1, GFX_BLACK);
    }
    if (dense) {
        for (int i = 0; i < 18; i++) {
            int px = x + 10 + (i * 17) % (w - 20);
            int py = y + h - 76 + (i * 11) % 60;
            gfx_fill_rect(fb, px, py, 2, 2, GFX_BLACK);
        }
    }
}

static void draw_bookshelf_cover_art(gfx_framebuffer_t *fb, const bookshelf_display_book_t *book,
                                     int x, int y, int w, int h, int index) {
    switch (book->style) {
        case BOOKSHELF_COVER_MOON:
            gfx_fill_rect(fb, x + 4, y + 4, w - 8, h - 8, GFX_BLACK);
            settings_draw_circle(fb, x + 46, y + 50, 16, 1, GFX_WHITE);
            settings_fill_circle(fb, x + 54, y + 45, 15, GFX_BLACK);
            for (int i = 0; i < 11; i++) {
                gfx_fill_rect(fb, x + 8, y + 122 + i * 3, w - 16, 1, GFX_WHITE);
            }
            font_draw_text_builtin(18, fb, x + 88, y + 22, "月亮", GFX_WHITE);
            font_draw_text_builtin(18, fb, x + 88, y + 47, "六便士", GFX_WHITE);
            break;
        case BOOKSHELF_COVER_FLOWER:
            settings_draw_line(fb, x + 30, y + 139, x + 66, y + 76, 2, GFX_BLACK);
            settings_draw_circle(fb, x + 70, y + 70, 8, 1, GFX_BLACK);
            settings_draw_circle(fb, x + 61, y + 74, 7, 1, GFX_BLACK);
            settings_draw_circle(fb, x + 75, y + 80, 7, 1, GFX_BLACK);
            font_draw_text_builtin(24, fb, x + 82, y + 36, "我与", GFX_BLACK);
            font_draw_text_builtin(24, fb, x + 82, y + 72, "地坛", GFX_BLACK);
            font_draw_text_builtin(14, fb, x + 92, y + 118, "史铁生", GFX_BLACK);
            break;
        case BOOKSHELF_COVER_FILE:
            draw_bookshelf_file_cover(fb, x, y, w, h, book->file_type);
            break;
        case BOOKSHELF_COVER_RIVER:
            draw_bookshelf_landscape(fb, x, y, w, h, 1);
            font_draw_text_aligned_builtin(24, fb, x, y + 28, w, "三体", FONT_ALIGN_CENTER, GFX_BLACK);
            font_draw_text_aligned_builtin(14, fb, x, y + 63, w, "（全三册）", FONT_ALIGN_CENTER, GFX_BLACK);
            break;
        case BOOKSHELF_COVER_INK:
        default:
            draw_bookshelf_landscape(fb, x, y, w, h, index == 3);
            if (index == 0) {
                font_draw_text_builtin(24, fb, x + 56, y + 28, "活着", GFX_BLACK);
                font_draw_text_builtin(14, fb, x + 14, y + 70, "余华", GFX_BLACK);
            } else if (index == 1) {
                font_draw_text_builtin(24, fb, x + 72, y + 26, "人间", GFX_BLACK);
                font_draw_text_builtin(24, fb, x + 72, y + 62, "值得", GFX_BLACK);
                font_draw_text_builtin(14, fb, x + 18, y + 32, "过一生", GFX_BLACK);
            } else if (index == 3) {
                font_draw_text_builtin(24, fb, x + 72, y + 28, "浮生", GFX_BLACK);
                font_draw_text_builtin(24, fb, x + 72, y + 64, "六记", GFX_BLACK);
            } else {
                font_draw_text_builtin(24, fb, x + 78, y + 26, book->title, GFX_BLACK);
            }
            break;
    }
}

static void render_bookshelf(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const int cover_w = 124;
    const int start_x = 24;
    const int start_y = 58;
    const int step_x = 156;
    const int step_y = 222;
    const int display_count = (int)(sizeof(bookshelf_books) / sizeof(bookshelf_books[0]));
    (void)font;

    bookshelf_status_bar(fb);

    for (int i = 0; i < display_count; i++) {
        int col = i % 3;
        int row = i / 3;
        int cover_x = start_x + col * step_x;
        int cover_y = start_y + row * step_y;
        int cover_h = row == 2 ? 158 : 170;
        bookshelf_display_book_t book = bookshelf_book_for_index(i);
        int selected = i < APP_BOOK_COUNT && app->bookshelf_selection == i;
        int title_size = i == 6 ? 16 : 18;
        int author_size = (i == 5 || row == 2) ? 12 : 14;
        int title_y = cover_y + cover_h + (row == 2 ? 7 : 10);
        int author_y = cover_y + cover_h + (row == 2 ? 30 : 35);

        if (selected) {
            gfx_draw_rounded_rect_thick(fb, cover_x - 2, cover_y - 2, cover_w + 4, cover_h + 4, 8, 2, GFX_BLACK);
        } else {
            gfx_draw_rounded_rect(fb, cover_x, cover_y, cover_w, cover_h, 7, GFX_BLACK);
        }
        draw_bookshelf_cover_art(fb, &book, cover_x, cover_y, cover_w, cover_h, i);

        font_draw_text_aligned_builtin(title_size, fb, cover_x - 12, title_y, cover_w + 24,
                                       book.title, FONT_ALIGN_CENTER, GFX_BLACK);
        font_draw_text_aligned_builtin(author_size, fb, cover_x - 18, author_y, cover_w + 36,
                                       book.author, FONT_ALIGN_CENTER, GFX_BLACK);
    }

    gfx_fill_rect(fb, 0, 768, GFX_WIDTH, 1, GFX_BLACK);
    font_draw_text_builtin(14, fb, 18, 779, "共 12 本书", GFX_BLACK);
    font_draw_text_aligned_builtin(20, fb, 190, 777, 100, "1 / 2", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_builtin(14, fb, 372, 779, "按最近阅读", GFX_BLACK);
    settings_draw_line(fb, 450, 784, 456, 790, 1, GFX_BLACK);
    settings_draw_line(fb, 456, 790, 462, 784, 1, GFX_BLACK);
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

static void reader_settings_doc_icon(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x, y, 16, 20, GFX_BLACK);
    settings_draw_line(fb, x + 11, y, x + 16, y + 5, 1, GFX_BLACK);
    gfx_fill_rect(fb, x + 12, y + 5, 4, 1, GFX_BLACK);
    gfx_fill_rect(fb, x + 4, y + 8, 8, 1, GFX_BLACK);
    gfx_fill_rect(fb, x + 4, y + 12, 8, 1, GFX_BLACK);
    gfx_fill_rect(fb, x + 4, y + 16, 6, 1, GFX_BLACK);
}

static void reader_settings_divider(gfx_framebuffer_t *fb, int y) {
    gfx_fill_rect(fb, 23, y, 434, 1, GFX_BLACK);
}

static void reader_settings_selection(gfx_framebuffer_t *fb, const app_state_t *app, int index, int y, int h) {
    if (app->reader_settings_selection == index) {
        gfx_draw_rect(fb, 27, y + 5, 426, h - 10, GFX_BLACK);
    }
}

static void reader_settings_slider(gfx_framebuffer_t *fb, int x, int y, int w, int steps, int value) {
    int knob_x;
    if (steps < 2) {
        steps = 2;
    }
    if (value < 0) {
        value = 0;
    }
    if (value >= steps) {
        value = steps - 1;
    }
    knob_x = x + value * w / (steps - 1);
    gfx_fill_rect(fb, x, y, w, 2, GFX_BLACK);
    for (int i = 0; i < steps; i++) {
        int tick_x = x + i * w / (steps - 1);
        gfx_fill_rect(fb, tick_x, y + 10, 1, 5, GFX_BLACK);
    }
    settings_fill_circle(fb, knob_x, y + 1, 7, GFX_BLACK);
}

static void reader_settings_segment(gfx_framebuffer_t *fb, int x, int y, int w, int h,
                                    const char **labels, int count, int selected) {
    int seg_w = w / count;
    gfx_draw_rounded_rect_thick(fb, x, y, w, h, 4, 1, GFX_BLACK);
    for (int i = 0; i < count; i++) {
        int seg_x = x + i * seg_w;
        gfx_color_t color = GFX_BLACK;
        if (i > 0) {
            gfx_fill_rect(fb, seg_x, y, 1, h, GFX_BLACK);
        }
        if (i == selected) {
            gfx_fill_rect(fb, seg_x + 1, y + 1, seg_w - 1, h - 2, GFX_BLACK);
            color = GFX_WHITE;
        }
        font_draw_text_aligned_builtin(18, fb, seg_x, y + 6, seg_w, labels[i], FONT_ALIGN_CENTER, color);
    }
}

static void reader_settings_toggle(gfx_framebuffer_t *fb, int x, int y, int enabled) {
    if (enabled) {
        gfx_fill_rounded_rect(fb, x, y, 50, 24, 12, GFX_BLACK);
        settings_fill_circle(fb, x + 38, y + 12, 10, GFX_WHITE);
    } else {
        gfx_draw_rounded_rect_thick(fb, x, y, 50, 24, 12, 1, GFX_BLACK);
        settings_fill_circle(fb, x + 12, y + 12, 8, GFX_WHITE);
        settings_draw_circle(fb, x + 12, y + 12, 8, 1, GFX_BLACK);
    }
}

static void reader_settings_margin_preview(gfx_framebuffer_t *fb, int x, int y, int selected, const char *label) {
    int card_w = 64;
    int card_h = 86;
    int page_x = x + 12;
    int page_y = y + 8;
    int page_w = 38;
    int page_h = 54;
    if (selected) {
        gfx_draw_rounded_rect_thick(fb, x - 1, y - 5, card_w + 2, card_h + 10, 4, 1, GFX_BLACK);
    }
    gfx_draw_rect(fb, page_x, page_y, page_w, page_h, GFX_BLACK);
    for (int i = 0; i < 6; i++) {
        int line_w = 22 - (i % 2) * 5;
        gfx_fill_rect(fb, page_x + 8, page_y + 14 + i * 6, line_w, 1, GFX_BLACK);
    }
    font_draw_text_aligned_builtin(16, fb, x, y + 68, card_w, label, FONT_ALIGN_CENTER, GFX_BLACK);
}

static void render_reader_settings(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const int panel_x = 23;
    const int panel_y = 90;
    const int panel_w = 434;
    const int panel_h = 626;
    const char *font_labels[] = {"大黑", "正圆", "更纱", "唐美"};
    const char *refresh_labels[] = {"普通", "快速", "极速"};
    const char *margin_labels[] = {"窄", "适中", "宽", "自定义"};
    int font_value = app->font_size_index;
    int spacing_value = app->line_spacing_index;
    int margin = app->reader_margin_index;
    (void)font;
    if (font_value < 0 || font_value > 4) {
        font_value = 2;
    }
    if (spacing_value < 0 || spacing_value > 2) {
        spacing_value = 1;
    }
    if (margin < 0 || margin > 3) {
        margin = 1;
    }

    bookshelf_status_bar(fb);
    font_draw_text_aligned_builtin(28, fb, 0, 50, GFX_WIDTH, "阅读设置", FONT_ALIGN_CENTER, GFX_BLACK);

    gfx_draw_rounded_rect_thick(fb, panel_x, panel_y, panel_w, panel_h, 5, 1, GFX_BLACK);
    reader_settings_doc_icon(fb, 40, 105);
    font_draw_text_builtin(20, fb, 66, 105, "排版设置", GFX_BLACK);
    reader_settings_divider(fb, 136);

    reader_settings_selection(fb, app, 0, 136, 66);
    font_draw_text_builtin(20, fb, 40, 164, "字号", GFX_BLACK);
    font_draw_text_builtin(20, fb, 132, 162, "A-", GFX_BLACK);
    reader_settings_slider(fb, 162, 172, 216, 5, font_value);
    font_draw_text_builtin(20, fb, 390, 162, "A+", GFX_BLACK);
    {
        char size_text[8];
        int size_text_w;
        snprintf(size_text, sizeof(size_text), "%d", 28 + font_value * 4);
        size_text_w = font_measure_text_builtin(20, size_text);
        font_draw_text_builtin(20, fb, 444 - size_text_w, 162, size_text, GFX_BLACK);
    }
    reader_settings_divider(fb, 202);

    reader_settings_selection(fb, app, 1, 202, 66);
    font_draw_text_builtin(20, fb, 40, 231, "字体", GFX_BLACK);
    reader_settings_segment(fb, 132, 222, 312, 30, font_labels, 4, app->reader_font_index);
    reader_settings_divider(fb, 268);

    reader_settings_selection(fb, app, 2, 268, 70);
    font_draw_text_builtin(20, fb, 40, 296, "行距", GFX_BLACK);
    reader_settings_slider(fb, 132, 305, 312, 3, spacing_value);
    font_draw_text_builtin(14, fb, 132, 320, "紧凑", GFX_BLACK);
    font_draw_text_aligned_builtin(14, fb, 292, 320, 56, "适中", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_builtin(14, fb, 404, 320, "宽松", GFX_BLACK);
    reader_settings_divider(fb, 338);

    reader_settings_selection(fb, app, 3, 338, 124);
    font_draw_text_builtin(20, fb, 40, 389, "页边距", GFX_BLACK);
    for (int i = 0; i < 4; i++) {
        reader_settings_margin_preview(fb, 132 + i * 78, 360, margin == i, margin_labels[i]);
    }
    reader_settings_divider(fb, 462);

    reader_settings_selection(fb, app, 4, 462, 68);
    font_draw_text_builtin(20, fb, 40, 482, "段首缩进", GFX_BLACK);
    font_draw_text_builtin(14, fb, 40, 512, "每段首行自动缩进两个字符", GFX_BLACK);
    reader_settings_toggle(fb, 392, 490, app->reader_indent_enabled);
    reader_settings_divider(fb, 530);

    reader_settings_selection(fb, app, 5, 530, 68);
    font_draw_text_builtin(20, fb, 40, 550, "加粗", GFX_BLACK);
    font_draw_text_builtin(14, fb, 40, 580, "启用后正文将以加粗字体显示", GFX_BLACK);
    reader_settings_toggle(fb, 392, 558, app->reader_bold_enabled);
    reader_settings_divider(fb, 598);

    reader_settings_selection(fb, app, 6, 598, 58);
    font_draw_text_builtin(20, fb, 40, 619, "翻页动画", GFX_BLACK);
    font_draw_text_builtin(20, fb, 378, 619, app->reader_page_turn_mode == 0 ? "仿真" : app->reader_page_turn_mode == 1 ? "平移" : "无", GFX_BLACK);
    settings_draw_arrow(fb, 432, 619);
    reader_settings_divider(fb, 656);

    reader_settings_selection(fb, app, 7, 656, 60);
    font_draw_text_builtin(20, fb, 40, 680, "刷新模式", GFX_BLACK);
    reader_settings_segment(fb, 216, 672, 228, 30, refresh_labels, 3, app->reader_refresh_mode);

    if (app->reader_settings_selection == 8) {
        gfx_draw_rect(fb, 30, 738, 198, 50, GFX_BLACK);
    }
    gfx_draw_rounded_rect_thick(fb, 32, 744, 194, 40, 4, 1, GFX_BLACK);
    settings_draw_arc(fb, 88, 764, 10, 1, 0, 1, 1);
    settings_draw_line(fb, 79, 760, 84, 755, 2, GFX_BLACK);
    settings_draw_line(fb, 79, 760, 73, 758, 2, GFX_BLACK);
    font_draw_text_aligned_builtin(20, fb, 92, 755, 110, "恢复默认", FONT_ALIGN_CENTER, GFX_BLACK);

    if (app->reader_settings_selection == 9) {
        gfx_draw_rect(fb, 252, 738, 198, 50, GFX_BLACK);
    }
    gfx_fill_rounded_rect(fb, 254, 744, 194, 40, 4, GFX_BLACK);
    settings_draw_circle(fb, 328, 764, 10, 1, GFX_WHITE);
    settings_draw_line(fb, 322, 763, 327, 768, 2, GFX_WHITE);
    settings_draw_line(fb, 327, 768, 335, 758, 2, GFX_WHITE);
    font_draw_text_aligned_builtin(20, fb, 342, 755, 70, "应用", FONT_ALIGN_CENTER, GFX_WHITE);
}

static void reader_catalog_draw_triangle(gfx_framebuffer_t *fb, int x, int y) {
    for (int row = 0; row < 14; row++) {
        int w = row + 1;
        gfx_fill_rect(fb, x, y - row / 2 + row, w, 1, GFX_BLACK);
    }
}

static void render_reader_catalog(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    int chapter_count = reader_library_chapter_count(app->current_book);

    for (int i = 0; i < chapter_count && i < 9; i++) {
        int row_y = 22 + i * 52;
        int page = reader_library_chapter_page(app->current_book, i) + 1;
        char page_text[8];
        if (app->reader_catalog_selection == i) {
            gfx_draw_rounded_rect_thick(fb, 14, row_y + 4, 452, 44, 4, 1, GFX_BLACK);
            reader_catalog_draw_triangle(fb, 28, row_y + 20);
        } else if (i > 0) {
            gfx_fill_rect(fb, 22, row_y, 436, 1, GFX_BLACK);
        }
        snprintf(page_text, sizeof(page_text), "%d", page);
        font_draw_text_builtin(20, fb, app->reader_catalog_selection == i ? 48 : 40, row_y + 17,
                               reader_library_chapter_title(app->current_book, i), GFX_BLACK);
        font_draw_text_aligned_builtin(20, fb, 386, row_y + 17, 60, page_text, FONT_ALIGN_RIGHT, GFX_BLACK);
    }
    home_status_bar(fb, font);
}

static void draw_weather_digit(gfx_framebuffer_t *fb, int x, int y, char digit) {
    static const uint8_t segments[10] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };
    int value = digit - '0';
    int mask;
    if (value < 0 || value > 9) {
        return;
    }
    mask = segments[value];
    if ((mask & 0x01) != 0) gfx_fill_rect(fb, x + 8, y, 34, 7, GFX_BLACK);
    if ((mask & 0x02) != 0) gfx_fill_rect(fb, x + 42, y + 8, 7, 28, GFX_BLACK);
    if ((mask & 0x04) != 0) gfx_fill_rect(fb, x + 42, y + 43, 7, 28, GFX_BLACK);
    if ((mask & 0x08) != 0) gfx_fill_rect(fb, x + 8, y + 72, 34, 7, GFX_BLACK);
    if ((mask & 0x10) != 0) gfx_fill_rect(fb, x, y + 43, 7, 28, GFX_BLACK);
    if ((mask & 0x20) != 0) gfx_fill_rect(fb, x, y + 8, 7, 28, GFX_BLACK);
    if ((mask & 0x40) != 0) gfx_fill_rect(fb, x + 8, y + 36, 34, 7, GFX_BLACK);
}

static void draw_weather_big_number(gfx_framebuffer_t *fb, int x, int y, const char *text) {
    int draw_x = x;
    if (text == NULL) {
        return;
    }
    while (*text != '\0') {
        draw_weather_digit(fb, draw_x, y, *text);
        draw_x += 58;
        text++;
    }
}

static void render_reader(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    char chapter_progress[64];
    char percent_text[8];
    int total_pages = app->book_pages[app->current_book];
    int current_page = app->reader_page + 1;
    int percent = total_pages > 0 ? current_page * 100 / total_pages : 0;
    const font_face_t *body_font = reader_body_font(app);
    int line_height = reader_line_height(app, body_font) + 12;
    (void)font;

    settings_status_bar(fb);

    font_draw_text_box_spaced_family(body_font->size, app->reader_font_index, fb, 28, 70, 424, 690,
                                     reader_library_page_text(app->current_book, app->reader_page),
                                     line_height,
                                     GFX_BLACK);

    gfx_fill_rect(fb, 20, 770, 440, 1, GFX_BLACK);
    snprintf(percent_text, sizeof(percent_text), "%d", percent);
    font_draw_text_builtin(14, fb, 28, 782, percent_text, GFX_BLACK);
    font_draw_text_builtin(14, fb, 50, 782, "%", GFX_BLACK);

    snprintf(chapter_progress, sizeof(chapter_progress), "本章 %d / %d  |  全书还剩 %d 页",
             current_page, total_pages, total_pages > current_page ? total_pages - current_page : 0);
    font_draw_text_aligned_builtin(14, fb, 250, 782, 205, chapter_progress, FONT_ALIGN_RIGHT, GFX_BLACK);

    if (app->reader_menu_open) {
        const font_face_t *menu = font_get_face(FONT_SIZE_18);
        int has_bookmark = app->book_bookmark_pages[app->current_book] == app->reader_page;
        const char *items[] = {
            "继续阅读",
            "查看目录",
            has_bookmark ? "已加书签" : "添加书签",
            "阅读设置",
            "退出到书架"
        };
        /* Centered menu overlay */
        int menu_w = 320;
        int menu_h = 5 * 40 + 24;
        int menu_x = (GFX_WIDTH - menu_w) / 2;
        int menu_y = (GFX_HEIGHT - menu_h) / 2;
        gfx_fill_rect(fb, menu_x, menu_y, menu_w, menu_h, GFX_WHITE);
        gfx_draw_rounded_rect_thick(fb, menu_x, menu_y, menu_w, menu_h, 10, 2, GFX_BLACK);
        for (int i = 0; i < 5; i++) {
            int y = menu_y + 20 + i * 40;
            gfx_color_t color = GFX_BLACK;
            if (app->reader_menu_selection == i) {
                gfx_fill_rect(fb, menu_x + 12, y - 4, menu_w - 24, 32, GFX_BLACK);
                color = GFX_WHITE;
            }
            font_draw_text_aligned(menu, fb, menu_x + 12, y, menu_w - 24, items[i], FONT_ALIGN_CENTER, color);
        }
    }
}

static void render_weather(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const char *cities[] = {"北京市", "上海市", "广州市"};
    const char *conditions[] = {"晴", "多云", "小雨"};
    const int temps[] = {23, 22, 29};
    const int humidities[] = {32, 72, 61};
    const char *wind[] = {"北风 2级", "东风 3级", "南风 2级"};
    int city = app->weather_city_index;
    int scroll = app->weather_scroll;
    if (city < 0 || city > 2) {
        city = 0;
    }

    gfx_fill_rect(fb, 0, 48 - scroll, GFX_WIDTH, 1, GFX_BLACK);

    settings_draw_circle(fb, 46, 96 - scroll, 9, 2, GFX_BLACK);
    settings_fill_circle(fb, 46, 96 - scroll, 3, GFX_BLACK);
    settings_draw_line(fb, 46, 109 - scroll, 40, 99 - scroll, 2, GFX_BLACK);
    font_draw_text_builtin(24, fb, 64, 86 - scroll, cities[city], GFX_BLACK);
    settings_draw_arrow(fb, 138, 90 - scroll);
    {
        char updated[32];
        char time_text[8];
        ui_format_time(time_text, sizeof(time_text));
        snprintf(updated, sizeof(updated), "更新于 %s", time_text);
        font_draw_text_builtin(14, fb, 38, 126 - scroll, updated, GFX_BLACK);
    }

    font_draw_text_builtin(36, fb, 38, 178 - scroll, conditions[city], GFX_BLACK);
    font_draw_text_builtin(18, fb, 38, 236 - scroll, "空气质量", GFX_BLACK);
    gfx_draw_rounded_rect_thick(fb, 116, 233 - scroll, 20, 20, 3, 1, GFX_BLACK);
    font_draw_text_builtin(14, fb, 120, 236 - scroll, "良", GFX_BLACK);
    font_draw_text_builtin(18, fb, 148, 236 - scroll, "58", GFX_BLACK);
    {
        char detail[48];
        snprintf(detail, sizeof(detail), "湿度 %d%%   |   %s", humidities[city], wind[city]);
        font_draw_text_builtin(18, fb, 38, 282 - scroll, detail, GFX_BLACK);
    }

    settings_draw_circle(fb, 358, 150 - scroll, 29, 4, GFX_BLACK);
    gfx_fill_rect(fb, 356, 96 - scroll, 4, 18, GFX_BLACK);
    gfx_fill_rect(fb, 356, 187 - scroll, 4, 18, GFX_BLACK);
    gfx_fill_rect(fb, 304, 149 - scroll, 18, 4, GFX_BLACK);
    gfx_fill_rect(fb, 394, 149 - scroll, 18, 4, GFX_BLACK);
    settings_draw_line(fb, 321, 113 - scroll, 334, 126 - scroll, 4, GFX_BLACK);
    settings_draw_line(fb, 396, 113 - scroll, 383, 126 - scroll, 4, GFX_BLACK);
    settings_draw_line(fb, 321, 189 - scroll, 334, 176 - scroll, 4, GFX_BLACK);
    settings_draw_line(fb, 396, 189 - scroll, 383, 176 - scroll, 4, GFX_BLACK);
    {
        char temp_text[8];
        snprintf(temp_text, sizeof(temp_text), "%d", temps[city]);
        draw_weather_big_number(fb, 302, 232 - scroll, temp_text);
        settings_draw_circle(fb, 412, 274 - scroll, 4, 1, GFX_BLACK);
        font_draw_text_builtin(24, fb, 423, 266 - scroll, "C", GFX_BLACK);
    }

    gfx_fill_rect(fb, 15, 350 - scroll, 450, 1, GFX_BLACK);
    font_draw_text_builtin(20, fb, 30, 368 - scroll, "5日预报", GFX_BLACK);
    {
        const char *dates[] = {"05/20", "05/21", "05/22", "05/23", "05/24"};
        const char *days[] = {"今天", "明天", "周四", "周五", "周六"};
        const char *weather[] = {"晴", "多云", "小雨", "多云", "晴"};
        const char *range[] = {"26C / 14C", "24C / 13C", "20C / 12C", "22C / 13C", "25C / 15C"};
        for (int i = 0; i < 5; i++) {
            int y = 416 + i * 52 - scroll;
            font_draw_text_builtin(18, fb, 32, y, dates[i], GFX_BLACK);
            font_draw_text_builtin(18, fb, 123, y, days[i], GFX_BLACK);
            if (i == 0 || i == 4) {
                settings_draw_circle(fb, 220, y + 8, 8, 2, GFX_BLACK);
                gfx_fill_rect(fb, 219, y - 6, 2, 7, GFX_BLACK);
                gfx_fill_rect(fb, 219, y + 18, 2, 7, GFX_BLACK);
                gfx_fill_rect(fb, 206, y + 7, 7, 2, GFX_BLACK);
                gfx_fill_rect(fb, 228, y + 7, 7, 2, GFX_BLACK);
            } else {
                settings_draw_icon(fb, SETTINGS_ICON_WEATHER, 203, y - 5);
            }
            font_draw_text_builtin(18, fb, 262, y, weather[i], GFX_BLACK);
            font_draw_text_builtin(18, fb, 360, y, range[i], GFX_BLACK);
            if (i < 4) {
                gfx_fill_rect(fb, 16, y + 32, 448, 1, GFX_BLACK);
            }
        }
    }
    gfx_fill_rect(fb, 15, 700 - scroll, 450, 1, GFX_BLACK);

    font_draw_text_builtin(20, fb, 30, 722 - scroll, "今日建议", GFX_BLACK);
    {
        const char *titles[] = {"穿衣", "出行", "紫外线"};
        const char *subtitles[] = {"薄外套", "适宜", "中等"};
        const char *note_line1[] = {"早晚微凉 建议", "天气良好 适宜", "外出请做好防晒"};
        const char *note_line2[] = {"搭配薄外套", "出行", "措施"};
        for (int i = 0; i < 3; i++) {
            int x = 23 + i * 148;
            int card_y = 756 - scroll;
            gfx_draw_rounded_rect_thick(fb, x, card_y, 139, 146, 6, 1, GFX_BLACK);
            if (i == 0) {
                gfx_draw_rect(fb, x + 56, card_y + 14, 28, 30, GFX_BLACK);
                settings_draw_line(fb, x + 56, card_y + 14, x + 68, card_y + 25, 2, GFX_BLACK);
                settings_draw_line(fb, x + 84, card_y + 14, x + 72, card_y + 25, 2, GFX_BLACK);
            } else if (i == 1) {
                gfx_draw_rect(fb, x + 52, card_y + 25, 44, 14, GFX_BLACK);
                settings_draw_circle(fb, x + 62, card_y + 43, 5, 2, GFX_BLACK);
                settings_draw_circle(fb, x + 86, card_y + 43, 5, 2, GFX_BLACK);
            } else {
                settings_draw_circle(fb, x + 70, card_y + 30, 17, 2, GFX_BLACK);
                font_draw_text_builtin(14, fb, x + 58, card_y + 24, "UV", GFX_BLACK);
            }
            font_draw_text_aligned_builtin(18, fb, x, card_y + 61, 139, titles[i], FONT_ALIGN_CENTER, GFX_BLACK);
            font_draw_text_aligned_builtin(16, fb, x, card_y + 88, 139, subtitles[i], FONT_ALIGN_CENTER, GFX_BLACK);
            font_draw_text_aligned_builtin(14, fb, x + 8, card_y + 112, 123, note_line1[i], FONT_ALIGN_CENTER, GFX_BLACK);
            font_draw_text_aligned_builtin(14, fb, x + 8, card_y + 128, 123, note_line2[i], FONT_ALIGN_CENTER, GFX_BLACK);
        }
    }
    font_draw_text_aligned_builtin(14, fb, 0, 940 - scroll, GFX_WIDTH, "— 1 / 1 —", FONT_ALIGN_CENTER, GFX_BLACK);
    home_status_bar(fb, font);
}

static void calendar_draw_chevron(gfx_framebuffer_t *fb, int x, int y, int right) {
    if (right) {
        settings_draw_line(fb, x, y, x + 9, y + 9, 2, GFX_BLACK);
        settings_draw_line(fb, x + 9, y + 9, x, y + 18, 2, GFX_BLACK);
    } else {
        settings_draw_line(fb, x + 9, y, x, y + 9, 2, GFX_BLACK);
        settings_draw_line(fb, x, y + 9, x + 9, y + 18, 2, GFX_BLACK);
    }
}

static void calendar_draw_calendar_icon(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x, y + 4, 24, 24, GFX_BLACK);
    gfx_fill_rect(fb, x + 1, y + 10, 22, 1, GFX_BLACK);
    gfx_fill_rect(fb, x + 6, y, 2, 8, GFX_BLACK);
    gfx_fill_rect(fb, x + 16, y, 2, 8, GFX_BLACK);
    gfx_fill_rect(fb, x + 6, y + 16, 3, 3, GFX_BLACK);
    gfx_fill_rect(fb, x + 14, y + 16, 3, 3, GFX_BLACK);
    gfx_fill_rect(fb, x + 6, y + 22, 3, 3, GFX_BLACK);
    gfx_fill_rect(fb, x + 14, y + 22, 3, 3, GFX_BLACK);
}

static void calendar_draw_sun(gfx_framebuffer_t *fb, int cx, int cy) {
    settings_draw_circle(fb, cx, cy, 7, 2, GFX_BLACK);
    gfx_fill_rect(fb, cx - 1, cy - 17, 2, 6, GFX_BLACK);
    gfx_fill_rect(fb, cx - 1, cy + 11, 2, 6, GFX_BLACK);
    gfx_fill_rect(fb, cx - 17, cy - 1, 6, 2, GFX_BLACK);
    gfx_fill_rect(fb, cx + 11, cy - 1, 6, 2, GFX_BLACK);
    settings_draw_line(fb, cx - 12, cy - 12, cx - 8, cy - 8, 2, GFX_BLACK);
    settings_draw_line(fb, cx + 8, cy - 8, cx + 12, cy - 12, 2, GFX_BLACK);
    settings_draw_line(fb, cx - 12, cy + 12, cx - 8, cy + 8, 2, GFX_BLACK);
    settings_draw_line(fb, cx + 8, cy + 8, cx + 12, cy + 12, 2, GFX_BLACK);
}

static void calendar_draw_moon(gfx_framebuffer_t *fb, int cx, int cy) {
    settings_fill_circle(fb, cx, cy, 11, GFX_BLACK);
    settings_fill_circle(fb, cx + 6, cy - 2, 11, GFX_WHITE);
    settings_draw_circle(fb, cx, cy, 11, 1, GFX_BLACK);
}

static void calendar_draw_checkbox(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x, y, 13, 13, GFX_BLACK);
    settings_draw_line(fb, x + 3, y + 7, x + 6, y + 10, 1, GFX_BLACK);
    settings_draw_line(fb, x + 6, y + 10, x + 11, y + 3, 1, GFX_BLACK);
}

static void calendar_draw_bookmark(gfx_framebuffer_t *fb, int x, int y) {
    gfx_draw_rect(fb, x, y, 13, 18, GFX_BLACK);
    settings_draw_line(fb, x + 1, y + 17, x + 6, y + 12, 1, GFX_BLACK);
    settings_draw_line(fb, x + 6, y + 12, x + 12, y + 17, 1, GFX_BLACK);
}

static const char *calendar_lunar_label(int month, int day) {
    static const char *regular[] = {"初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
                                    "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
                                    "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十", "三十一"};
    if (month == 5 && day == 1) {
        return "劳动节";
    }
    if (month == 6 && day == 1) {
        return "儿童节";
    }
    if (day >= 1 && day <= 31) {
        return regular[day - 1];
    }
    return "";
}

static void render_calendar(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const char *week[] = {"一", "二", "三", "四", "五", "六", "日"};
    char month_title[32];
    char solar_date[48];
    struct tm now_tm;
    struct tm selected_tm;
    int month;
    int year;
    int selected_day = app->calendar_selected_day;
    int grid_x = 12;
    int grid_y = 112;
    int cell_w = 65;
    int cell_h = 49;
    int grid_w = cell_w * 7 + 1;
    int grid_h = cell_h * 6 + 1;
    int first_weekday;
    int month_days;
    int prev_month;
    int prev_year;
    int prev_days;
    (void)font;

    ui_now(&now_tm);
    year = now_tm.tm_year + 1900;
    month = now_tm.tm_mon + 1;
    ui_add_month_offset(&year, &month, app->calendar_month_offset);
    month_days = ui_days_in_month(year, month);
    if (app->calendar_month_offset == 0 || selected_day < 1 || selected_day > month_days) {
        selected_day = now_tm.tm_mday;
        if (selected_day > month_days) {
            selected_day = month_days;
        }
    }
    first_weekday = ui_month_first_weekday_monday(year, month);
    prev_month = month - 1;
    prev_year = year;
    if (prev_month < 1) {
        prev_month = 12;
        prev_year--;
    }
    prev_days = ui_days_in_month(prev_year, prev_month);

    selected_tm = now_tm;
    selected_tm.tm_year = year - 1900;
    selected_tm.tm_mon = month - 1;
    selected_tm.tm_mday = selected_day;
    selected_tm.tm_hour = 12;
    selected_tm.tm_min = 0;
    selected_tm.tm_sec = 0;
    selected_tm.tm_isdst = -1;
    (void)mktime(&selected_tm);

    snprintf(month_title, sizeof(month_title), "%d年%d月", year, month);

    home_status_bar(fb, font);

    calendar_draw_chevron(fb, 42, 50, 0);
    font_draw_text_aligned_builtin(20, fb, 0, 47, GFX_WIDTH, month_title, FONT_ALIGN_CENTER, GFX_BLACK);
    settings_draw_line(fb, 292, 55, 298, 61, 1, GFX_BLACK);
    settings_draw_line(fb, 298, 61, 304, 55, 1, GFX_BLACK);
    calendar_draw_chevron(fb, 428, 50, 1);

    for (int i = 0; i < 7; i++) {
        font_draw_text_aligned_builtin(16, fb, grid_x + i * cell_w, 89, cell_w, week[i], FONT_ALIGN_CENTER, GFX_BLACK);
    }
    for (int r = 0; r <= 6; r++) {
        gfx_fill_rect(fb, grid_x, grid_y + r * cell_h, grid_w, 1, GFX_BLACK);
    }
    for (int c = 0; c <= 7; c++) {
        gfx_fill_rect(fb, grid_x + c * cell_w, grid_y, 1, grid_h, GFX_BLACK);
    }
    for (int i = 0; i < 42; i++) {
        int row = i / 7;
        int col = i % 7;
        int x = grid_x + col * cell_w;
        int y = grid_y + row * cell_h;
        int day_number;
        int cell_month = month;
        int in_month = i >= first_weekday && i < first_weekday + month_days;
        int selected;
        char day[4];
        if (in_month) {
            day_number = i - first_weekday + 1;
        } else if (i < first_weekday) {
            day_number = prev_days - first_weekday + i + 1;
            cell_month = prev_month;
        } else {
            day_number = i - first_weekday - month_days + 1;
            cell_month = month == 12 ? 1 : month + 1;
        }
        selected = in_month && day_number == selected_day;
        snprintf(day, sizeof(day), "%d", day_number);
        if (selected) {
            gfx_fill_rect(fb, x + 1, y + 1, cell_w - 1, cell_h - 1, GFX_BLACK);
        }
        font_draw_text_aligned_builtin(18, fb, x, y + 6, cell_w, day, FONT_ALIGN_CENTER, selected ? GFX_WHITE : GFX_BLACK);
        font_draw_text_aligned_builtin(12, fb, x + 2, y + 29, cell_w - 4, calendar_lunar_label(cell_month, day_number), FONT_ALIGN_CENTER, selected ? GFX_WHITE : GFX_BLACK);
    }

    snprintf(solar_date, sizeof(solar_date), "%d年%d月%d日 %s", year, month, selected_day, ui_weekday_name(selected_tm.tm_wday));
    gfx_draw_rounded_rect_thick(fb, 12, 424, 456, 332, 8, 1, GFX_BLACK);
    gfx_fill_rect(fb, 230, 440, 1, 44, GFX_BLACK);
    calendar_draw_sun(fb, 39, 463);
    font_draw_text_builtin(16, fb, 65, 443, "阳历", GFX_BLACK);
    font_draw_text_builtin(14, fb, 65, 468, solar_date, GFX_BLACK);
    calendar_draw_moon(fb, 251, 463);
    font_draw_text_builtin(16, fb, 277, 443, "农历", GFX_BLACK);
    font_draw_text_builtin(14, fb, 277, 468, "农历 --", GFX_BLACK);

    gfx_fill_rect(fb, 13, 504, 454, 1, GFX_BLACK);
    settings_draw_circle(fb, 33, 529, 8, 1, GFX_BLACK);
    font_draw_text_aligned_builtin(16, fb, 26, 521, 16, "宜", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_builtin(15, fb, 56, 519, "读书 学习 写作 订计划 旅行 散步", GFX_BLACK);
    settings_draw_circle(fb, 33, 562, 8, 1, GFX_BLACK);
    font_draw_text_aligned_builtin(16, fb, 26, 554, 16, "忌", FONT_ALIGN_CENTER, GFX_BLACK);
    font_draw_text_builtin(15, fb, 56, 552, "搬家 动土 嫁娶 安葬 诉讼 争吵", GFX_BLACK);

    gfx_fill_rect(fb, 13, 586, 454, 1, GFX_BLACK);
    calendar_draw_calendar_icon(fb, 28, 600);
    font_draw_text_builtin(18, fb, 62, 603, "今日事件", GFX_BLACK);
    gfx_fill_rect(fb, 28, 638, 420, 1, GFX_BLACK);
    calendar_draw_checkbox(fb, 31, 652);
    font_draw_text_builtin(14, fb, 53, 648, "09:00–10:30 读书计划：《时间简史》第三章", GFX_BLACK);
    calendar_draw_bookmark(fb, 432, 648);
    for (int x = 28; x < 448; x += 8) {
        gfx_fill_rect(fb, x, 674, 4, 1, GFX_BLACK);
    }
    calendar_draw_checkbox(fb, 31, 685);
    font_draw_text_builtin(14, fb, 53, 681, "19:30–20:30 英语复习：Unit 5 词汇与语法", GFX_BLACK);
    calendar_draw_bookmark(fb, 432, 681);

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
        gfx_draw_rounded_rect_thick(fb, PAGE_MARGIN_X, card_y, CONTENT_WIDTH, card_h, 10, 2, GFX_BLACK);
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

/* ====== Settings Page Helper Functions ====== */

/* Draw a section group box with rounded corners */
static void draw_settings_group_box(gfx_framebuffer_t *fb, int x, int y, int w, int h, int radius) {
    gfx_draw_rounded_rect_thick(fb, x, y, w, h, radius, 1, GFX_BLACK);
}

static void settings_fill_circle(gfx_framebuffer_t *fb, int cx, int cy, int r, gfx_color_t color) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                gfx_set_pixel(fb, cx + dx, cy + dy, color);
            }
        }
    }
}

static void settings_draw_circle(gfx_framebuffer_t *fb, int cx, int cy, int r, int thickness, gfx_color_t color) {
    int outer = r * r;
    int inner_r = r - thickness;
    int inner = inner_r > 0 ? inner_r * inner_r : 0;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int d = dx * dx + dy * dy;
            if (d <= outer && d >= inner) {
                gfx_set_pixel(fb, cx + dx, cy + dy, color);
            }
        }
    }
}

static void settings_draw_line(gfx_framebuffer_t *fb, int x0, int y0, int x1, int y1, int thickness, gfx_color_t color) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int radius = thickness / 2;

    for (;;) {
        gfx_fill_rect(fb, x0 - radius, y0 - radius, thickness, thickness, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void settings_draw_arc(gfx_framebuffer_t *fb, int cx, int cy, int r, int q1, int q2, int q3, int q4) {
    int rr = r * r;
    int inner = (r - 2) * (r - 2);
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int d = dx * dx + dy * dy;
            int quadrant = (dx >= 0 && dy < 0 && q1) ||
                           (dx < 0 && dy < 0 && q2) ||
                           (dx < 0 && dy >= 0 && q3) ||
                           (dx >= 0 && dy >= 0 && q4);
            if (quadrant && d <= rr && d >= inner) {
                gfx_set_pixel(fb, cx + dx, cy + dy, GFX_BLACK);
            }
        }
    }
}

static void settings_status_bar(gfx_framebuffer_t *fb) {
    home_status_bar(fb, NULL);
}

static void settings_draw_icon(gfx_framebuffer_t *fb, settings_icon_t icon, int x, int y) {
    switch (icon) {
        case SETTINGS_ICON_WIFI:
            settings_draw_arc(fb, x + 17, y + 25, 20, 1, 1, 0, 0);
            settings_draw_arc(fb, x + 17, y + 25, 13, 1, 1, 0, 0);
            settings_draw_arc(fb, x + 17, y + 25, 7, 1, 1, 0, 0);
            settings_fill_circle(fb, x + 17, y + 25, 2, GFX_BLACK);
            break;
        case SETTINGS_ICON_BLUETOOTH:
            settings_draw_line(fb, x + 16, y + 3, x + 16, y + 31, 3, GFX_BLACK);
            settings_draw_line(fb, x + 16, y + 3, x + 28, y + 14, 3, GFX_BLACK);
            settings_draw_line(fb, x + 28, y + 14, x + 10, y + 25, 3, GFX_BLACK);
            settings_draw_line(fb, x + 16, y + 31, x + 28, y + 20, 3, GFX_BLACK);
            settings_draw_line(fb, x + 28, y + 20, x + 10, y + 9, 3, GFX_BLACK);
            break;
        case SETTINGS_ICON_WEATHER:
            settings_draw_circle(fb, x + 23, y + 12, 6, 2, GFX_BLACK);
            gfx_fill_rect(fb, x + 22, y + 1, 2, 7, GFX_BLACK);
            gfx_fill_rect(fb, x + 29, y + 5, 5, 2, GFX_BLACK);
            settings_draw_circle(fb, x + 10, y + 23, 8, 2, GFX_BLACK);
            settings_draw_circle(fb, x + 20, y + 20, 10, 2, GFX_BLACK);
            gfx_fill_rect(fb, x + 5, y + 22, 26, 10, GFX_WHITE);
            settings_draw_arc(fb, x + 10, y + 23, 8, 1, 1, 1, 0);
            settings_draw_arc(fb, x + 20, y + 20, 10, 1, 1, 0, 0);
            gfx_fill_rect(fb, x + 7, y + 30, 25, 2, GFX_BLACK);
            break;
        case SETTINGS_ICON_CLOCK:
            settings_draw_circle(fb, x + 17, y + 17, 14, 2, GFX_BLACK);
            settings_draw_line(fb, x + 17, y + 8, x + 17, y + 18, 2, GFX_BLACK);
            settings_draw_line(fb, x + 17, y + 18, x + 25, y + 24, 2, GFX_BLACK);
            break;
        case SETTINGS_ICON_LEAF:
            settings_draw_arc(fb, x + 19, y + 16, 18, 0, 1, 1, 0);
            settings_draw_arc(fb, x + 19, y + 16, 17, 1, 0, 0, 1);
            settings_draw_line(fb, x + 6, y + 31, x + 30, y + 7, 2, GFX_BLACK);
            settings_draw_line(fb, x + 12, y + 23, x + 23, y + 23, 2, GFX_BLACK);
            break;
        case SETTINGS_ICON_STORAGE:
            gfx_draw_rect(fb, x + 3, y + 9, 28, 17, GFX_BLACK);
            gfx_draw_rect(fb, x + 5, y + 11, 24, 13, GFX_BLACK);
            gfx_fill_rect(fb, x + 31, y + 14, 3, 7, GFX_BLACK);
            break;
        case SETTINGS_ICON_DICT:
            gfx_draw_rect(fb, x + 8, y + 4, 22, 27, GFX_BLACK);
            font_draw_text(font_get_face(FONT_SIZE_12), fb, x + 11, y + 12, "Aa", GFX_BLACK);
            break;
        case SETTINGS_ICON_INFO:
            settings_draw_circle(fb, x + 17, y + 17, 14, 2, GFX_BLACK);
            settings_fill_circle(fb, x + 17, y + 9, 2, GFX_BLACK);
            gfx_fill_rect(fb, x + 16, y + 15, 3, 12, GFX_BLACK);
            break;
        case SETTINGS_ICON_UPDATE:
            settings_draw_circle(fb, x + 17, y + 17, 14, 2, GFX_BLACK);
            settings_draw_line(fb, x + 17, y + 27, x + 17, y + 7, 2, GFX_BLACK);
            settings_draw_line(fb, x + 17, y + 7, x + 9, y + 15, 2, GFX_BLACK);
            settings_draw_line(fb, x + 17, y + 7, x + 25, y + 15, 2, GFX_BLACK);
            break;
    }
}

static void settings_draw_arrow(gfx_framebuffer_t *fb, int x, int y) {
    settings_draw_line(fb, x, y, x + 7, y + 9, 2, GFX_BLACK);
    settings_draw_line(fb, x + 7, y + 9, x, y + 18, 2, GFX_BLACK);
}

static void settings_draw_value(gfx_framebuffer_t *fb, int x_right, int y, const char *value) {
    if (value != NULL && value[0] != '\0') {
        int value_w = font_measure_text_builtin(18, value);
        font_draw_text_builtin(18, fb, x_right - value_w, y, value, GFX_BLACK);
    }
}

static void settings_draw_item(gfx_framebuffer_t *fb, int y, int row_h, settings_icon_t icon,
                               const char *label, const char *value, int toggle_on) {
    const int icon_x = 42;
    const int label_x = 82;
    const int arrow_x = 435;
    int icon_y = y + (row_h - 34) / 2;
    int label_y = y + (row_h - 20) / 2;
    int value_y = y + (row_h - 18) / 2;

    settings_draw_icon(fb, icon, icon_x, icon_y);
    font_draw_text_builtin(20, fb, label_x, label_y, label, GFX_BLACK);
    settings_draw_arrow(fb, arrow_x, y + (row_h - 18) / 2);

    if (toggle_on >= 0) {
        int toggle_x = 371;
        int toggle_y = y + (row_h - 24) / 2;
        if (toggle_on) {
            gfx_fill_rounded_rect(fb, toggle_x, toggle_y, 46, 24, 12, GFX_BLACK);
            settings_fill_circle(fb, toggle_x + 34, toggle_y + 12, 10, GFX_WHITE);
        } else {
            gfx_draw_rounded_rect_thick(fb, toggle_x, toggle_y, 46, 24, 12, 2, GFX_BLACK);
            settings_fill_circle(fb, toggle_x + 12, toggle_y + 12, 8, GFX_BLACK);
        }
    } else {
        settings_draw_value(fb, arrow_x - 18, value_y, value);
    }
}

static void settings_divider(gfx_framebuffer_t *fb, int y) {
    gfx_fill_rect(fb, 34, y, 412, 1, GFX_BLACK);
}

static void draw_section_header(gfx_framebuffer_t *fb, int x, int y, const char *text) {
    font_draw_text_builtin(20, fb, x, y, text, GFX_BLACK);
}

static void render_settings(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    const char *cities[] = {"北京市", "上海市", "杭州市"};
    int city = app->weather_city_index;
    int scroll = app->settings_scroll;
    const int title_y = 80 - scroll;
    const int section_x = 32;
    const int group_x = 27;
    const int group_w = 428;
    const int row_h = 56;
    if (city < 0 || city > 2) {
        city = 0;
    }

    font_draw_text_builtin(36, fb, 30, title_y, "系统设置", GFX_BLACK);

    draw_section_header(fb, section_x, 150 - scroll, "网络与连接");
    draw_settings_group_box(fb, group_x, 184 - scroll, group_w, row_h * 2, 7);
    settings_draw_item(fb, 184 - scroll, row_h, SETTINGS_ICON_WIFI, "Wi-Fi",
                       app->wifi_connected ? "已连接 Reader_5G" : "未连接", -1);
    settings_divider(fb, 184 + row_h - scroll);
    settings_draw_item(fb, 184 + row_h - scroll, row_h, SETTINGS_ICON_BLUETOOTH, "蓝牙", "已关闭", -1);

    draw_section_header(fb, section_x, 330 - scroll, "系统与时间");
    draw_settings_group_box(fb, group_x, 364 - scroll, group_w, row_h * 2, 7);
    settings_draw_item(fb, 364 - scroll, row_h, SETTINGS_ICON_WEATHER, "天气城市", cities[city], -1);
    settings_divider(fb, 364 + row_h - scroll);
    {
        char sync_text[32];
        char time_text[8];
        ui_format_time(time_text, sizeof(time_text));
        snprintf(sync_text, sizeof(sync_text), "已同步 %s", time_text);
        settings_draw_item(fb, 364 + row_h - scroll, row_h, SETTINGS_ICON_CLOCK, "时间同步", sync_text, -1);
    }

    draw_section_header(fb, section_x, 510 - scroll, "电源与性能");
    draw_settings_group_box(fb, group_x, 544 - scroll, group_w, row_h * 2, 7);
    settings_draw_item(fb, 544 - scroll, row_h, SETTINGS_ICON_LEAF, "电池节能模式", "", app->power_saving_enabled ? 1 : 0);
    settings_divider(fb, 544 + row_h - scroll);
    settings_draw_item(fb, 544 + row_h - scroll, row_h, SETTINGS_ICON_STORAGE, "存储空间", "已使用 12.6GB / 32GB", -1);

    draw_section_header(fb, section_x, 690 - scroll, "内容与服务");
    draw_settings_group_box(fb, group_x, 724 - scroll, group_w, row_h, 7);
    settings_draw_item(fb, 724 - scroll, row_h, SETTINGS_ICON_DICT, "字典管理", "已安装 3 个字典", -1);

    draw_section_header(fb, section_x, 820 - scroll, "关于与更新");
    draw_settings_group_box(fb, group_x, 854 - scroll, group_w, row_h * 2, 7);
    settings_draw_item(fb, 854 - scroll, row_h, SETTINGS_ICON_INFO, "关于设备", "型号 Reader X", -1);
    settings_divider(fb, 854 + row_h - scroll);
    settings_draw_item(fb, 854 + row_h - scroll, row_h, SETTINGS_ICON_UPDATE, "软件更新", "当前版本 1.2.0", -1);

    home_status_bar(fb, font);
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
        case APP_PAGE_READER_CATALOG:
            render_reader_catalog(fb, app, font);
            break;
        case APP_PAGE_READER_SETTINGS:
            render_reader_settings(fb, app, font);
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
