#include "mathview.h"
#include "gfx.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "l10n.h"
typedef struct {
    float w, h, base;
    int ok;
} Box;
static void value_text(CLValue v, char out[160]) {
    if (!v.exact && v.re == CL_PI && !v.im)
        strcpy(out, "pi");
    else if (!v.exact && v.re == exp(1) && !v.im)
        strcpy(out, "e");
    else
        cl_format(v, 12, 0, 1, out, 160);
}
static Box label_box(const char *s) { return (Box){(float)gfx_text_width(s, 1), 14, 9, 1}; }
static Box measure(const CLProgram *p, int i, int depth) {
    if (i < 0 || i >= p->count || depth > 16)
        return (Box){0, 0, 0, 0};
    const CLNode *n = &p->nodes[i];
    char s[160];
    if (n->kind == N_VALUE) {
        value_text(n->value, s);
        if (n->value.exact && n->value.den != 1) {
            char a[40], b[40];
            snprintf(a, sizeof(a), "%lld", (long long)n->value.num);
            snprintf(b, sizeof(b), "%lld", (long long)n->value.den);
            return (Box){fmaxf(gfx_text_width(a, 1), gfx_text_width(b, 1)) + 8, 33, 20, 1};
        }
        return label_box(s);
    }
    if (n->kind == N_VAR) {
        if (n->op == 7)
            strcpy(s, "x");
        else if (n->op == 0)
            strcpy(s, "Ans");
        else
            snprintf(s, sizeof(s), "%c", 'A' + n->op - 1);
        return label_box(s);
    }
    Box a = measure(p, n->a, depth + 1), b = {0, 0, 0, 1}, c = b;
    if (!a.ok)
        return a;
    if (n->b >= 0)
        b = measure(p, n->b, depth + 1);
    if (n->c >= 0)
        c = measure(p, n->c, depth + 1);
    if (!b.ok || !c.ok)
        return (Box){0, 0, 0, 0};
    if (n->kind == N_DIV)
        return (Box){fmaxf(a.w, b.w) + 8, a.h + b.h + 5, a.h + 6, 1};
    if (n->kind == N_POW)
        return (Box){a.w + b.w + 3, fmaxf(a.h + b.h / 2, b.h), a.base + b.h / 2, 1};
    if (n->kind == N_FUNC && n->op == F_SQRT)
        return (Box){a.w + 15, a.h + 3, a.base + 3, 1};
    float baseline = fmaxf(9, fmaxf(a.base, fmaxf(b.base, c.base))),
          bottom = fmaxf(5, fmaxf(a.h - a.base, fmaxf(b.h - b.base, c.h - c.base)));
    if (n->kind == N_NEG)
        return (Box){a.w + gfx_text_width("(-)", 1) + 4, baseline + bottom, baseline, 1};
    if (n->kind == N_FACT || n->kind == N_PERCENT)
        return (Box){a.w + gfx_text_width(n->kind == N_FACT ? "!" : "%", 1) + 2, a.h, a.base, 1};
    if (n->kind == N_FUNC) {
        float width =
            gfx_text_width(cl_function_name(n->op), 1) + gfx_text_width("()", 1) + a.w + 4;
        if (n->b >= 0)
            width += b.w + gfx_text_width(";", 1) + 4;
        if (n->c >= 0)
            width += c.w + gfx_text_width(";", 1) + 4;
        return (Box){width, baseline + bottom, baseline, 1};
    }
    if (n->kind >= N_ADD && n->kind <= N_MUL)
        return (Box){a.w + b.w +
                         gfx_text_width(n->kind == N_ADD   ? "(+)"
                                        : n->kind == N_SUB ? "(-)"
                                                           : "(*)",
                                        1) +
                         12,
                     baseline + bottom, baseline, 1};
    return (Box){0, 0, 0, 0};
}
static void draw(const CLProgram *p, int i, float x, float y, uint32_t ink, int depth) {
    const CLNode *n = &p->nodes[i];
    Box box = measure(p, i, depth);
    char s[160];
    if (n->kind == N_VALUE) {
        value_text(n->value, s);
        if (n->value.exact && n->value.den != 1) {
            char a[40], b[40];
            snprintf(a, sizeof(a), "%lld", (long long)n->value.num);
            snprintf(b, sizeof(b), "%lld", (long long)n->value.den);
            gfx_text(x + (box.w - gfx_text_width(a, 1)) / 2, y, a, 1, ink);
            gfx_line(x, y + 16, x + box.w, y + 16, 1, ink);
            gfx_text(x + (box.w - gfx_text_width(b, 1)) / 2, y + 19, b, 1, ink);
        } else
            gfx_text(x, y, s, 1, ink);
        return;
    }
    if (n->kind == N_VAR) {
        if (n->op == 7)
            strcpy(s, "x");
        else if (n->op == 0)
            strcpy(s, "Ans");
        else
            snprintf(s, sizeof(s), "%c", 'A' + n->op - 1);
        gfx_text(x, y, s, 1, ink);
        return;
    }
    Box a = measure(p, n->a, depth + 1), b = {0, 0, 0, 1};
    if (n->b >= 0)
        b = measure(p, n->b, depth + 1);
    if (n->kind == N_DIV) {
        draw(p, n->a, x + (box.w - a.w) / 2, y, ink, depth + 1);
        gfx_line(x, y + a.h + 2, x + box.w, y + a.h + 2, 1, ink);
        draw(p, n->b, x + (box.w - b.w) / 2, y + a.h + 5, ink, depth + 1);
        return;
    }
    if (n->kind == N_POW) {
        draw(p, n->a, x, y + b.h / 2, ink, depth + 1);
        draw(p, n->b, x + a.w + 3, y, ink, depth + 1);
        return;
    }
    if (n->kind == N_FUNC && n->op == F_SQRT) {
        float h = box.h;
        gfx_line(x + 1, y + h * .65f, x + 4, y + h - 2, 1, ink);
        gfx_line(x + 4, y + h - 2, x + 10, y + 1, 1, ink);
        gfx_line(x + 10, y + 1, x + box.w, y + 1, 1, ink);
        draw(p, n->a, x + 13, y + 3, ink, depth + 1);
        return;
    }
    float text_y = y + box.base - 9;
    if (n->kind == N_NEG) {
        gfx_text(x, text_y, "(-", 1, ink);
        float start = x + gfx_text_width("(-", 1) + 2;
        draw(p, n->a, start, y + box.base - a.base, ink, depth + 1);
        gfx_text(start + a.w + 2, text_y, ")", 1, ink);
        return;
    }
    if (n->kind == N_FACT || n->kind == N_PERCENT) {
        draw(p, n->a, x, y, ink, depth + 1);
        gfx_text(x + a.w + 2, y + box.base - 9, n->kind == N_FACT ? "!" : "%", 1, ink);
        return;
    }
    if (n->kind == N_FUNC) {
        snprintf(s, sizeof(s), "%s(", cl_function_name(n->op));
        gfx_text(x, text_y, s, 1, ink);
        float start = x + gfx_text_width(s, 1) + 2;
        draw(p, n->a, start, y + box.base - a.base, ink, depth + 1);
        start += a.w;
        if (n->b >= 0) {
            gfx_text(start + 2, text_y, ";", 1, ink);
            start += gfx_text_width(";", 1) + 4;
            draw(p, n->b, start, y + box.base - b.base, ink, depth + 1);
            start += b.w;
        }
        if (n->c >= 0) {
            Box c = measure(p, n->c, depth + 1);
            gfx_text(start + 2, text_y, ";", 1, ink);
            start += gfx_text_width(";", 1) + 4;
            draw(p, n->c, start, y + box.base - c.base, ink, depth + 1);
            start += c.w;
        }
        gfx_text(start + 2, text_y, ")", 1, ink);
        return;
    }
    const char *op = n->kind == N_ADD ? "+" : n->kind == N_SUB ? "-" : "*";
    gfx_text(x, text_y, "(", 1, ink);
    float start = x + gfx_text_width("(", 1) + 3;
    draw(p, n->a, start, y + box.base - a.base, ink, depth + 1);
    start += a.w + 3;
    gfx_text(start, text_y, op, 1, ink);
    start += gfx_text_width(op, 1) + 3;
    draw(p, n->b, start, y + box.base - b.base, ink, depth + 1);
    gfx_text(start + b.w + 3, text_y, ")", 1, ink);
}
int aero_draw_math(const CLProgram *p, float x, float y, float width, float height,
                   uint32_t color) {
    Box box = measure(p, p->root, 0);
    if (!box.ok || box.w > width || box.h > height)
        return 0;
    draw(p, p->root, x + (width - box.w) / 2, y + (height - box.h) / 2, color, 0);
    return 1;
}
