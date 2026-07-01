#ifndef FONT_H
#define FONT_H

#include <stdint.h>

#include "gfx/gfx.h"

#define FONT_MAX_BITMAP_BYTES 96

typedef enum {
    FONT_SIZE_12 = 12,
    FONT_SIZE_14 = 14,
    FONT_SIZE_16 = 16,
    FONT_SIZE_18 = 18,
    FONT_SIZE_20 = 20,
    FONT_SIZE_22 = 22,
    FONT_SIZE_24 = 24
} font_size_t;

typedef enum {
    FONT_ALIGN_LEFT = 0,
    FONT_ALIGN_CENTER,
    FONT_ALIGN_RIGHT
} font_align_t;

typedef struct {
    uint32_t codepoint;
    uint8_t width;
    uint8_t height;
    uint8_t advance;
    uint8_t bitmap[FONT_MAX_BITMAP_BYTES];
} font_glyph_t;

typedef struct {
    int size;
    int line_height;
    int bytes_per_glyph;
    const font_glyph_t *glyphs;
    int glyph_count;
} font_face_t;

typedef font_face_t font_t;

/* External binary font support */
typedef struct {
    int loaded;              /* 1 if font is loaded */
    int width;               /* Glyph width in pixels */
    int height;              /* Glyph height in pixels */
    int bytes_per_glyph;     /* Bytes per glyph bitmap */
    uint8_t *data;           /* Raw font data (malloc'd) */
    int data_size;           /* Total data size in bytes */
    char name[64];           /* Font name for identification */
} external_font_t;

int font_load_default(font_t *font);
void font_free(font_t *font);
const font_face_t *font_get_face(font_size_t size);

/* External binary font functions */
external_font_t *external_font_create(void);
int external_font_load_file(external_font_t *efont, const char *filepath);
void external_font_free(external_font_t *efont);
const font_glyph_t *external_font_find_glyph(const external_font_t *efont, uint32_t codepoint);
void external_font_draw_text(const external_font_t *efont, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color);
void external_font_draw_text_aligned(const external_font_t *efont, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, font_align_t align, gfx_color_t color);
int external_font_measure_text(const external_font_t *efont, const char *text);
int font_decode_utf8(const unsigned char **cursor, uint32_t *codepoint);
const font_glyph_t *font_find_glyph(const font_face_t *font, uint32_t codepoint);
int font_measure_text(const font_face_t *font, const char *text);
void font_draw_text(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color);
void font_draw_text_aligned(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, font_align_t align, gfx_color_t color);
void font_draw_text_box(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, gfx_color_t color);
void font_draw_text_box_spaced(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, int line_height, gfx_color_t color);
void font_draw_ellipsis(const font_face_t *font, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, gfx_color_t color);
void font_draw_text_builtin(int size, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color);
void font_draw_text_aligned_builtin(int size, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, font_align_t align, gfx_color_t color);
int font_measure_text_builtin(int size, const char *text);

/* ====== Font Manager: manages external fonts by size ====== */

/* Initialize font manager and load all fonts from a directory.
 * Returns number of fonts loaded, or -1 on error. */
int font_manager_load_dir(const char *dirpath);

/* Get an external font by requested size.
 * Returns the closest matching loaded external font, or NULL if none loaded. */
const external_font_t *font_manager_get(int size);
const external_font_t *font_manager_get_family(int family_index, int size);

/* Check if external fonts are available for a given size */
int font_manager_has_external(int size);

/* Free all loaded external fonts */
void font_manager_free_all(void);

/* Unified text drawing: uses external font if available, otherwise built-in */
void font_draw_text_auto(int size, gfx_framebuffer_t *fb, int x, int y, const char *text, gfx_color_t color);
void font_draw_text_aligned_auto(int size, gfx_framebuffer_t *fb, int x, int y, int width, const char *text, font_align_t align, gfx_color_t color);
int font_measure_text_auto(int size, const char *text);
void font_draw_text_box_spaced_auto(int size, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, int line_height, gfx_color_t color);
void font_draw_text_box_spaced_family(int size, int family_index, gfx_framebuffer_t *fb, int x, int y, int width, int height, const char *text, int line_height, gfx_color_t color);

#endif
