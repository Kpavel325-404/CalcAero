#define CA_NO_PRINTF_WRAP
#include "unicode_font.h"
#include "l10n.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define CELL 20
#define CACHE_TOP 288
#define COLS (512 / CELL)
#define SLOTS (COLS * ((512 - CACHE_TOP) / CELL))
#define LAYOUTS 64
static FT_Library library;
static FT_Face faces[2];
static hb_font_t *fonts[2];
static unsigned char *font_bytes[2];
static int selected = LANG_EN, faults;
static unsigned frame = 1, clock_id;
static struct {
    unsigned gid, frame, last;
    int face, valid;
    float bx, by;
    PPGlyph box;
} cache[SLOTS];
static struct {
    char text[600];
    unsigned last;
    int valid;
    CATextLayout layout;
} layouts[LAYOUTS];
static void reset(void) {
    memset(cache, 0, sizeof(cache));
    memset(layouts, 0, sizeof(layouts));
}
static int open_face(const char *dir, const char *name, FT_Face *face, hb_font_t **font,
                     unsigned char **bytes) {
    char path[700];
    int n = snprintf(path, sizeof(path), "%s/assets/fonts/%s", dir, name);
    if (n < 0 || (size_t)n >= sizeof(path))
        return 0;
    struct stat st;
    if (stat(path, &st) || st.st_size <= 0 || st.st_size > 2097152)
        return 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    *bytes = malloc((size_t)st.st_size);
    if (!*bytes) {
        fclose(f);
        return 0;
    }
    int ok = fread(*bytes, 1, (size_t)st.st_size, f) == (size_t)st.st_size;
    fclose(f);
    if (!ok || FT_New_Memory_Face(library, *bytes, st.st_size, 0, face)) {
        free(*bytes);
        *bytes = 0;
        return 0;
    }
    FT_Set_Pixel_Sizes(*face, 0, 12);
    *font = hb_ft_font_create_referenced(*face);
    if (!*font) {
        FT_Done_Face(*face);
        free(*bytes);
        *bytes = 0;
        *face = 0;
        return 0;
    }
    hb_ft_font_set_load_flags(*font, FT_LOAD_DEFAULT);
    return 1;
}
static void close_face(int i) {
    if (fonts[i])
        hb_font_destroy(fonts[i]);
    if (faces[i])
        FT_Done_Face(faces[i]);
    free(font_bytes[i]);
    faces[i] = 0;
    fonts[i] = 0;
    font_bytes[i] = 0;
}
int ca_font_select(const char *dir, int lang) {
    if (lang < 0 || lang >= LANG_COUNT)
        return 0;
    if (!library && FT_Init_FreeType(&library))
        return 0;
    if (!faces[0]) {
        if (!open_face(dir, "base.ttf", &faces[0], &fonts[0], &font_bytes[0]))
            return lang == LANG_EN;
        FT_Set_Pixel_Sizes(faces[0], 0, 11);
        hb_ft_font_changed(fonts[0]);
    }
    const char *script = lang == LANG_AR      ? "ar.ttf"
                         : lang == LANG_HI    ? "hi.ttf"
                         : lang == LANG_ZH_TW ? "zh-TW.otf"
                         : lang == LANG_ZH_CN ? "zh-CN.otf"
                         : lang == LANG_KO    ? "ko.otf"
                         : lang == LANG_JA    ? "ja.otf"
                                              : 0;
    FT_Face face = 0;
    hb_font_t *font = 0;
    unsigned char *bytes = 0;
    if (script && !open_face(dir, script, &face, &font, &bytes))
        return 0;
    close_face(1);
    faces[1] = face;
    fonts[1] = font;
    font_bytes[1] = bytes;
    selected = lang;
    reset();
    return 1;
}
void ca_font_close(void) {
    close_face(1);
    close_face(0);
    if (library)
        FT_Done_FreeType(library);
    library = 0;
    reset();
}
void ca_font_frame(void) {
    if (++frame == 0) {
        frame = 1;
        reset();
    }
}
int ca_font_faults(void) { return faults; }
static unsigned decode(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned c = *s++;
    if (c >= 0xc2 && c < 0xe0 && (s[0] & 0xc0) == 0x80) {
        c = (c & 31) << 6 | (s[0] & 63);
        s++;
    } else if (c >= 0xe0 && c < 0xf0 && s[0] && s[1] && (s[0] & 0xc0) == 0x80 &&
               (s[1] & 0xc0) == 0x80) {
        c = (c & 15) << 12 | (s[0] & 63) << 6 | (s[1] & 63);
        s += 2;
    } else if (c >= 0xf0 && c < 0xf5 && s[0] && s[1] && s[2] && (s[0] & 0xc0) == 0x80 &&
               (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80) {
        c = (c & 7) << 18 | (s[0] & 63) << 12 | (s[1] & 63) << 6 | (s[2] & 63);
        s += 3;
    }
    *p = (const char *)s;
    return c;
}
static int script_char(unsigned c) {
    if (selected == LANG_AR)
        return c >= 0x600 && c <= 0x8ff;
    if (selected == LANG_HI)
        return c >= 0x900 && c <= 0x97f;
    if (selected >= LANG_ZH_TW)
        return c >= 0x2e80;
    return 0;
}
static void shape(CATextLayout *out, const char *s) {
    struct Run {
        const char *p;
        int bytes, face;
    } runs[64];
    int nr = 0;
    const char *p = s, *start = s;
    int face = -1, arabic = 0;
    while (*p) {
        const char *begin = p;
        unsigned c = decode(&p);
        int next = script_char(c) ? 1 : 0;
        int neutral = c == 32 || c == ':' || c == ',' || c == ';' || c == '.' || c == '!' ||
                      c == '?' || c == '(' || c == ')';
        if (neutral && face >= 0)
            next = face;
        if (face >= 0 && next != face && nr < 63) {
            runs[nr++] = (struct Run){start, (int)(begin - start), face};
            start = begin;
        }
        face = next;
        if (selected == LANG_AR && next)
            arabic = 1;
    }
    if (p > start)
        runs[nr++] = (struct Run){start, (int)(p - start), face};
    out->count = 0;
    out->width = 0;
    for (int k = 0; k < nr; k++) {
        struct Run r = runs[arabic ? nr - 1 - k : k];
        int f = r.face && fonts[1] ? 1 : 0;
        if (!fonts[f])
            continue;
        hb_buffer_t *buffer = hb_buffer_create();
        hb_buffer_add_utf8(buffer, r.p, r.bytes, 0, r.bytes);
        hb_buffer_guess_segment_properties(buffer);
        hb_buffer_set_direction(buffer, selected == LANG_AR && r.face ? HB_DIRECTION_RTL
                                                                      : HB_DIRECTION_LTR);
        hb_shape(fonts[f], buffer, 0, 0);
        unsigned n;
        hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, &n);
        hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buffer, &n);
        for (unsigned i = 0; i < n; i++) {
            if (out->count >= CA_TEXT_GLYPHS) {
                faults++;
                break;
            }
            out->glyph[out->count++] =
                (CAGlyph){info[i].codepoint, out->width + pos[i].x_offset / 64.0f,
                          -pos[i].y_offset / 64.0f, (unsigned char)f};
            out->width += pos[i].x_advance / 64.0f;
        }
        hb_buffer_destroy(buffer);
    }
}
const CATextLayout *ca_font_layout(const char *s) {
    static CATextLayout long_text;
    unsigned last = ++clock_id;
    int victim = 0;
    for (int i = 0; i < LAYOUTS; i++) {
        if (layouts[i].valid && !strcmp(layouts[i].text, s)) {
            layouts[i].last = last;
            return &layouts[i].layout;
        }
        if (!layouts[i].valid) {
            victim = i;
            break;
        }
        if (layouts[i].last < layouts[victim].last)
            victim = i;
    }
    if (strlen(s) >= sizeof(layouts[0].text)) {
        shape(&long_text, s);
        return &long_text;
    }
    strcpy(layouts[victim].text, s);
    layouts[victim].last = last;
    layouts[victim].valid = 1;
    shape(&layouts[victim].layout, s);
    return &layouts[victim].layout;
}
const PPGlyph *ca_font_glyph(unsigned gid, int f) {
    unsigned oldest = ~0u;
    int victim = -1;
    for (int i = 0; i < SLOTS; i++) {
        if (cache[i].valid && cache[i].gid == gid && cache[i].face == f) {
            cache[i].frame = frame;
            cache[i].last = ++clock_id;
            return &cache[i].box;
        }
        if (!cache[i].valid) {
            victim = i;
            break;
        }
        if (cache[i].frame != frame && cache[i].last < oldest) {
            oldest = cache[i].last;
            victim = i;
        }
    }
    if (victim < 0 || !faces[f] || FT_Load_Glyph(faces[f], gid, FT_LOAD_RENDER)) {
        faults++;
        return &pp_glyphs[0]['?' - 32];
    }
    FT_GlyphSlot g = faces[f]->glyph;
    int w = (int)g->bitmap.width, h = (int)g->bitmap.rows;
    if (w > CELL - 2 || h > CELL - 2) {
        faults++;
        return &pp_glyphs[0]['?' - 32];
    }
    int u = (victim % COLS) * CELL, v = CACHE_TOP + (victim / COLS) * CELL;
    for (int row = 0; row < CELL; row++)
        memset(pp_atlas + (v + row) * 512 + u, 0, CELL);
    for (int row = 0; row < h; row++)
        memcpy(pp_atlas + (v + row + 1) * 512 + u + 1, g->bitmap.buffer + row * g->bitmap.pitch,
               (size_t)w);
    cache[victim].box =
        (PPGlyph){(uint16_t)(u + 1), (uint16_t)(v + 1), (uint16_t)w, (uint16_t)h, 0};
    cache[victim].bx = (float)g->bitmap_left;
    cache[victim].by = 11.0f - g->bitmap_top;
    cache[victim].valid = 1;
    cache[victim].gid = gid;
    cache[victim].face = f;
    cache[victim].frame = frame;
    cache[victim].last = ++clock_id;
    return &cache[victim].box;
}
void ca_font_bearing(unsigned gid, int f, float *x, float *y) {
    for (int i = 0; i < SLOTS; i++)
        if (cache[i].valid && cache[i].gid == gid && cache[i].face == f) {
            *x = cache[i].bx;
            *y = cache[i].by;
            return;
        }
    *x = 0;
    *y = 0;
}
