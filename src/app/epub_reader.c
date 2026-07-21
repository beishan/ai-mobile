#include "app/epub_reader.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "miniz.h"
#else
#include <zlib.h>
#endif

#define EPUB_MAX_ENTRY_SIZE (16U * 1024U * 1024U)
#define EPUB_MAX_MANIFEST_ITEMS 256
#define EPUB_PATH_MAX 512

typedef struct {
    uint16_t method;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t local_offset;
} zip_entry_t;

typedef struct {
    char id[96];
    char href[EPUB_PATH_MAX];
} manifest_item_t;

static uint16_t read_le16(const unsigned char *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int zip_central_directory(FILE *file, uint32_t *offset, uint16_t *entries) {
    unsigned char *tail;
    long size;
    long start;
    size_t length;
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 22) return -1;
    length = (size_t)(size > 65557 ? 65557 : size);
    start = size - (long)length;
    tail = malloc(length);
    if (tail == NULL || fseek(file, start, SEEK_SET) != 0 ||
        fread(tail, 1, length, file) != length) {
        free(tail);
        return -1;
    }
    for (size_t i = length - 22;; i--) {
        if (read_le32(tail + i) == 0x06054b50U) {
            *entries = read_le16(tail + i + 10);
            *offset = read_le32(tail + i + 16);
            free(tail);
            return 0;
        }
        if (i == 0) break;
    }
    free(tail);
    return -1;
}

static int zip_find_entry(FILE *file, const char *wanted, zip_entry_t *entry) {
    unsigned char header[46];
    uint32_t central_offset;
    uint16_t entry_count;
    if (zip_central_directory(file, &central_offset, &entry_count) != 0 ||
        fseek(file, (long)central_offset, SEEK_SET) != 0) return -1;
    for (uint16_t i = 0; i < entry_count; i++) {
        char name[EPUB_PATH_MAX];
        uint16_t name_len;
        uint16_t extra_len;
        uint16_t comment_len;
        if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
            read_le32(header) != 0x02014b50U) return -1;
        name_len = read_le16(header + 28);
        extra_len = read_le16(header + 30);
        comment_len = read_le16(header + 32);
        if (name_len >= sizeof(name)) {
            if (fseek(file, (long)name_len + extra_len + comment_len, SEEK_CUR) != 0) return -1;
            continue;
        }
        if (fread(name, 1, name_len, file) != name_len) return -1;
        name[name_len] = '\0';
        if (strcmp(name, wanted) == 0) {
            entry->method = read_le16(header + 10);
            entry->compressed_size = read_le32(header + 20);
            entry->uncompressed_size = read_le32(header + 24);
            entry->local_offset = read_le32(header + 42);
            return 0;
        }
        if (fseek(file, (long)extra_len + comment_len, SEEK_CUR) != 0) return -1;
    }
    return -1;
}

static int inflate_raw(unsigned char *output, size_t output_size,
                       const unsigned char *input, size_t input_size) {
#ifdef ESP_PLATFORM
    size_t result = tinfl_decompress_mem_to_mem(output, output_size, input, input_size, 0);
    return result == output_size ? 0 : -1;
#else
    z_stream stream;
    int result;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef *)input;
    stream.avail_in = (uInt)input_size;
    stream.next_out = output;
    stream.avail_out = (uInt)output_size;
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) return -1;
    result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    return result == Z_STREAM_END && stream.total_out == output_size ? 0 : -1;
#endif
}

static unsigned char *zip_read_entry(FILE *file, const char *name, size_t *size) {
    unsigned char local[30];
    unsigned char *compressed = NULL;
    unsigned char *output = NULL;
    zip_entry_t entry;
    uint16_t name_len;
    uint16_t extra_len;
    if (size == NULL || zip_find_entry(file, name, &entry) != 0 ||
        entry.uncompressed_size > EPUB_MAX_ENTRY_SIZE || entry.compressed_size > EPUB_MAX_ENTRY_SIZE ||
        fseek(file, (long)entry.local_offset, SEEK_SET) != 0 ||
        fread(local, 1, sizeof(local), file) != sizeof(local) ||
        read_le32(local) != 0x04034b50U) return NULL;
    name_len = read_le16(local + 26);
    extra_len = read_le16(local + 28);
    if (fseek(file, (long)name_len + extra_len, SEEK_CUR) != 0) return NULL;
    compressed = malloc(entry.compressed_size > 0 ? entry.compressed_size : 1);
    output = malloc((size_t)entry.uncompressed_size + 1);
    if (compressed == NULL || output == NULL ||
        fread(compressed, 1, entry.compressed_size, file) != entry.compressed_size) goto fail;
    if (entry.method == 0) {
        if (entry.compressed_size != entry.uncompressed_size) goto fail;
        memcpy(output, compressed, entry.uncompressed_size);
    } else if (entry.method == 8) {
        if (inflate_raw(output, entry.uncompressed_size, compressed, entry.compressed_size) != 0) goto fail;
    } else {
        goto fail;
    }
    output[entry.uncompressed_size] = '\0';
    *size = entry.uncompressed_size;
    free(compressed);
    return output;
fail:
    free(compressed);
    free(output);
    return NULL;
}

static int xml_attribute(const char *tag, const char *end, const char *name,
                         char *value, size_t value_size) {
    size_t name_len = strlen(name);
    const char *p = tag;
    while (p != NULL && p < end) {
        p = strstr(p, name);
        if (p == NULL || p >= end) break;
        if ((p == tag || isspace((unsigned char)p[-1])) && p + name_len < end) {
            const char *q = p + name_len;
            while (q < end && isspace((unsigned char)*q)) q++;
            if (q < end && *q == '=') {
                char quote;
                const char *start;
                size_t len;
                q++;
                while (q < end && isspace((unsigned char)*q)) q++;
                if (q >= end || (*q != '\'' && *q != '"')) return -1;
                quote = *q++;
                start = q;
                while (q < end && *q != quote) q++;
                if (q >= end) return -1;
                len = (size_t)(q - start);
                if (len >= value_size) len = value_size - 1;
                memcpy(value, start, len);
                value[len] = '\0';
                return 0;
            }
        }
        p += name_len;
    }
    return -1;
}

static int xml_element_text(const char *xml, const char *element,
                            char *value, size_t value_size) {
    const char *tag = strstr(xml, element);
    const char *start;
    const char *end;
    size_t len;
    if (tag == NULL || (start = strchr(tag, '>')) == NULL) return -1;
    start++;
    end = strchr(start, '<');
    if (end == NULL || end <= start) return -1;
    len = (size_t)(end - start);
    if (len >= value_size) len = value_size - 1;
    memcpy(value, start, len);
    value[len] = '\0';
    return 0;
}

static void utf8_remove_incomplete_tail(char *text) {
    size_t len = strlen(text);
    size_t cursor = 0;
    size_t valid = 0;
    while (cursor < len) {
        unsigned char lead = (unsigned char)text[cursor];
        size_t count = 1;
        int continuation_ok = 1;
        if ((lead & 0xe0U) == 0xc0U) count = 2;
        else if ((lead & 0xf0U) == 0xe0U) count = 3;
        else if ((lead & 0xf8U) == 0xf0U) count = 4;
        else if ((lead & 0x80U) != 0) break;
        if (cursor + count > len) break;
        for (size_t i = 1; i < count; i++) {
            if (((unsigned char)text[cursor + i] & 0xc0U) != 0x80U) continuation_ok = 0;
        }
        if (!continuation_ok) break;
        cursor += count;
        valid = cursor;
    }
    text[valid] = '\0';
}

static void percent_decode(char *path) {
    char *read = path;
    char *write = path;
    while (*read != '\0') {
        if (read[0] == '%' && isxdigit((unsigned char)read[1]) && isxdigit((unsigned char)read[2])) {
            char hex[3] = {read[1], read[2], '\0'};
            *write++ = (char)strtoul(hex, NULL, 16);
            read += 3;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

static void normalize_zip_path(char *path, size_t path_size) {
    char normalized[EPUB_PATH_MAX];
    size_t output_len = 0;
    const char *cursor = path;
    while (*cursor != '\0') {
        const char *segment;
        size_t segment_len;
        while (*cursor == '/') cursor++;
        segment = cursor;
        while (*cursor != '\0' && *cursor != '/') cursor++;
        segment_len = (size_t)(cursor - segment);
        if (segment_len == 0 || (segment_len == 1 && segment[0] == '.')) continue;
        if (segment_len == 2 && segment[0] == '.' && segment[1] == '.') {
            while (output_len > 0 && normalized[output_len - 1] != '/') output_len--;
            if (output_len > 0) output_len--;
            continue;
        }
        if (output_len > 0 && output_len < sizeof(normalized) - 1) normalized[output_len++] = '/';
        if (segment_len > sizeof(normalized) - 1 - output_len)
            segment_len = sizeof(normalized) - 1 - output_len;
        memcpy(normalized + output_len, segment, segment_len);
        output_len += segment_len;
    }
    normalized[output_len] = '\0';
    snprintf(path, path_size, "%s", normalized);
}

static int join_resource_path(const char *opf_path, const char *href,
                              char *result, size_t result_size) {
    const char *slash = strrchr(opf_path, '/');
    int written;
    if (slash == NULL) written = snprintf(result, result_size, "%s", href);
    else written = snprintf(result, result_size, "%.*s/%s", (int)(slash - opf_path), opf_path, href);
    if (written < 0 || (size_t)written >= result_size) return -1;
    percent_decode(result);
    normalize_zip_path(result, result_size);
    return 0;
}

static int utf8_write_codepoint(FILE *output, unsigned long cp) {
    unsigned char bytes[4];
    size_t count;
    if (cp <= 0x7f) { bytes[0] = (unsigned char)cp; count = 1; }
    else if (cp <= 0x7ff) {
        bytes[0] = (unsigned char)(0xc0 | (cp >> 6));
        bytes[1] = (unsigned char)(0x80 | (cp & 0x3f));
        count = 2;
    } else if (cp <= 0xffff) {
        bytes[0] = (unsigned char)(0xe0 | (cp >> 12));
        bytes[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | (cp & 0x3f));
        count = 3;
    } else if (cp <= 0x10ffff) {
        bytes[0] = (unsigned char)(0xf0 | (cp >> 18));
        bytes[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        bytes[3] = (unsigned char)(0x80 | (cp & 0x3f));
        count = 4;
    } else return -1;
    return fwrite(bytes, 1, count, output) == count ? 0 : -1;
}

static int html_write_entity(FILE *output, const char *entity, size_t length) {
    if (length == 3 && memcmp(entity, "amp", 3) == 0) return fputc('&', output) == EOF ? -1 : 0;
    if (length == 2 && memcmp(entity, "lt", 2) == 0) return fputc('<', output) == EOF ? -1 : 0;
    if (length == 2 && memcmp(entity, "gt", 2) == 0) return fputc('>', output) == EOF ? -1 : 0;
    if (length == 4 && memcmp(entity, "quot", 4) == 0) return fputc('"', output) == EOF ? -1 : 0;
    if (length == 4 && memcmp(entity, "apos", 4) == 0) return fputc('\'', output) == EOF ? -1 : 0;
    if (length == 4 && memcmp(entity, "nbsp", 4) == 0) return fputc(' ', output) == EOF ? -1 : 0;
    if (length > 1 && entity[0] == '#') {
        char number[16];
        char *end;
        unsigned long cp;
        int base = 10;
        const char *digits = entity + 1;
        size_t digits_len = length - 1;
        if (digits_len > 1 && (*digits == 'x' || *digits == 'X')) { base = 16; digits++; digits_len--; }
        if (digits_len >= sizeof(number)) return 0;
        memcpy(number, digits, digits_len);
        number[digits_len] = '\0';
        cp = strtoul(number, &end, base);
        if (*end == '\0') return utf8_write_codepoint(output, cp);
    }
    return 0;
}

static int tag_is_block(const char *tag, size_t len) {
    static const char *blocks[] = {"p", "div", "br", "h1", "h2", "h3", "h4", "h5", "h6",
                                   "li", "tr", "blockquote", "section", "article", "title"};
    while (len > 0 && (*tag == '/' || isspace((unsigned char)*tag))) { tag++; len--; }
    for (size_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++) {
        size_t block_len = strlen(blocks[i]);
        if (len >= block_len && strncasecmp(tag, blocks[i], block_len) == 0 &&
            (len == block_len || isspace((unsigned char)tag[block_len]) || tag[block_len] == '/' || tag[block_len] == '>')) return 1;
    }
    return 0;
}

static int tag_name_is(const char *tag, size_t len, const char *name, int *closing) {
    size_t name_len = strlen(name);
    while (len > 0 && isspace((unsigned char)*tag)) { tag++; len--; }
    *closing = len > 0 && *tag == '/';
    if (*closing) { tag++; len--; }
    while (len > 0 && isspace((unsigned char)*tag)) { tag++; len--; }
    return len >= name_len && strncasecmp(tag, name, name_len) == 0 &&
           (len == name_len || isspace((unsigned char)tag[name_len]) || tag[name_len] == '/' || tag[name_len] == '>');
}

static int html_to_text(FILE *output, const unsigned char *html, size_t size) {
    size_t i = 0;
    int last_newline = 1;
    int suppressed = 0;
    while (i < size) {
        if (html[i] == '<') {
            size_t end = i + 1;
            int closing = 0;
            while (end < size && html[end] != '>') end++;
            if (end >= size) break;
            if (tag_name_is((const char *)html + i + 1, end - i - 1, "style", &closing) ||
                tag_name_is((const char *)html + i + 1, end - i - 1, "script", &closing)) {
                suppressed = closing ? 0 : 1;
            } else if (!suppressed &&
                       tag_is_block((const char *)html + i + 1, end - i - 1) && !last_newline) {
                if (fputc('\n', output) == EOF) return -1;
                last_newline = 1;
            }
            i = end + 1;
        } else if (suppressed) {
            i++;
        } else if (html[i] == '&') {
            size_t end = i + 1;
            while (end < size && end - i < 16 && html[end] != ';') end++;
            if (end < size && html[end] == ';') {
                if (html_write_entity(output, (const char *)html + i + 1, end - i - 1) != 0) return -1;
                last_newline = 0;
                i = end + 1;
            } else {
                if (fputc('&', output) == EOF) return -1;
                last_newline = 0;
                i++;
            }
        } else {
            unsigned char ch = html[i++];
            if (ch == '\r') continue;
            if (ch == '\n' || ch == '\t') ch = ' ';
            if (ch == ' ' && last_newline) continue;
            if (fputc(ch, output) == EOF) return -1;
            last_newline = ch == '\n';
        }
    }
    if (!last_newline && fputc('\n', output) == EOF) return -1;
    return 0;
}

int epub_extract_text(const char *epub_path, const char *text_path,
                      epub_metadata_t *metadata) {
    manifest_item_t *manifest = NULL;
    char (*spine)[96] = NULL;
    char opf_path[EPUB_PATH_MAX];
    unsigned char *container = NULL;
    unsigned char *opf = NULL;
    FILE *archive = NULL;
    FILE *output = NULL;
    size_t size;
    int manifest_count = 0;
    int spine_count = 0;
    int written_sections = 0;
    int result = -1;
    if (epub_path == NULL || text_path == NULL || metadata == NULL) return -1;
    memset(metadata, 0, sizeof(*metadata));
    manifest = calloc(EPUB_MAX_MANIFEST_ITEMS, sizeof(*manifest));
    spine = calloc(EPUB_MAX_MANIFEST_ITEMS, sizeof(*spine));
    if (manifest == NULL || spine == NULL) goto cleanup;
    archive = fopen(epub_path, "rb");
    if (archive == NULL) goto cleanup;
    container = zip_read_entry(archive, "META-INF/container.xml", &size);
    if (container == NULL) goto cleanup;
    {
        const char *rootfile = strstr((const char *)container, "<rootfile ");
        const char *end = rootfile != NULL ? strchr(rootfile, '>') : NULL;
        if (rootfile == NULL || end == NULL ||
            xml_attribute(rootfile, end, "full-path", opf_path, sizeof(opf_path)) != 0) goto cleanup;
        percent_decode(opf_path);
        normalize_zip_path(opf_path, sizeof(opf_path));
    }
    opf = zip_read_entry(archive, opf_path, &size);
    if (opf == NULL) goto cleanup;
    (void)xml_element_text((const char *)opf, "<dc:title", metadata->title, sizeof(metadata->title));
    (void)xml_element_text((const char *)opf, "<dc:creator", metadata->author, sizeof(metadata->author));
    utf8_remove_incomplete_tail(metadata->title);
    utf8_remove_incomplete_tail(metadata->author);
    {
        const char *p = (const char *)opf;
        while (manifest_count < EPUB_MAX_MANIFEST_ITEMS && (p = strstr(p, "<item")) != NULL) {
            const char *end = strchr(p, '>');
            if (end == NULL) break;
            if (strncmp(p, "<itemref", 8) != 0 &&
                xml_attribute(p, end, "id", manifest[manifest_count].id,
                              sizeof(manifest[manifest_count].id)) == 0 &&
                xml_attribute(p, end, "href", manifest[manifest_count].href,
                              sizeof(manifest[manifest_count].href)) == 0) manifest_count++;
            p = end + 1;
        }
    }
    {
        const char *p = (const char *)opf;
        while (spine_count < EPUB_MAX_MANIFEST_ITEMS && (p = strstr(p, "<itemref")) != NULL) {
            const char *end = strchr(p, '>');
            if (end == NULL) break;
            if (xml_attribute(p, end, "idref", spine[spine_count], sizeof(spine[spine_count])) == 0) spine_count++;
            p = end + 1;
        }
    }
    output = fopen(text_path, "wb");
    if (output == NULL) goto cleanup;
    for (int i = 0; i < spine_count; i++) {
        const char *href = NULL;
        char resource_path[EPUB_PATH_MAX];
        unsigned char *html;
        for (int j = 0; j < manifest_count; j++) {
            if (strcmp(manifest[j].id, spine[i]) == 0) { href = manifest[j].href; break; }
        }
        if (href == NULL || join_resource_path(opf_path, href, resource_path, sizeof(resource_path)) != 0) continue;
        html = zip_read_entry(archive, resource_path, &size);
        if (html == NULL) continue;
        if (html_to_text(output, html, size) == 0) written_sections++;
        free(html);
        if (fputc('\n', output) == EOF) goto cleanup;
    }
    if (written_sections > 0 && fflush(output) == 0) result = 0;
cleanup:
    free(manifest);
    free(spine);
    free(container);
    free(opf);
    if (output != NULL && fclose(output) != 0) result = -1;
    if (archive != NULL) fclose(archive);
    if (result != 0) remove(text_path);
    return result;
}
