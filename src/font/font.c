#include "font/font.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

#include "fonts/sim_zh12.h"
#include "fonts/sim_zh14.h"
#include "fonts/sim_zh16.h"
#include "fonts/sim_zh18.h"
#include "fonts/sim_zh20.h"
#include "fonts/sim_zh22.h"
#include "fonts/sim_zh24.h"

static const font_face_t face12 = {12, 15, SIM_ZH12_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh12_glyphs, sim_zh12_glyph_count};
static const font_face_t face14 = {14, 17, SIM_ZH14_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh14_glyphs, sim_zh14_glyph_count};
static const font_face_t face16 = {16, 20, SIM_ZH16_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh16_glyphs, sim_zh16_glyph_count};
static const font_face_t face18 = {18, 23, SIM_ZH18_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh18_glyphs, sim_zh18_glyph_count};
static const font_face_t face20 = {20, 25, SIM_ZH20_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh20_glyphs, sim_zh20_glyph_count};
static const font_face_t face22 = {22, 28, SIM_ZH22_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh22_glyphs, sim_zh22_glyph_count};
static const font_face_t face24 = {24, 30, SIM_ZH24_BYTES_PER_GLYPH, (const font_glyph_t *)sim_zh24_glyphs, sim_zh24_glyph_count};

const font_face_t *font_get_face(font_size_t size) {
    switch (size) {
        case FONT_SIZE_12:
            return &face12;
        case FONT_SIZE_14:
            return &face14;
        case FONT_SIZE_18:
            return &face18;
        case FONT_SIZE_20:
            return &face20;
        case FONT_SIZE_22:
            return &face22;
        case FONT_SIZE_24:
            return &face24;
        case FONT_SIZE_16:
        default:
            return &face16;
    }
}

int font_load_default(font_t *font) {
    if (font == NULL) {
        return 0;
    }
    *font = *font_get_face(FONT_SIZE_16);
    return 1;
}

void font_free(font_t *font) {
    if (font != NULL) {
        memset(font, 0, sizeof(*font));
    }
}

int font_decode_utf8(const unsigned char **cursor, uint32_t *codepoint) {
    const unsigned char *p;
    if (cursor == NULL || *cursor == NULL || codepoint == NULL) {
        return 0;
    }
    p = *cursor;
    if (*p == '\0') {
        return 0;
    }
    if (p[0] < 0x80) {
        *codepoint = p[0];
        *cursor = p + 1;
        return 1;
    }
    if ((p[0] & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
        *codepoint = ((uint32_t)(p[0] & 0x1f) << 6) | (uint32_t)(p[1] & 0x3f);
        *cursor = p + 2;
        return 1;
    }
    if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
        *codepoint = ((uint32_t)(p[0] & 0x0f) << 12) |
                     ((uint32_t)(p[1] & 0x3f) << 6) |
                     (uint32_t)(p[2] & 0x3f);
        *cursor = p + 3;
        return 1;
    }
    if ((p[0] & 0xf8) == 0xf0 &&
        (p[1] & 0xc0) == 0x80 &&
        (p[2] & 0xc0) == 0x80 &&
        (p[3] & 0xc0) == 0x80) {
        *codepoint = ((uint32_t)(p[0] & 0x07) << 18) |
                     ((uint32_t)(p[1] & 0x3f) << 12) |
                     ((uint32_t)(p[2] & 0x3f) << 6) |
                     (uint32_t)(p[3] & 0x3f);
        *cursor = p + 4;
        return 1;
    }
    *codepoint = 0xfffd;
    *cursor = p + 1;
    return 1;
}

const font_glyph_t *font_find_glyph(const font_face_t *font, uint32_t codepoint) {
    if (font == NULL || font->glyphs == NULL) {
        return NULL;
    }
    for (int i = 0; i < font->glyph_count; i++) {
        if (font->glyphs[i].codepoint == codepoint) {
            return &font->glyphs[i];
        }
    }
    return NULL;
}

int font_measure_text(const font_face_t *font, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    int width = 0;
    while (cursor != NULL && *cursor != '\0') {
        uint32_t cp;
        if (!font_decode_utf8(&cursor, &cp)) {
            break;
        }
        if (cp == '\n') {
            break;
        }
        const font_glyph_t *glyph = font_find_glyph(font, cp);
        width += glyph != NULL ? glyph->advance : (font != NULL ? font->size : 16);
    }
    return width;
}

static void draw_replacement(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, gfx_color_t color) {
    int size = font != NULL ? font->size : 16;
    gfx_draw_rect(fb, x, y, size, size, color);
}

static void draw_glyph_bitmap(const font_face_t *font, gfx_framebuffer_t *fb, const font_glyph_t *glyph, int x, int y, gfx_color_t color) {
    int bytes_per_row = (font->size + 7) / 8;
    for (int row = 0; row < font->size; row++) {
        for (int col = 0; col < font->size; col++) {
            uint8_t byte = glyph->bitmap[row * bytes_per_row + col / 8];
            if ((byte & (uint8_t)(1u << (7 - (col % 8)))) != 0) {
                gfx_set_pixel(fb, x + col, y + row, color);
            }
        }
    }
}

void font_draw_text(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color) {
    const unsigned char *cursor = (const unsigned char *)text;
    int draw_x = x;
    if (font == NULL || fb == NULL || text == NULL) {
        return;
    }
    while (*cursor != '\0') {
        uint32_t cp;
        if (!font_decode_utf8(&cursor, &cp)) {
            break;
        }
        if (cp == '\n') {
            draw_x = x;
            y += font->line_height;
            continue;
        }
        const font_glyph_t *glyph = font_find_glyph(font, cp);
        if (glyph != NULL) {
            draw_glyph_bitmap(font, fb, glyph, draw_x, y, color);
            draw_x += glyph->advance;
        } else {
            draw_replacement(font, fb, draw_x, y, color);
            draw_x += font->size;
        }
    }
}

void font_draw_text_aligned(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, font_align_t align, gfx_color_t color) {
    int text_width = font_measure_text(font, text);
    int draw_x = x;
    if (align == FONT_ALIGN_CENTER) {
        draw_x = x + (width - text_width) / 2;
    } else if (align == FONT_ALIGN_RIGHT) {
        draw_x = x + width - text_width;
    }
    font_draw_text(font, fb, draw_x, y, text, color);
}

void font_draw_ellipsis(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, gfx_color_t color) {
    const char *ellipsis = "...";
    const unsigned char *cursor = (const unsigned char *)text;
    int used = 0;
    int ellipsis_width = font_measure_text(font, ellipsis);
    if (font_measure_text(font, text) <= width) {
        font_draw_text(font, fb, x, y, text, color);
        return;
    }
    while (*cursor != '\0') {
        const unsigned char *before = cursor;
        uint32_t cp;
        font_decode_utf8(&cursor, &cp);
        const font_glyph_t *glyph = font_find_glyph(font, cp);
        int advance = glyph != NULL ? glyph->advance : font->size;
        if (used + advance + ellipsis_width > width) {
            break;
        }
        char tmp[5] = {0};
        int len = (int)(cursor - before);
        memcpy(tmp, before, (size_t)len);
        font_draw_text(font, fb, x + used, y, tmp, color);
        used += advance;
    }
    font_draw_text(font, fb, x + used, y, ellipsis, color);
}

void font_draw_text_box_spaced(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, int line_height, gfx_color_t color) {
    const unsigned char *cursor = (const unsigned char *)text;
    int line_x = x;
    int line_y = y;
    if (font == NULL || fb == NULL || text == NULL) {
        return;
    }
    if (line_height < font->size) {
        line_height = font->size;
    }
    while (*cursor != '\0' && line_y + font->size <= y + height) {
        const unsigned char *before = cursor;
        uint32_t cp;
        font_decode_utf8(&cursor, &cp);
        if (cp == '\n') {
            line_x = x;
            line_y += line_height;
            continue;
        }
        const font_glyph_t *glyph = font_find_glyph(font, cp);
        int advance = glyph != NULL ? glyph->advance : font->size;
        if (line_x > x && line_x + advance > x + width) {
            line_x = x;
            line_y += line_height;
        }
        if (line_y + font->size > y + height) {
            break;
        }
        char tmp[5] = {0};
        int len = (int)(cursor - before);
        memcpy(tmp, before, (size_t)len);
        font_draw_text(font, fb, line_x, line_y, tmp, color);
        line_x += advance;
    }
}

void font_draw_text_box(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, gfx_color_t color) {
    font_draw_text_box_spaced(font, fb, x, y, width, height, text, font != NULL ? font->line_height : 16, color);
}

/* ============== External Binary Font Implementation ============== */

external_font_t *external_font_create(void) {
    external_font_t *efont = (external_font_t *)malloc(sizeof(external_font_t));
    if (efont != NULL) {
        memset(efont, 0, sizeof(external_font_t));
    }
    return efont;
}

int external_font_load_file(external_font_t *efont, const char *filepath) {
    FILE *file;
    long file_size;
    size_t read_size;
    
    if (efont == NULL || filepath == NULL) {
        return -1;
    }
    
    /* Free existing data if any */
    if (efont->data != NULL) {
        free(efont->data);
        efont->data = NULL;
    }
    
    /* Open file */
    file = fopen(filepath, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open font file: %s\n", filepath);
        return -1;
    }
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        fprintf(stderr, "Invalid font file size: %ld\n", file_size);
        return -1;
    }
    
    /* Allocate buffer */
    efont->data = (uint8_t *)malloc((size_t)file_size);
    if (efont->data == NULL) {
        fclose(file);
        fprintf(stderr, "Failed to allocate memory for font data\n");
        return -1;
    }
    
    /* Read file content */
    read_size = fread(efont->data, 1, (size_t)file_size, file);
    fclose(file);
    
    if ((long)read_size != file_size) {
        free(efont->data);
        efont->data = NULL;
        fprintf(stderr, "Failed to read complete font file\n");
        return -1;
    }
    
    /* Parse filename to extract dimensions
     * Expected format: "20 21×27 汉仪唐美人.bin"
     * where 20 = font size, 21×27 = width×height
     */
    const char *filename = strrchr(filepath, '/');
    if (filename != NULL) {
        filename++;  /* Skip the '/' */
    } else {
        filename = filepath;
    }
    
    /* Extract size and dimensions from filename */
    int parsed_size = 0, parsed_width = 0, parsed_height = 0;
    if (sscanf(filename, "%d %d×%d", &parsed_size, &parsed_width, &parsed_height) == 3) {
        efont->width = parsed_width;
        efont->height = parsed_height;
    } else {
        /* Fallback: try to infer from file size
         * Assume full Unicode BMP (65536 characters)
         */
        int bytes_per_glyph = (int)(file_size / 65536);
        /* Try common widths: 16, 20, 24, 32 */
        for (int w = 16; w <= 48; w += 4) {
            int bytes_per_row = (w + 7) / 8;
            for (int h = 16; h <= 64; h++) {
                if (bytes_per_row * h == bytes_per_glyph) {
                    efont->width = w;
                    efont->height = h;
                    break;
                }
            }
            if (efont->width > 0) break;
        }
    }
    
    if (efont->width <= 0 || efont->height <= 0) {
        free(efont->data);
        efont->data = NULL;
        fprintf(stderr, "Could not determine font dimensions\n");
        return -1;
    }
    
    /* Calculate bytes per glyph */
    int bytes_per_row = (efont->width + 7) / 8;
    efont->bytes_per_glyph = bytes_per_row * efont->height;
    efont->data_size = (int)file_size;
    
    /* Store font name */
    strncpy(efont->name, filename, sizeof(efont->name) - 1);
    efont->name[sizeof(efont->name) - 1] = '\0';
    
    efont->loaded = 1;
    
    printf("Loaded external font: %s (%dx%d, %d bytes/glyph)\n",
           efont->name, efont->width, efont->height, efont->bytes_per_glyph);
    
    return 0;
}

void external_font_free(external_font_t *efont) {
    if (efont != NULL) {
        if (efont->data != NULL) {
            free(efont->data);
            efont->data = NULL;
        }
        memset(efont, 0, sizeof(external_font_t));
        free(efont);
    }
}

const font_glyph_t *external_font_find_glyph(const external_font_t *efont, uint32_t codepoint) {
    static font_glyph_t temp_glyph;
    
    if (efont == NULL || !efont->loaded || efont->data == NULL) {
        return NULL;
    }
    
    /* Check if codepoint is in range */
    int total_glyphs = efont->data_size / efont->bytes_per_glyph;
    if ((int)codepoint >= total_glyphs) {
        return NULL;
    }
    
    /* Calculate offset in data */
    int offset = (int)codepoint * efont->bytes_per_glyph;
    
    /* Fill temporary glyph structure */
    temp_glyph.codepoint = codepoint;
    temp_glyph.width = (uint8_t)efont->width;
    temp_glyph.height = (uint8_t)efont->height;
    temp_glyph.advance = (uint8_t)efont->width;
    
    /* Copy bitmap data - copy what fits in FONT_MAX_BITMAP_BYTES
     * Note: for fonts larger than FONT_MAX_BITMAP_BYTES, the rendering
     * function external_font_draw_text reads directly from raw data
     * and does not use this function. */
    memset(temp_glyph.bitmap, 0, FONT_MAX_BITMAP_BYTES);
    int copy_bytes = efont->bytes_per_glyph;
    if (copy_bytes > FONT_MAX_BITMAP_BYTES) {
        copy_bytes = FONT_MAX_BITMAP_BYTES;
    }
    memcpy(temp_glyph.bitmap, &efont->data[offset], (size_t)copy_bytes);
    
    return &temp_glyph;
}

/* Render a single glyph from external font raw data directly.
 * This bypasses font_glyph_t to support fonts larger than FONT_MAX_BITMAP_BYTES. */
static void draw_external_glyph_raw(const external_font_t *efont, gfx_framebuffer_t *fb,
                                     uint32_t codepoint, int x, int y, gfx_color_t color) {
    int total_glyphs = efont->data_size / efont->bytes_per_glyph;
    /* ASCII characters use half-width (standard CJK font behavior) */
    int draw_w = (codepoint < 128) ? (efont->width + 1) / 2 : efont->width;
    if ((int)codepoint >= total_glyphs) {
        /* Out of range: draw replacement box */
        gfx_draw_rect(fb, x, y, draw_w, efont->height, color);
        return;
    }
    
    int offset = (int)codepoint * efont->bytes_per_glyph;
    int bytes_per_row = (efont->width + 7) / 8;
    
    for (int row = 0; row < efont->height; row++) {
        for (int col = 0; col < draw_w; col++) {
            int byte_index = offset + row * bytes_per_row + col / 8;
            uint8_t byte = efont->data[byte_index];
            if ((byte & (uint8_t)(1u << (7 - (col % 8)))) != 0) {
                gfx_set_pixel(fb, x + col, y + row, color);
            }
        }
    }
}

int external_font_measure_text(const external_font_t *efont, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    int width = 0;
    if (efont == NULL || text == NULL) {
        return 0;
    }
    while (cursor != NULL && *cursor != '\0') {
        uint32_t cp;
        if (!font_decode_utf8(&cursor, &cp)) {
            break;
        }
        if (cp == '\n') {
            break;
        }
        width += (cp < 128) ? (efont->width + 1) / 2 : efont->width;
    }
    return width;
}

void external_font_draw_text(const external_font_t *efont, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color) {
    const unsigned char *cursor = (const unsigned char *)text;
    int draw_x = x;
    
    if (efont == NULL || !efont->loaded || fb == NULL || text == NULL) {
        return;
    }
    
    while (*cursor != '\0') {
        uint32_t cp;
        if (!font_decode_utf8(&cursor, &cp)) {
            break;
        }
        if (cp == '\n') {
            draw_x = x;
            y += efont->height;
            continue;
        }
        draw_external_glyph_raw(efont, fb, cp, draw_x, y, color);
        draw_x += (cp < 128) ? (efont->width + 1) / 2 : efont->width;
    }
}

void external_font_draw_text_aligned(const external_font_t *efont, gfx_framebuffer_t *fb,
                                      int x, int y, int width, const char *text,
                                      font_align_t align, gfx_color_t color) {
    int text_width = external_font_measure_text(efont, text);
    int draw_x = x;
    if (align == FONT_ALIGN_CENTER) {
        draw_x = x + (width - text_width) / 2;
    } else if (align == FONT_ALIGN_RIGHT) {
        draw_x = x + width - text_width;
    }
    external_font_draw_text(efont, fb, draw_x, y, text, color);
}

/* ====== Font Manager Implementation ====== */

#define FONT_MGR_MAX_FONTS 16

static struct {
    external_font_t *fonts[FONT_MGR_MAX_FONTS];
    int count;
    int initialized;
} font_mgr = {0};

static const font_face_t *font_builtin_face_for_size(int size) {
    if (size <= 13) {
        return font_get_face(FONT_SIZE_12);
    }
    if (size <= 15) {
        return font_get_face(FONT_SIZE_14);
    }
    if (size <= 17) {
        return font_get_face(FONT_SIZE_16);
    }
    if (size <= 19) {
        return font_get_face(FONT_SIZE_18);
    }
    if (size <= 21) {
        return font_get_face(FONT_SIZE_20);
    }
    if (size <= 23) {
        return font_get_face(FONT_SIZE_22);
    }
    return font_get_face(FONT_SIZE_24);
}

void font_draw_text_builtin(int size, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color) {
    font_draw_text(font_builtin_face_for_size(size), fb, x, y, text, color);
}

void font_draw_text_aligned_builtin(int size, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, font_align_t align, gfx_color_t color) {
    font_draw_text_aligned(font_builtin_face_for_size(size), fb, x, y, width, text, align, color);
}

int font_measure_text_builtin(int size, const char *text) {
    return font_measure_text(font_builtin_face_for_size(size), text);
}

int font_manager_load_dir(const char *dirpath) {
    DIR *dir;
    struct dirent *entry;
    char filepath[512];
    
    if (dirpath == NULL) {
        return -1;
    }
    
    /* Free existing fonts */
    font_manager_free_all();
    
    dir = opendir(dirpath);
    if (dir == NULL) {
        fprintf(stderr, "font_manager: cannot open directory %s\n", dirpath);
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        /* Only process .bin files */
        const char *ext = strrchr(entry->d_name, '.');
        if (ext == NULL || strcmp(ext, ".bin") != 0) {
            continue;
        }
        
        /* Build full path */
        snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
        
        /* Create and load font */
        if (font_mgr.count >= FONT_MGR_MAX_FONTS) {
            break;
        }
        
        external_font_t *efont = external_font_create();
        if (efont == NULL) {
            continue;
        }
        
        if (external_font_load_file(efont, filepath) != 0) {
            external_font_free(efont);
            continue;
        }
        
        font_mgr.fonts[font_mgr.count] = efont;
        font_mgr.count++;
    }
    
    closedir(dir);
    font_mgr.initialized = 1;
    
    printf("font_manager: loaded %d external fonts from %s\n", font_mgr.count, dirpath);
    return font_mgr.count;
}

const external_font_t *font_manager_get(int size) {
    if (!font_mgr.initialized || font_mgr.count == 0) {
        return NULL;
    }
    
    /* Find the external font with the closest size match.
     * The size is the first number in the filename (e.g., "20" in "20 21x27 ...").
     * We look for the external font whose height is closest to the requested size. */
    const external_font_t *best = NULL;
    int best_diff = 9999;
    
    for (int i = 0; i < font_mgr.count; i++) {
        if (font_mgr.fonts[i] == NULL || !font_mgr.fonts[i]->loaded) {
            continue;
        }
        int diff = font_mgr.fonts[i]->height - size;
        if (diff < 0) diff = -diff;
        if (diff < best_diff) {
            best_diff = diff;
            best = font_mgr.fonts[i];
        }
    }
    
    /* Only return if the match is reasonable (within 15px) */
    if (best != NULL && best_diff <= 15) {
        return best;
    }
    return NULL;
}

const external_font_t *font_manager_get_family(int family_index, int size) {
    const char *families[] = {"方正大黑", "汉仪正圆", "更纱黑体", "汉仪唐美人"};
    const char *family;
    const external_font_t *best = NULL;
    int best_diff = 9999;

    if (!font_mgr.initialized || font_mgr.count == 0) {
        return NULL;
    }
    if (family_index < 0 || family_index >= (int)(sizeof(families) / sizeof(families[0]))) {
        return font_manager_get(size);
    }

    family = families[family_index];
    for (int i = 0; i < font_mgr.count; i++) {
        int diff;
        if (font_mgr.fonts[i] == NULL || !font_mgr.fonts[i]->loaded) {
            continue;
        }
        if (strstr(font_mgr.fonts[i]->name, family) == NULL) {
            continue;
        }
        diff = font_mgr.fonts[i]->height - size;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < best_diff) {
            best_diff = diff;
            best = font_mgr.fonts[i];
        }
    }

    return best != NULL ? best : font_manager_get(size);
}

int font_manager_has_external(int size) {
    return font_manager_get(size) != NULL ? 1 : 0;
}

void font_manager_free_all(void) {
    for (int i = 0; i < font_mgr.count; i++) {
        if (font_mgr.fonts[i] != NULL) {
            external_font_free(font_mgr.fonts[i]);
            font_mgr.fonts[i] = NULL;
        }
    }
    font_mgr.count = 0;
    font_mgr.initialized = 0;
}

/* Unified text drawing: uses external font if available, otherwise built-in */
void font_draw_text_auto(int size, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color) {
    const external_font_t *efont = font_manager_get(size);
    if (efont != NULL) {
        external_font_draw_text(efont, fb, x, y, text, color);
    } else {
        font_draw_text_builtin(size, fb, x, y, text, color);
    }
}

void font_draw_text_aligned_auto(int size, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, font_align_t align, gfx_color_t color) {
    const external_font_t *efont = font_manager_get(size);
    if (efont != NULL) {
        external_font_draw_text_aligned(efont, fb, x, y, width, text, align, color);
    } else {
        font_draw_text_aligned_builtin(size, fb, x, y, width, text, align, color);
    }
}

int font_measure_text_auto(int size, const char *text) {
    const external_font_t *efont = font_manager_get(size);
    if (efont != NULL) {
        return external_font_measure_text(efont, text);
    } else {
        return font_measure_text_builtin(size, text);
    }
}

static void font_draw_text_box_spaced_external(const external_font_t *efont, int size,
                                               gfx_framebuffer_t *fb, int x, int y, int width, int height,
                                               const char *text, int line_height, gfx_color_t color) {
    const unsigned char *cursor = (const unsigned char *)text;
    int line_x = x;
    int line_y = y;
    int glyph_height;

    if (fb == NULL || text == NULL) {
        return;
    }
    if (efont == NULL) {
        font_draw_text_box_spaced(font_builtin_face_for_size(size), fb, x, y, width, height, text, line_height, color);
        return;
    }

    glyph_height = efont->height;
    if (line_height < glyph_height) {
        line_height = glyph_height;
    }
    while (*cursor != '\0' && line_y + glyph_height <= y + height) {
        const unsigned char *before = cursor;
        uint32_t cp;
        int advance;
        char tmp[5] = {0};
        int len;

        if (!font_decode_utf8(&cursor, &cp)) {
            break;
        }
        if (cp == '\n') {
            line_x = x;
            line_y += line_height;
            continue;
        }
        advance = (cp < 128) ? (efont->width + 1) / 2 : efont->width;
        if (line_x > x && line_x + advance > x + width) {
            line_x = x;
            line_y += line_height;
        }
        if (line_y + glyph_height > y + height) {
            break;
        }
        len = (int)(cursor - before);
        memcpy(tmp, before, (size_t)len);
        external_font_draw_text(efont, fb, line_x, line_y, tmp, color);
        line_x += advance;
    }
}

void font_draw_text_box_spaced_auto(int size, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, int line_height, gfx_color_t color) {
    font_draw_text_box_spaced_external(font_manager_get(size), size, fb, x, y, width, height, text, line_height, color);
}

void font_draw_text_box_spaced_family(int size, int family_index, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, int line_height, gfx_color_t color) {
    font_draw_text_box_spaced_external(font_manager_get_family(family_index, size), size, fb, x, y, width, height, text, line_height, color);
}
