#include "app/reader_library.h"

#include "app/app_state.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define READER_CHAPTER_COUNT 3
#define READER_MAX_PAGES 6
#define READER_PAGE_TEXT_MAX 256
#define READER_AUTO_PAGE_BYTES 150
#define READER_SOURCE_TEXT_MAX 1536

typedef struct {
    reader_book_t info;
    const char *source;
    const char *chapters[READER_CHAPTER_COUNT];
    int chapter_pages[READER_CHAPTER_COUNT];
} reader_book_content_t;

static const reader_book_content_t books[APP_BOOK_COUNT] = {
    {
        {"三体", "刘慈欣", "1.2MB", "TXT", "第三章"},
        "第三章 静夜。城市的灯光在远处闪烁，像一片低垂的星空。他翻到下一页，等待墨水屏完成一次干净的黑白刷新。"
        "\f叶文洁把旧资料重新摊开。纸页边缘有潮气留下的痕迹，字迹却仍然清楚，像某种从年代深处传来的回声。"
        "\f观测站外的风停了。屏幕上的数字缓慢跳动，每一次变化都像在提醒人类，宇宙并不因为沉默而空无一物。"
        "\f他合上记录本，又很快打开。那些问题没有答案，但每一次阅读都让问题变得更具体，也更难回避。"
        "\f天色微亮时，远山只剩黑白两层轮廓。新的页面在墨水屏上稳定下来，像给漫长夜晚盖上一个句号。",
        {"第一章  开端", "第二章  转折", "第三章  回声"},
        {0, 2, 4}
    },
    {
        {"百年孤独", "马尔克斯", "0.8MB", "EPUB", "马孔多"},
        "马孔多的下午潮湿而明亮。多年以后，他仍会想起那阵穿过院子的热风，以及风里混着的木屑气味。"
        "\f小镇的钟声很慢，像从纸页深处传来。每个人都在等待某件事发生，却没人能说清那件事的形状。"
        "\f雨季让道路变得柔软。她把信放进抽屉，仿佛只要锁住木盒，时间也能被暂时收好。"
        "\f夜晚降临后，屋里的灯光贴着墙面移动。那些名字一个接一个出现，又像河水一样从记忆里退去。",
        {"第一章  小镇", "第二章  雨季", "第三章  家族"},
        {0, 1, 3}
    },
    {
        {"活着", "余华", "0.4MB", "MOBI", "田埂"},
        "田野安静下来，风从麦穗上掠过。他把书合上又打开，仿佛那些旧日子仍然在黑白之间缓缓移动。"
        "\f牛在前面慢慢走，人的影子落在后面。路并不长，可每一步都像要把一生重新量过。"
        "\f傍晚的炊烟升起来，村口只剩几声很轻的说话声。日子没有停下，只是换了一种更慢的走法。",
        {"第一章  回家", "第二章  田埂", "第三章  黄昏"},
        {0, 1, 2}
    }
};

static char page_cache[APP_BOOK_COUNT][READER_MAX_PAGES][READER_PAGE_TEXT_MAX];
static int page_cache_ready[APP_BOOK_COUNT];
static char source_overrides[APP_BOOK_COUNT][READER_SOURCE_TEXT_MAX];
static int source_override_ready[APP_BOOK_COUNT];

static const char *source_for_book(int book_index) {
    if (source_override_ready[book_index]) {
        return source_overrides[book_index];
    }
    return books[book_index].source;
}

static int source_has_explicit_pages(const char *source) {
    if (source == NULL || source[0] == '\0') {
        return 0;
    }
    for (const char *p = source; *p != '\0'; p++) {
        if (*p == '\f') {
            return 1;
        }
    }
    return 0;
}

static int is_utf8_continuation(unsigned char value) {
    return (value & 0xc0u) == 0x80u;
}

static int utf8_safe_length(const char *start, int max_len) {
    int length = max_len;
    while (length > 0 && is_utf8_continuation((unsigned char)start[length])) {
        length--;
    }
    return length > 0 ? length : max_len;
}

static int auto_page_segment_length(const char *start) {
    int len = 0;
    int last_break = -1;
    while (start[len] != '\0' && len < READER_AUTO_PAGE_BYTES) {
        if (start[len] == '\n' || start[len] == ' ') {
            last_break = len + 1;
        }
        len++;
    }
    if (start[len] == '\0') {
        return len;
    }
    if (last_break > 32) {
        return utf8_safe_length(start, last_break);
    }
    return utf8_safe_length(start, len);
}

static int count_pages_in_source(const char *source) {
    int count = 1;
    const char *p = source;
    if (source == NULL || source[0] == '\0') {
        return 0;
    }
    if (source_has_explicit_pages(source)) {
        for (; *p != '\0'; p++) {
            if (*p == '\f') {
                count++;
            }
        }
        return count > READER_MAX_PAGES ? READER_MAX_PAGES : count;
    }
    while (*p != '\0' && count < READER_MAX_PAGES) {
        int segment_len = auto_page_segment_length(p);
        p += segment_len;
        while (*p == '\n' || *p == ' ') {
            p++;
        }
        if (*p != '\0') {
            count++;
        }
    }
    return count > READER_MAX_PAGES ? READER_MAX_PAGES : count;
}

static void copy_page_segment(char *dest, const char *start, int length) {
    int copy_len = length;
    if (copy_len >= READER_PAGE_TEXT_MAX) {
        copy_len = READER_PAGE_TEXT_MAX - 1;
    }
    memcpy(dest, start, (size_t)copy_len);
    dest[copy_len] = '\0';
}

static void ensure_page_cache(int book_index) {
    const char *segment_start;
    int page = 0;
    if (book_index < 0 || book_index >= APP_BOOK_COUNT || page_cache_ready[book_index]) {
        return;
    }

    segment_start = source_for_book(book_index);
    if (source_has_explicit_pages(segment_start)) {
        for (const char *p = segment_start; page < READER_MAX_PAGES; p++) {
            if (*p == '\f' || *p == '\0') {
                copy_page_segment(page_cache[book_index][page], segment_start, (int)(p - segment_start));
                page++;
                if (*p == '\0') {
                    break;
                }
                segment_start = p + 1;
            }
        }
    } else {
        while (*segment_start != '\0' && page < READER_MAX_PAGES) {
            int segment_len = auto_page_segment_length(segment_start);
            copy_page_segment(page_cache[book_index][page], segment_start, segment_len);
            page++;
            segment_start += segment_len;
            while (*segment_start == '\n' || *segment_start == ' ') {
                segment_start++;
            }
        }
    }
    page_cache_ready[book_index] = 1;
}

int reader_library_book_count(void) {
    return APP_BOOK_COUNT;
}

const reader_book_t *reader_library_book(int book_index) {
    if (book_index < 0 || book_index >= APP_BOOK_COUNT) {
        return NULL;
    }
    return &books[book_index].info;
}

int reader_library_page_count(int book_index) {
    if (book_index < 0 || book_index >= APP_BOOK_COUNT) {
        return 0;
    }
    return count_pages_in_source(source_for_book(book_index));
}

const char *reader_library_source_text(int book_index) {
    if (book_index < 0 || book_index >= APP_BOOK_COUNT) {
        return "";
    }
    return source_for_book(book_index);
}

int reader_library_load_book_file(int book_index, const char *path) {
    FILE *file;
    size_t read;
    if (book_index < 0 || book_index >= APP_BOOK_COUNT || path == NULL) {
        return -1;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    read = fread(source_overrides[book_index], 1, READER_SOURCE_TEXT_MAX - 1, file);
    if (ferror(file)) {
        fclose(file);
        source_overrides[book_index][0] = '\0';
        return -1;
    }
    fclose(file);

    source_overrides[book_index][read] = '\0';
    source_override_ready[book_index] = source_overrides[book_index][0] != '\0';
    page_cache_ready[book_index] = 0;
    return source_override_ready[book_index] ? 0 : -1;
}

const char *reader_library_page_text(int book_index, int page_index) {
    if (book_index < 0 || book_index >= APP_BOOK_COUNT) {
        return "";
    }
    if (page_index < 0 || page_index >= reader_library_page_count(book_index)) {
        return "";
    }
    ensure_page_cache(book_index);
    return page_cache[book_index][page_index];
}

const char *reader_library_chapter_title(int book_index, int chapter_index) {
    if (book_index < 0 || book_index >= APP_BOOK_COUNT) {
        return "";
    }
    if (chapter_index < 0 || chapter_index >= READER_CHAPTER_COUNT) {
        return "";
    }
    return books[book_index].chapters[chapter_index];
}

int reader_library_chapter_page(int book_index, int chapter_index) {
    if (book_index < 0 || book_index >= APP_BOOK_COUNT) {
        return 0;
    }
    if (chapter_index < 0 || chapter_index >= READER_CHAPTER_COUNT) {
        return 0;
    }
    return books[book_index].chapter_pages[chapter_index];
}

int reader_library_chapter_count(int book_index) {
    if (book_index < 0 || book_index >= APP_BOOK_COUNT) {
        return 0;
    }
    return READER_CHAPTER_COUNT;
}
