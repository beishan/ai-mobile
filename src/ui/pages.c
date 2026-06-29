#include "ui/pages.h"
#include "ui/icons.h"
#include "app/reader_library.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

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
    const char *conditions[] = {"晴", "多云", "雨", "雪"};
    const int temps[] = {26, 22, 29};
    int city = app->weather_city_index;
    if (city < 0 || city > 2) {
        city = 0;
    }

    /* Outer border - rounded rectangle with thicker border and larger radius */
    gfx_draw_rounded_rect_thick(fb, x, y, w, h, 12, 3, GFX_BLACK);

    /* Vertical divider — left ~60%, right ~40% */
    int divider_x = x + w * 3 / 5;
    gfx_fill_rect(fb, divider_x, y + 8, 1, h - 16, GFX_BLACK);

    /* --- Left: weather with icon based on weather type --- */
    /* Select weather icon based on weather_type: 0=sunny, 1=cloudy, 2=rainy, 3=snowy */
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
    
    /* Draw weather icon at left side */
    int icon_size = 64;
    int icon_x = x + 16;
    int icon_y = y + (h - icon_size) / 2;
    ui_draw_icon(fb, weather_icon, icon_x, icon_y, 0);

    /* Weather text to the right of the icon */
    int text_x = icon_x + icon_size + 8;
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%d C %s", temps[city], conditions[app->weather_type]);  // Use " C" instead of "°C" for font compatibility
    font_draw_text(temp_font, fb, text_x, y + h / 2 - temp_font->size / 2 - 6, temp_str, GFX_BLACK);  // Centered vertically
    font_draw_text(city_font, fb, text_x, y + h / 2 + 6, cities[city], GFX_BLACK);  // Below temperature

    /* --- Right: large clock with lunar date --- */
    const char *clock_text = "09:41";
    font_draw_text_aligned(clock_font, fb, divider_x, y + h / 2 - clock_font->size / 2 - 8,
                            w - (divider_x - x), clock_text, FONT_ALIGN_CENTER, GFX_BLACK);
    
    /* Lunar calendar date below clock */
    const char *lunar_date = "农历 五月初十";
    font_draw_text_aligned(small, fb, divider_x, y + h / 2 + small->size / 2 + 2,
                            w - (divider_x - x), lunar_date, FONT_ALIGN_CENTER, GFX_BLACK);
}

static void app_tile(gfx_framebuffer_t *fb, const font_t *font, ui_icon_kind_t icon, int x, int y, int w, int h, const char *label, int selected) {
    const font_face_t *label_font = font_get_face(FONT_SIZE_18);
    /* Use 52 as the uniform icon bounding box size (largest icon is reader/weather at ~52px) */
    int icon_bbox_size = 52;
    int label_height = label_font->size;
    int total_content_height = icon_bbox_size + 8 + label_height;  // icon bbox + spacing + text
    
    /* Center the entire content vertically */
    int content_start_y = y + (h - total_content_height) / 2;
    
    /* Center the icon bounding box horizontally */
    int icon_x = x + (w - icon_bbox_size) / 2;
    int icon_y = content_start_y;
    
    (void)font;
    if (selected) {
        gfx_draw_rounded_rect_thick(fb, x, y, w, h, 12, 3, GFX_BLACK);
    }
    ui_draw_icon(fb, icon, icon_x, icon_y, 0);
    
    /* Center text below icon */
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
    const int card_h = 120;  // Increased from 96 to 120 for better spacing
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
    const font_face_t *small = font_get_face(FONT_SIZE_14);
    /* Check if external font is available for cover decoration */
    const external_font_t *cover_ext = font_manager_get(24);
    int cover_font_size = cover_ext != NULL ? cover_ext->height : 24;
    int cover_font_width = cover_ext != NULL ? cover_ext->width : 24;
    
    /* Grid layout: 3 columns x N rows */
    const int cols = 3;
    const int card_w = 128;      /* Card width including cover + text */
    const int card_h = 280;      /* Total card height (cover + title + author + progress) */
    const int cover_h = 220;     /* Cover area height - larger for decorative pattern */
    const int gap_x = 24;        /* Horizontal gap between cards */
    const int gap_y = 20;        /* Vertical gap between cards */
    
    /* Calculate grid position to center it in content area */
    int total_grid_w = cols * card_w + (cols - 1) * gap_x;
    int start_x = PAGE_MARGIN_X + (CONTENT_WIDTH - total_grid_w) / 2;
    int start_y = BODY_TOP + 10;
    
    int book_count = reader_library_book_count();
    int books_per_page = 9;  /* 3x3 grid per page */
    int max_books_to_show = book_count < books_per_page ? book_count : books_per_page;

    title_bar(fb, font, "书架", NULL);
    
    for (int i = 0; i < max_books_to_show; i++) {
        int col = i % cols;
        int row = i / cols;
        int card_x = start_x + col * (card_w + gap_x);
        int card_y = start_y + row * (card_h + gap_y);
        
        int percent = (app->book_current_pages[i] + 1) * 100 / app->book_pages[i];
        const reader_book_t *book = reader_library_book(i);

        /* Draw cover frame with rounded corners */
        int cover_x = card_x;
        int cover_y = card_y;
        int cover_w = card_w;
        int cover_r = 8;
        
        if (app->bookshelf_selection == i) {
            /* Selected book gets thick rounded border */
            gfx_draw_rounded_rect_thick(fb, cover_x - 2, cover_y - 2, cover_w + 4, cover_h + 4, 12, 3, GFX_BLACK);
        } else {
            /* Regular cover border */
            gfx_draw_rounded_rect(fb, cover_x, cover_y, cover_w, cover_h, cover_r, GFX_BLACK);
        }

        /* Decorative cover pattern - large first character of title */
        /* Center the large character in the cover area */
        const char *first_char = book->title;
        if (first_char != NULL && first_char[0] != '\0') {
            char single_char[5] = {0};
            /* Handle UTF-8 multi-byte characters */
            int byte_len = 1;
            if ((unsigned char)first_char[0] >= 0xE0) {
                byte_len = 3;  /* Chinese character */
            } else if ((unsigned char)first_char[0] >= 0xC0) {
                byte_len = 2;
            }
            strncpy(single_char, first_char, byte_len);
            single_char[byte_len] = '\0';
            
            /* Draw large centered character as decorative element using external font */
            int char_x = cover_x + (cover_w - cover_font_width) / 2;
            int char_y = cover_y + (cover_h - cover_font_size) / 2 - 20;  /* Slightly above center */
            font_draw_text_auto(24, fb, char_x, char_y, single_char, GFX_BLACK);
        }
        
        /* File type icon in top-left corner (smaller and less prominent) */
        draw_file_type_icon(fb, small, cover_x + 6, cover_y + 6, book->file_type);
        
        /* "Recent" badge in top-right if this is the recently read book */
        if (app->recent_book == i) {
            font_draw_text(small, fb, cover_x + cover_w - 50, cover_y + 10, "最近", GFX_BLACK);
        }

        /* Title centered below cover (uses external font if available) */
        int title_y = cover_y + cover_h + 10;
        font_draw_text_aligned_auto(22, fb, card_x, title_y, card_w, book->title, FONT_ALIGN_CENTER, GFX_BLACK);
        
        /* Author centered below title */
        int author_y = title_y + 18 + 4;  /* Match original spacing: normal font size + gap */
        font_draw_text_aligned_auto(18, fb, card_x, author_y, card_w, book->author, FONT_ALIGN_CENTER, GFX_BLACK);
        
        /* Progress percentage at bottom of card */
        char progress[12];
        snprintf(progress, sizeof(progress), "%d%%", percent);
        int progress_y = author_y + 18 + 6;  /* Use size 18 for author height */
        font_draw_text_aligned_auto(14, fb, card_x, progress_y, card_w, progress, FONT_ALIGN_CENTER, GFX_BLACK);
    }
    
    /* Bottom status bar showing total books and pagination */
    int footer_y = BODY_BOTTOM - 40;
    char footer_text[64];
    snprintf(footer_text, sizeof(footer_text), "共 %d 本书", book_count);
    font_draw_text_auto(14, fb, PAGE_MARGIN_X, footer_y, footer_text, GFX_BLACK);
    
    /* Page indicator in center */
    int page_indicator_y = footer_y;
    int page_indicator_x = (GFX_WIDTH - 60) / 2;
    char page_text[16];
    snprintf(page_text, sizeof(page_text), "< 1/1 >");
    font_draw_text_aligned_auto(14, fb, page_indicator_x, page_indicator_y, 60, page_text, FONT_ALIGN_CENTER, GFX_BLACK);
    
    /* Sort option on right */
    font_draw_text_auto(14, fb, GFX_WIDTH - PAGE_MARGIN_X - 100, footer_y, "按最近阅读", GFX_BLACK);
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

    /* Reader body text fills the content area below the title bar */
    int reader_y = BODY_TOP + 8;
    int reader_h = BODY_HEIGHT - 24;  /* leave room for bottom progress bar */
    font_draw_text_box_spaced(body_font, fb, PAGE_MARGIN_X, reader_y, CONTENT_WIDTH, reader_h,
                              reader_library_page_text(app->current_book, app->reader_page),
                              reader_line_height(app, body_font),
                              GFX_BLACK);

    /* Bottom reading progress strip */
    {
        int bar_y = BODY_BOTTOM - 8;
        int bar_h = 6;
        int total_pages = app->book_pages[app->current_book];
        int fill_w = CONTENT_WIDTH;
        if (total_pages > 1) {
            fill_w = CONTENT_WIDTH * (app->reader_page + 1) / total_pages;
        }
        gfx_fill_rect(fb, PAGE_MARGIN_X, bar_y, CONTENT_WIDTH, bar_h, GFX_WHITE);
        gfx_draw_rect(fb, PAGE_MARGIN_X, bar_y, CONTENT_WIDTH, bar_h, GFX_BLACK);
        gfx_fill_rect(fb, PAGE_MARGIN_X + 1, bar_y + 1, fill_w > 2 ? fill_w - 2 : 0, bar_h - 2, GFX_BLACK);
    }

    if (app->reader_menu_open) {
        const font_face_t *menu = font_get_face(FONT_SIZE_18);
        int has_bookmark = app->book_bookmark_pages[app->current_book] == app->reader_page;
        const char *items[] = {
            "继续阅读",
            "查看目录",
            has_bookmark ? "已加书签" : "添加书签",
            "退出到书架"
        };
        /* Centered menu overlay */
        int menu_w = 320;
        int menu_h = 4 * 40 + 24;
        int menu_x = (GFX_WIDTH - menu_w) / 2;
        int menu_y = (GFX_HEIGHT - menu_h) / 2;
        gfx_fill_rect(fb, menu_x, menu_y, menu_w, menu_h, GFX_WHITE);
        gfx_draw_rounded_rect_thick(fb, menu_x, menu_y, menu_w, menu_h, 10, 2, GFX_BLACK);
        for (int i = 0; i < 4; i++) {
            int y = menu_y + 20 + i * 40;
            gfx_color_t color = GFX_BLACK;
            if (app->reader_menu_selection == i) {
                gfx_fill_rect(fb, menu_x + 12, y - 4, menu_w - 24, 32, GFX_BLACK);
                color = GFX_WHITE;
            }
            font_draw_text_aligned(menu, fb, menu_x + 12, y, menu_w - 24, items[i], FONT_ALIGN_CENTER, color);
        }
        if (app->reader_catalog_open) {
            int chapter_count = reader_library_chapter_count(app->current_book);
            int cat_w = 360;
            int cat_h = chapter_count * 40 + 60;
            int cat_x = (GFX_WIDTH - cat_w) / 2;
            int cat_y = (GFX_HEIGHT - cat_h) / 2;
            gfx_fill_rect(fb, cat_x, cat_y, cat_w, cat_h, GFX_WHITE);
            gfx_draw_rounded_rect_thick(fb, cat_x, cat_y, cat_w, cat_h, 10, 2, GFX_BLACK);
            font_draw_text_aligned(font_get_face(FONT_SIZE_18), fb, cat_x + 16, cat_y + 16, cat_w - 32, "目录", FONT_ALIGN_CENTER, GFX_BLACK);
            for (int i = 0; i < chapter_count; i++) {
                int y = cat_y + 52 + i * 40;
                gfx_color_t color = GFX_BLACK;
                if (app->reader_catalog_selection == i) {
                    gfx_fill_rect(fb, cat_x + 12, y - 4, cat_w - 24, 32, GFX_BLACK);
                    color = GFX_WHITE;
                }
                font_draw_text_aligned(menu, fb, cat_x + 16, y, cat_w - 32, reader_library_chapter_title(app->current_book, i), FONT_ALIGN_CENTER, color);
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
            gfx_draw_rounded_rect_thick(fb, cx, card_y, card_w, card_h, 8, 2, GFX_BLACK);
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
        gfx_draw_rounded_rect_thick(fb, PAGE_MARGIN_X, aq_y + 40, CONTENT_WIDTH, 16, 4, 1, GFX_BLACK);
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
                gfx_draw_rounded_rect_thick(fb, cx + 8, cy + 8, col_w - 16, cell_h - 16, 6, 2, GFX_BLACK);
                gfx_draw_rounded_rect_thick(fb, cx + 10, cy + 10, col_w - 20, cell_h - 20, 4, 1, GFX_BLACK);
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
        gfx_draw_rounded_rect_thick(fb, PAGE_MARGIN_X, box_y, CONTENT_WIDTH, box_h, 10, 2, GFX_BLACK);
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

/* Draw a settings list item with icon, label, value, and arrow */
static void draw_settings_list_item(gfx_framebuffer_t *fb, int x, int y, int w, int h,
                                     const char *icon_char, const char *label,
                                     const char *value, int selected) {
    /* Background highlight for selected item */
    if (selected) {
        gfx_fill_rect(fb, x + 2, y + 2, w - 4, h - 4, GFX_BLACK);
    }
    
    /* Icon on the left (optional) */
    int label_x = x + 20;
    if (icon_char != NULL && icon_char[0] != '\0') {
        int icon_y = y + (h - 24) / 2;
        font_draw_text_auto(24, fb, label_x, icon_y, icon_char, selected ? GFX_WHITE : GFX_BLACK);
        label_x += 32;  /* Space for icon */
    }
    
    /* Label */
    int label_y = y + (h - 20) / 2;
    font_draw_text_auto(20, fb, label_x, label_y, label, selected ? GFX_WHITE : GFX_BLACK);
    
    /* Arrow '>' on far right */
    int arrow_x = x + w - 24;
    int arrow_y = y + (h - 18) / 2;
    font_draw_text_auto(18, fb, arrow_x, arrow_y, ">", selected ? GFX_WHITE : GFX_BLACK);
    
    /* Value before arrow (right-aligned) */
    if (value != NULL && value[0] != '\0') {
        int value_width = font_measure_text_auto(18, value);
        int value_x = arrow_x - value_width - 8;  /* Position before arrow with small spacing */
        int value_y = y + (h - 18) / 2;
        
        /* Always keep value right-aligned before arrow, even if it overlaps with label */
        /* This is common UI pattern for settings pages */
        font_draw_text_auto(18, fb, value_x, value_y, value, selected ? GFX_WHITE : GFX_BLACK);
    }
}

/* Draw section header */
static void draw_section_header(gfx_framebuffer_t *fb, int x, int y, const char *text) {
    font_draw_text_auto(20, fb, x, y, text, GFX_BLACK);
}

/* Main settings page rendering */
static void render_settings(gfx_framebuffer_t *fb, const app_state_t *app, const font_t *font) {
    (void)font;  /* Suppress unused parameter warning */
    
    const char *cities[] = {"北京", "上海", "杭州"};
    
    int current_y = BODY_TOP + 10;
    int item_height = 46;
    int group_gap = 12;
    int header_gap = 26;
    
    /* Title: 系统设置 */
    font_draw_text_auto(24, fb, PAGE_MARGIN_X, current_y, "系统设置", GFX_BLACK);
    current_y += 34;
    
    /* === Group 1: 网络与连接 === */
    draw_section_header(fb, PAGE_MARGIN_X, current_y, "网络与连接");
    current_y += header_gap;
    
    int group1_items = 2;
    int group1_h = group1_items * item_height + (group1_items - 1) * 0;
    int group1_y = current_y;
    draw_settings_group_box(fb, PAGE_MARGIN_X, group1_y, CONTENT_WIDTH, group1_h, 8);
    
    /* WiFi item */
    char wifi_value[32];
    snprintf(wifi_value, sizeof(wifi_value), "%s", app->wifi_connected ? "已连接 Reader5G" : "未连接");
    draw_settings_list_item(fb, PAGE_MARGIN_X + 10, group1_y, CONTENT_WIDTH - 20, item_height,
                            NULL, "WiFi", app->wifi_connected ? wifi_value : "", 0);
    
    /* Divider line (between items) */
    int divider_y = group1_y + item_height;
    gfx_fill_rect(fb, PAGE_MARGIN_X + 70, divider_y, CONTENT_WIDTH - 140, 1, GFX_BLACK);
    
    /* Bluetooth item */
    draw_settings_list_item(fb, PAGE_MARGIN_X + 10, divider_y, CONTENT_WIDTH - 20, item_height,
                            NULL, "蓝牙", "关闭", 0);
    
    current_y += group1_h + group_gap;
    
    /* === Group 2: 系统与时间 === */
    draw_section_header(fb, PAGE_MARGIN_X, current_y, "系统与时间");
    current_y += header_gap;
    
    int group2_items = 2;
    int group2_h = group2_items * item_height + (group2_items - 1) * 0;
    int group2_y = current_y;
    draw_settings_group_box(fb, PAGE_MARGIN_X, group2_y, CONTENT_WIDTH, group2_h, 8);
    
    /* Weather city */
    draw_settings_list_item(fb, PAGE_MARGIN_X + 10, group2_y, CONTENT_WIDTH - 20, item_height,
                            NULL, "天气城市", cities[app->weather_city_index], 0);
    
    /* Divider */
    divider_y = group2_y + item_height;
    gfx_fill_rect(fb, PAGE_MARGIN_X + 70, divider_y, CONTENT_WIDTH - 140, 1, GFX_BLACK);
    
    /* Time sync */
    draw_settings_list_item(fb, PAGE_MARGIN_X + 10, divider_y, CONTENT_WIDTH - 20, item_height,
                            NULL, "时间同步", "已同步 09:41", 0);
    
    current_y += group2_h + group_gap;
    
    /* === Group 3: 电源与性能 === */
    draw_section_header(fb, PAGE_MARGIN_X, current_y, "电源与性能");
    current_y += header_gap;
    
    int group3_items = 2;
    int group3_h = group3_items * item_height + (group3_items - 1) * 0;
    int group3_y = current_y;
    draw_settings_group_box(fb, PAGE_MARGIN_X, group3_y, CONTENT_WIDTH, group3_h, 8);
    
    /* Battery saving mode with toggle */
    {
        int item_y = group3_y;
        const font_face_t *toggle_font = font_get_face(FONT_SIZE_20);
        int label_y = item_y + (item_height - toggle_font->size) / 2;
        
        /* No icon for battery saving mode, just label and toggle */
        int label_x = PAGE_MARGIN_X + 30;
        font_draw_text_auto(20, fb, label_x, label_y, "电池节能模式", GFX_BLACK);
        
        /* Toggle switch (positioned before arrow) */
        int toggle_w = 48;
        int toggle_h = 24;
        int toggle_y = item_y + (item_height - toggle_h) / 2;
        int arrow_x = PAGE_MARGIN_X + CONTENT_WIDTH - 20 - 24;  /* Arrow position */
        int toggle_x = arrow_x - toggle_w - 10;  /* Toggle before arrow with spacing */
        
        /* Toggle background */
        if (app->power_saving_enabled) {
            gfx_fill_rect(fb, toggle_x, toggle_y, toggle_w, toggle_h, GFX_BLACK);
        } else {
            gfx_draw_rect(fb, toggle_x, toggle_y, toggle_w, toggle_h, GFX_BLACK);
        }
        
        /* Toggle knob */
        int knob_r = 10;
        int knob_x = app->power_saving_enabled ? (toggle_x + toggle_w - knob_r - 2) : (toggle_x + knob_r + 2);
        int knob_y = toggle_y + toggle_h / 2;
        
        /* Draw filled circle as knob */
        gfx_color_t knob_color = app->power_saving_enabled ? GFX_WHITE : GFX_BLACK;
        for (int dy = -knob_r; dy <= knob_r; dy++) {
            for (int dx = -knob_r; dx <= knob_r; dx++) {
                if (dx*dx + dy*dy <= knob_r*knob_r) {
                    gfx_set_pixel(fb, knob_x + dx, knob_y + dy, knob_color);
                }
            }
        }
        
        /* Arrow after toggle */
        int arrow_y = item_y + (item_height - 18) / 2;
        font_draw_text_auto(18, fb, arrow_x, arrow_y, ">", GFX_BLACK);
    }
    
    /* Divider */
    divider_y = group3_y + item_height;
    gfx_fill_rect(fb, PAGE_MARGIN_X + 70, divider_y, CONTENT_WIDTH - 140, 1, GFX_BLACK);
    
    /* Storage space */
    draw_settings_list_item(fb, PAGE_MARGIN_X + 10, divider_y, CONTENT_WIDTH - 20, item_height,
                            NULL, "存储空间", "12.6GB/32GB", 0);
    
    current_y += group3_h + group_gap;
    
    /* === Group 4: 内容与服务 === */
    draw_section_header(fb, PAGE_MARGIN_X, current_y, "内容与服务");
    current_y += header_gap;
    
    int group4_items = 1;
    int group4_h = group4_items * item_height;
    int group4_y = current_y;
    draw_settings_group_box(fb, PAGE_MARGIN_X, group4_y, CONTENT_WIDTH, group4_h, 8);
    
    /* Dictionary management */
    draw_settings_list_item(fb, PAGE_MARGIN_X + 10, group4_y, CONTENT_WIDTH - 20, item_height,
                            NULL, "字典管理", "3个字典", 0);
    
    current_y += group4_h + group_gap;
    
    /* === Group 5: 关于与更新 === */
    draw_section_header(fb, PAGE_MARGIN_X, current_y, "关于与更新");
    current_y += header_gap;
    
    int group5_items = 2;
    int group5_h = group5_items * item_height + (group5_items - 1) * 0;
    int group5_y = current_y;
    draw_settings_group_box(fb, PAGE_MARGIN_X, group5_y, CONTENT_WIDTH, group5_h, 8);
    
    /* About device */
    draw_settings_list_item(fb, PAGE_MARGIN_X + 10, group5_y, CONTENT_WIDTH - 20, item_height,
                            NULL, "关于设备", "Reader X", 0);
    
    /* Divider */
    divider_y = group5_y + item_height;
    gfx_fill_rect(fb, PAGE_MARGIN_X + 70, divider_y, CONTENT_WIDTH - 140, 1, GFX_BLACK);
    
    /* Software update */
    draw_settings_list_item(fb, PAGE_MARGIN_X + 10, divider_y, CONTENT_WIDTH - 20, item_height,
                            NULL, "软件更新", "V1.2.0", 0);
    
    current_y += group5_h;
    
    /* Bottom progress indicator */
    font_draw_text_auto(24, fb, PAGE_MARGIN_X, GFX_HEIGHT - 30, "20%", GFX_BLACK);
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
