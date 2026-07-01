#include "app/reader_library.h"

#include "app/app_state.h"

#include <dirent.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define READER_CHAPTER_COUNT 3
#define READER_MAX_PAGES 6
#define READER_PAGE_TEXT_MAX 1024
#define READER_AUTO_PAGE_BYTES 600
#define READER_SOURCE_TEXT_MAX 4096
#define READER_BOOK_PATH_MAX 512
#define READER_META_TEXT_MAX 160

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
        "窗外没有月亮，只有远处高楼顶端微弱的红灯，像一颗不肯熄灭的信号。桌上的茶已经凉透了，杯壁上凝着细密的水珠。"
        "他想起小时候在乡下，夏天的夜晚铺着竹席躺在院子里，满天的星星低得像要落下来。父亲在旁边摇着蒲扇，讲一些关于星星的旧事。"
        "那时候他以为星星是永恒的，后来才知道，连光都有走到尽头的时候。就像现在，他坐在这间城市的屋子里，"
        "翻着一本旧书，而书的作者早已不在人世。但文字还在，思想还在，它们穿过时间的黑暗，像一束微弱却固执的光。"
        "\f叶文洁把旧资料重新摊开。纸页边缘有潮气留下的痕迹，字迹却仍然清楚，像某种从年代深处传来的回声。"
        "资料里记录着一些数字和坐标，那是多年前的一次观测。她记得那天的天气，记得望远镜里那片寂静的天空。"
        "同事们都已经离开了，只剩她一个人在档案室里。日光灯嗡嗡响着，白光照在纸面上，把字迹映得格外清晰。"
        "她用手指轻轻按住纸页的边角，仿佛通过触觉就能触碰到那个年代。那些数字背后是什么？一个信号？一次巧合？"
        "还是宇宙深处某个文明发出的第一声呼唤？她不知道，但她知道，人类已经打开了那扇门，就再也关不上了。"
        "\f观测站外的风停了。屏幕上的数字缓慢跳动，每一次变化都像在提醒人类，宇宙并不因为沉默而空无一物。"
        "夜班工程师端着咖啡走进来，看了一眼屏幕，又悄悄退了出去。这种寂静是观测站的常态，但今晚的寂静似乎格外深。"
        "天线阵列在夜空下排成一条弧线，像一排倾听的耳朵。它们对准的方向，是银河系深处一片被标记为可疑的区域。"
        "三年前，那里曾经收到过一段微弱的信号，持续了不到一秒就消失了。此后，每天晚上都有人守在这里，等待它再次出现。"
        "\f他合上记录本，又很快打开。那些问题没有答案，但每一次阅读都让问题变得更具体，也更难回避。"
        "窗外已经泛起鱼肚白，远处的高楼在晨光中显出轮廓。一夜就这样过去了，而他并不觉得疲倦。"
        "也许是因为夜晚给了他一种特殊的清醒，一种只有在万籁俱寂时才能抵达的专注。他重新拿起笔，在笔记本上写下几个字。"
        "\f天色微亮时，远山只剩黑白两层轮廓。新的页面在墨水屏上稳定下来，像给漫长夜晚盖上一个句号。",
        {"第一章  开端", "第二章  转折", "第三章  回声"},
        {0, 2, 4}
    },
    {
        {"百年孤独", "马尔克斯", "0.8MB", "EPUB", "马孔多"},
        "马孔多的下午潮湿而明亮。多年以后，他仍会想起那阵穿过院子的热风，以及风里混着的木屑气味。"
        "院子里的番石榴树又结果了。孩子们光着脚跑来跑去，笑声像被阳光晒过的铃铛。她坐在走廊的藤椅上，"
        "手里捧着一本翻旧了的书，书页的边角已经卷起。风从远处吹来，带着泥土和花草的气味。"
        "她闭上眼睛，听见远处河水的声音。那条河从镇子边上流过，多少年了，还是不紧不慢地流着。"
        "河对岸的山丘上，有一棵很高很大的树。据说那棵树在镇子建立之前就存在了，比任何人的记忆都要久远。"
        "\f小镇的钟声很慢，像从纸页深处传来。每个人都在等待某件事发生，却没人能说清那件事的形状。"
        "广场上的鸽子扑棱着翅膀飞起来，在空中绕了一圈，又落回原处。卖冰水的老头推着车慢慢走过，"
        "车轮碾过石板路的声音和钟声混在一起，变成一种慵懒的节奏。有人坐在廊柱下打盹，帽子歪到一边。"
        "时间在这里走得很慢，慢到几乎可以忽略不计。但时间确实在走，就像河水确实在流，只是你看不见它在动。"
        "\f雨季让道路变得柔软。她把信放进抽屉，仿佛只要锁住木盒，时间也能被暂时收好。"
        "雨滴打在铁皮屋顶上，发出密密麻麻的声响。院子里的积水没过了脚踝，孩子们却不怕，光着脚在水里踩来踩去。"
        "她站在窗前看着这一切，想起很多年前也有这样一个雨天。那时候她还年轻，穿着白色的裙子，站在同样的窗前。"
        "雨季总是让人想起太多东西。那些已经远去的人和事，在雨声中重新变得清晰，像被水洗过的画。"
        "\f夜晚降临后，屋里的灯光贴着墙面移动。那些名字一个接一个出现，又像河水一样从记忆里退去。",
        {"第一章  小镇", "第二章  雨季", "第三章  家族"},
        {0, 1, 3}
    },
    {
        {"活着", "余华", "0.4MB", "MOBI", "田埂"},
        "田野安静下来，风从麦穗上掠过。他把书合上又打开，仿佛那些旧日子仍然在黑白之间缓缓移动。"
        "田埂上的草已经长高了，漫过脚踝。远处有人在烧秸秆，淡淡的烟味顺风飘过来。他想起小时候也是这样，"
        "秋天的田野上到处是烟，空气里有一股焦甜的味道。那时候他跟在父亲身后，走过一块又一块田。"
        "父亲的背影像一座山，宽阔而沉默。他什么也不说，只是走。跟在后面的孩子也不说话，只是看着父亲的背影。"
        "那些年他们走了很多路，从村子到镇上，从镇上到县城。路边的树换了一茬又一茬，父亲的背也渐渐弯了。"
        "\f牛在前面慢慢走，人的影子落在后面。路并不长，可每一步都像要把一生重新量过。"
        "牛的步子很慢，尾巴一甩一甩地赶苍蝇。它走惯了这条路，知道在哪里拐弯，知道在哪里停。"
        "他跟着牛走，不用想方向。这让他觉得安心。有些日子就是这样，不用想太多，只需要一步一步往前走。"
        "太阳从头顶移到山边，影子从短变长又变短。路边的沟渠里有浅浅的水，偶尔能看见几条小鱼。"
        "他想起年轻时也走过这条路，那时候走得快，总想着赶到前面去。现在不急了，走得慢了，反而看见了更多东西。"
        "\f傍晚的炊烟升起来，村口只剩几声很轻的说话声。日子没有停下，只是换了一种更慢的走法。",
        {"第一章  回家", "第二章  田埂", "第三章  黄昏"},
        {0, 1, 2}
    }
};

static char page_cache[APP_BOOK_COUNT][READER_MAX_PAGES][READER_PAGE_TEXT_MAX];
static int page_cache_ready[APP_BOOK_COUNT];
static char source_overrides[APP_BOOK_COUNT][READER_SOURCE_TEXT_MAX];
static int source_override_ready[APP_BOOK_COUNT];
static reader_book_t info_overrides[APP_BOOK_COUNT];
static char title_overrides[APP_BOOK_COUNT][READER_META_TEXT_MAX];
static char author_overrides[APP_BOOK_COUNT][READER_META_TEXT_MAX];
static char size_overrides[APP_BOOK_COUNT][16];
static int info_override_ready[APP_BOOK_COUNT];

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
    if (info_override_ready[book_index]) {
        return &info_overrides[book_index];
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

static int has_txt_extension(const char *name) {
    size_t len;
    if (name == NULL) {
        return 0;
    }
    len = strlen(name);
    return len > 4 && strcmp(name + len - 4, ".txt") == 0;
}

static const char *path_basename(const char *path) {
    const char *base = path;
    if (path == NULL) {
        return "";
    }
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/') {
            base = p + 1;
        }
    }
    return base;
}

static void copy_without_txt_extension(char *dest, size_t dest_size, const char *path) {
    const char *base = path_basename(path);
    size_t len = strlen(base);
    if (len > 4 && strcmp(base + len - 4, ".txt") == 0) {
        len -= 4;
    }
    if (dest == NULL || dest_size == 0) {
        return;
    }
    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, base, len);
    dest[len] = '\0';
}

static void set_book_info_override(int book_index, const char *path) {
    if (book_index < 0 || book_index >= APP_BOOK_COUNT || path == NULL) {
        return;
    }
    copy_without_txt_extension(title_overrides[book_index], sizeof(title_overrides[book_index]), path);
    strncpy(author_overrides[book_index], "本地书籍", sizeof(author_overrides[book_index]) - 1);
    author_overrides[book_index][sizeof(author_overrides[book_index]) - 1] = '\0';
    strncpy(size_overrides[book_index], "TXT", sizeof(size_overrides[book_index]) - 1);
    size_overrides[book_index][sizeof(size_overrides[book_index]) - 1] = '\0';

    info_overrides[book_index].title = title_overrides[book_index];
    info_overrides[book_index].author = author_overrides[book_index];
    info_overrides[book_index].size_label = "";
    info_overrides[book_index].file_type = size_overrides[book_index];
    info_overrides[book_index].chapter_title = title_overrides[book_index];
    info_override_ready[book_index] = title_overrides[book_index][0] != '\0';
}

static void insert_sorted_path(char paths[][READER_BOOK_PATH_MAX], int *count, const char *path) {
    int pos;
    if (paths == NULL || count == NULL || path == NULL) {
        return;
    }
    if (*count >= APP_BOOK_COUNT && strcmp(path, paths[APP_BOOK_COUNT - 1]) >= 0) {
        return;
    }

    pos = *count < APP_BOOK_COUNT ? *count : APP_BOOK_COUNT - 1;
    while (pos > 0 && strcmp(path, paths[pos - 1]) < 0) {
        strncpy(paths[pos], paths[pos - 1], READER_BOOK_PATH_MAX - 1);
        paths[pos][READER_BOOK_PATH_MAX - 1] = '\0';
        pos--;
    }
    strncpy(paths[pos], path, READER_BOOK_PATH_MAX - 1);
    paths[pos][READER_BOOK_PATH_MAX - 1] = '\0';
    if (*count < APP_BOOK_COUNT) {
        (*count)++;
    }
}

static int collect_realbook_txt_paths(char paths[][READER_BOOK_PATH_MAX]) {
    const char *dirpath = "assets/books/realbook";
    DIR *dir = opendir(dirpath);
    struct dirent *entry;
    int count = 0;
    if (dir == NULL) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[READER_BOOK_PATH_MAX];
        if (!has_txt_extension(entry->d_name)) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
        insert_sorted_path(paths, &count, path);
    }
    closedir(dir);
    return count;
}

int reader_library_load_external_books(void) {
    static const char *fallback_paths[APP_BOOK_COUNT] = {
        "assets/books/real_santi.txt",
        "assets/books/real_bainian.txt",
        "assets/books/real_huozhe.txt"
    };
    char realbook_paths[APP_BOOK_COUNT][READER_BOOK_PATH_MAX] = {{0}};
    int realbook_count = collect_realbook_txt_paths(realbook_paths);
    int loaded = 0;

    for (int i = 0; i < realbook_count; i++) {
        if (reader_library_load_book_file(i, realbook_paths[i]) == 0) {
            set_book_info_override(i, realbook_paths[i]);
            loaded++;
        }
    }
    for (int i = realbook_count; i < APP_BOOK_COUNT; i++) {
        if (reader_library_load_book_file(i, fallback_paths[i]) == 0) {
            loaded++;
        }
    }
    return loaded;
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
