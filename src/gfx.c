#include "gfx.h"
#include "font.h"
#include "l10n.h"
#include "unicode_font.h"
#include <math.h>
#include <string.h>
PPVertex pp_vertices[PP_MAX_VERTICES] __attribute__((aligned(64)));
int pp_vertex_count, pp_vertex_overflow;
void gfx_begin(void) {
    ca_font_frame();
    pp_vertex_count = 0;
    pp_vertex_overflow = 0;
}
static void triangle(PPVertex a, PPVertex b, PPVertex c) {
    if (pp_vertex_count + 3 > PP_MAX_VERTICES) {
        pp_vertex_overflow = 1;
        return;
    }
    pp_vertices[pp_vertex_count++] = a;
    pp_vertices[pp_vertex_count++] = b;
    pp_vertices[pp_vertex_count++] = c;
}
static PPVertex point(float x, float y, uint32_t c) { return (PPVertex){2, 2, c, x, y, 0}; }
uint32_t gfx_mix(uint32_t a, uint32_t b, float t) {
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    uint32_t c = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        float v = (float)((a >> shift) & 255) * (1 - t) + (float)((b >> shift) & 255) * t;
        c |= (uint32_t)(v + .5f) << shift;
    }
    return c;
}
void gfx_triangle(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t c) {
    triangle(point(x1, y1, c), point(x2, y2, c), point(x3, y3, c));
}
void gfx_gradient(float x, float y, float w, float h, uint32_t top, uint32_t bottom) {
    PPVertex a = point(x, y, top), b = point(x + w, y, top), c = point(x + w, y + h, bottom),
             d = point(x, y + h, bottom);
    triangle(a, b, c);
    triangle(a, c, d);
}
void gfx_rect(float x, float y, float w, float h, uint32_t c) {
    if (w > 0 && h > 0)
        gfx_gradient(x, y, w, h, c, c);
}
void gfx_line(float x1, float y1, float x2, float y2, float width, uint32_t c) {
    float dx = x2 - x1, dy = y2 - y1, len = sqrtf(dx * dx + dy * dy);
    if (len < .001f)
        return;
    float nx = -dy * width * .5f / len, ny = dx * width * .5f / len;
    gfx_triangle(x1 + nx, y1 + ny, x1 - nx, y1 - ny, x2 - nx, y2 - ny, c);
    gfx_triangle(x1 + nx, y1 + ny, x2 - nx, y2 - ny, x2 + nx, y2 + ny, c);
}
void gfx_line_smooth(float x1, float y1, float x2, float y2, float width, uint32_t c) {
    float dx = x2 - x1, dy = y2 - y1, len = sqrtf(dx * dx + dy * dy);
    if (len < .001f)
        return;
    float nx = -dy / len, ny = dx / len, half = width * .5f;
    PPVertex a = point(x1 + nx * half, y1 + ny * half, c),
             b = point(x1 - nx * half, y1 - ny * half, c),
             d = point(x2 + nx * half, y2 + ny * half, c),
             e = point(x2 - nx * half, y2 - ny * half, c);
    triangle(a, b, e);
    triangle(a, e, d);
    uint32_t transparent = c & UINT32_C(0x00ffffff);
    PPVertex oa = point(x1 + nx * (half + .75f), y1 + ny * (half + .75f), transparent),
             od = point(x2 + nx * (half + .75f), y2 + ny * (half + .75f), transparent),
             ob = point(x1 - nx * (half + .75f), y1 - ny * (half + .75f), transparent),
             oe = point(x2 - nx * (half + .75f), y2 - ny * (half + .75f), transparent);
    triangle(a, d, od);
    triangle(a, od, oa);
    triangle(b, ob, oe);
    triangle(b, oe, e);
}
void gfx_round_gradient(float x, float y, float w, float h, float r, uint32_t top,
                        uint32_t bottom) {
    if (w <= 0 || h <= 0)
        return;
    r = fminf(r, fminf(w, h) * .5f);
    if (r < .5f) {
        gfx_gradient(x, y, w, h, top, bottom);
        return;
    }
    PPVertex in[20], out[20];
    float cx[] = {x + r, x + w - r, x + w - r, x + r}, cy[] = {y + r, y + r, y + h - r, y + h - r};
    static const float nx[] = {-1, -.923879533f, -.707106781f, -.382683432f, 0,
                               0,  .382683432f,  .707106781f,  .923879533f,  1,
                               1,  .923879533f,  .707106781f,  .382683432f,  0,
                               0,  -.382683432f, -.707106781f, -.923879533f, -1};
    static const float ny[] = {0,  -.382683432f, -.707106781f, -.923879533f, -1,
                               -1, -.923879533f, -.707106781f, -.382683432f, 0,
                               0,  .382683432f,  .707106781f,  .923879533f,  1,
                               1,  .923879533f,  .707106781f,  .382683432f,  0};
    for (int corner = 0; corner < 4; ++corner)
        for (int step = 0; step <= 4; ++step) {
            int i = corner * 5 + step;
            float px = cx[corner] + r * nx[i], py = cy[corner] + r * ny[i];
            uint32_t c = gfx_mix(top, bottom, (py - y) / h);
            in[i] = point(px, py, c);
            out[i] = point(px + nx[i] * .75f, py + ny[i] * .75f, c & UINT32_C(0x00ffffff));
        }
    PPVertex center = point(x + w * .5f, y + h * .5f, gfx_mix(top, bottom, .5f));
    for (int i = 0; i < 20; ++i) {
        int j = (i + 1) % 20;
        triangle(center, in[i], in[j]);
        triangle(in[i], out[i], out[j]);
        triangle(in[i], out[j], in[j]);
    }
}
void gfx_round(float x, float y, float w, float h, float r, uint32_t c) {
    gfx_round_gradient(x, y, w, h, r, c, c);
}
void gfx_circle(float x, float y, float r, uint32_t c) {
    gfx_round(x - r, y - r, r * 2, r * 2, r, c);
}
static unsigned int utf8(const char **s) {
    const unsigned char *p = (const unsigned char *)*s;
    unsigned int c = *p++;
    if ((c & 0xe0) == 0xc0 && *p)
        c = ((c & 31) << 6) | (*p++ & 63);
    else if ((c & 0xf0) == 0xe0 && p[0] && p[1]) {
        c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63);
        p += 2;
    }
    *s = (const char *)p;
    return c;
}
static int glyph(unsigned int c) {
    if (c >= 32 && c <= 126)
        return (int)c - 32;
    return '?' - 32;
}
void gfx_key_mask(float x, float y, int tile, uint32_t top, uint32_t bottom) {
    if (tile < 0 || tile > 2)
        return;
    const PPGlyph *g = &pp_key_masks[tile];
    PPVertex a = {(float)g->u, (float)g->v, top, x, y, 0},
             b = {(float)(g->u + g->width), (float)g->v, top, x + g->width, y, 0},
             c = {(float)(g->u + g->width),
                  (float)(g->v + g->height),
                  bottom,
                  x + g->width,
                  y + g->height,
                  0},
             d = {(float)g->u, (float)(g->v + g->height), bottom, x, y + g->height, 0};
    triangle(a, b, c);
    triangle(a, c, d);
}
static int unicode_text(const char *s) {
    while (*s)
        if ((unsigned char)*s++ >= 128)
            return 1;
    return 0;
}
static float text_right = 480;
static void glyph_quad(float x, float y, const PPGlyph *g, uint32_t color) {
    if (!g->width || !g->height)
        return;
    float x0 = fmaxf(0, x), y0 = fmaxf(0, y), x1 = fminf(text_right, x + g->width),
          y1 = fminf(272, y + g->height);
    if (x1 <= x0 || y1 <= y0)
        return;
    float u0 = g->u + x0 - x, v0 = g->v + y0 - y, u1 = g->u + x1 - x, v1 = g->v + y1 - y;
    PPVertex a = {u0, v0, color, x0, y0, 0}, b = {u1, v0, color, x1, y0, 0},
             c = {u1, v1, color, x1, y1, 0}, d = {u0, v1, color, x0, y1, 0};
    triangle(a, b, c);
    triangle(a, c, d);
}
static void text_face(float x, float y, const char *s, int face, uint32_t color) {
    if (face != 2)
        s = L(s);
    if (face == 0 && unicode_text(s)) {
        const CATextLayout *layout = ca_font_layout(s);
        for (int i = 0; i < layout->count; i++) {
            CAGlyph g = layout->glyph[i];
            float bx, by;
            const PPGlyph *box = ca_font_glyph(g.gid, g.font);
            ca_font_bearing(g.gid, g.font, &bx, &by);
            glyph_quad(x + g.x + bx, y + g.y + by, box, color);
        }
        return;
    }
    while (*s) {
        const PPGlyph *g = &pp_glyphs[face][glyph(utf8(&s))];
        glyph_quad(x, y, g, color);
        x += g->advance;
    }
}
void gfx_text_clipped(float x, float y, const char *s, int scale, uint32_t color, float width) {
    float old = text_right;
    text_right = fminf(480, x + width);
    text_face(x, y, s, scale == 2 ? 1 : 0, color);
    text_right = old;
}
void gfx_text(float x, float y, const char *s, int scale, uint32_t color) {
    text_face(x, y, s, scale == 2 ? 1 : 0, color);
}
void gfx_text_mono(float x, float y, const char *s, uint32_t color) {
    text_face(x, y, s, 2, color);
}
int gfx_mono_width(void) { return pp_glyphs[2]['0' - 32].advance; }
int gfx_text_width(const char *s, int scale) {
    s = L(s);
    if (scale != 2 && unicode_text(s))
        return (int)(ca_font_layout(s)->width + .5f);
    int n = 0, face = scale == 2 ? 1 : 0;
    while (*s)
        n += pp_glyphs[face][glyph(utf8(&s))].advance;
    return n;
}
void gfx_button(int button, float x, float y, float r, uint32_t c) {
    if (button == 0) {
        gfx_line(x - r, y - r, x + r, y - r, 2, c);
        gfx_line(x + r, y - r, x + r, y + r, 2, c);
        gfx_line(x + r, y + r, x - r, y + r, 2, c);
        gfx_line(x - r, y + r, x - r, y - r, 2, c);
    } else if (button == 1) {
        for (int i = 0; i < 24; ++i) {
            float a = i * 6.28318530718f / 24, b = (i + 1) * 6.28318530718f / 24;
            gfx_line(x + cosf(a) * r, y + sinf(a) * r, x + cosf(b) * r, y + sinf(b) * r, 2, c);
        }
    } else if (button == 2) {
        gfx_line(x - r, y - r, x + r, y + r, 2, c);
        gfx_line(x - r, y + r, x + r, y - r, 2, c);
    } else {
        gfx_line(x, y - r - 1, x + r + 1, y + r, 2, c);
        gfx_line(x + r + 1, y + r, x - r - 1, y + r, 2, c);
        gfx_line(x - r - 1, y + r, x, y - r - 1, 2, c);
    }
}
