#include "app.h"
#include "gfx.h"
#include "l10n.h"
#include "mathview.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    uint32_t bg, panel, key, ink, muted, accent, selected, error, sky, border;
} Palette;
static Palette palette(int t) {
    if (t == 3)
        return (Palette){PP_RGB(61, 122, 201),  PP_RGB(255, 251, 231), PP_RGB(229, 237, 246),
                         PP_RGB(20, 49, 100),   PP_RGB(59, 88, 124),   PP_RGB(23, 87, 184),
                         PP_RGB(254, 217, 127), PP_RGB(171, 34, 49),   PP_RGB(133, 182, 244),
                         PP_RGB(98, 147, 197)};
    if (t == 4)
        return (Palette){PP_RGB(19, 70, 102),   PP_RGB(239, 250, 250), PP_RGB(190, 218, 229),
                         PP_RGB(13, 41, 62),    PP_RGB(57, 88, 108),   PP_RGB(0, 109, 161),
                         PP_RGB(112, 214, 242), PP_RGB(166, 34, 54),   PP_RGB(67, 143, 192),
                         PP_RGB(74, 142, 169)};
    if (t == 5)
        return (Palette){PP_RGB(47, 17, 91),    PP_RGB(41, 29, 72),    PP_RGB(68, 45, 109),
                         PP_RGB(252, 240, 255), PP_RGB(206, 191, 235), PP_RGB(103, 249, 242),
                         PP_RGB(114, 58, 144),  PP_RGB(255, 169, 171), PP_RGB(125, 50, 150),
                         PP_RGB(155, 112, 187)};
    if (t == 6)
        return (Palette){PP_RGB(12, 21, 29),    PP_RGB(20, 32, 42),    PP_RGB(38, 52, 59),
                         PP_RGB(242, 245, 226), PP_RGB(177, 197, 187), PP_RGB(255, 206, 119),
                         PP_RGB(93, 74, 43),    PP_RGB(255, 155, 148), PP_RGB(34, 56, 63),
                         PP_RGB(109, 127, 116)};
    if (t == 7)
        return (Palette){PP_RGB(24, 49, 58),    PP_RGB(239, 248, 243), PP_RGB(206, 229, 218),
                         PP_RGB(27, 59, 60),    PP_RGB(66, 104, 101),  PP_RGB(17, 116, 111),
                         PP_RGB(161, 225, 203), PP_RGB(164, 37, 52),   PP_RGB(120, 183, 180),
                         PP_RGB(105, 162, 149)};
    if (t == 1)
        return (Palette){PP_RGB(65, 155, 109),  PP_RGB(247, 253, 247), PP_RGB(212, 235, 217),
                         PP_RGB(29, 61, 44),    PP_RGB(66, 105, 78),   PP_RGB(13, 108, 59),
                         PP_RGB(159, 220, 175), PP_RGB(173, 35, 44),   PP_RGB(164, 216, 166),
                         PP_RGB(121, 173, 140)};
    if (t == 2)
        return (Palette){PP_RGB(11, 32, 57),    PP_RGB(27, 49, 73),    PP_RGB(41, 69, 98),
                         PP_RGB(244, 250, 255), PP_RGB(173, 201, 223), PP_RGB(138, 221, 255),
                         PP_RGB(46, 112, 159),  PP_RGB(255, 162, 158), PP_RGB(42, 89, 134),
                         PP_RGB(78, 116, 151)};
    return (Palette){PP_RGB(47, 158, 191),  PP_RGB(246, 252, 255), PP_RGB(215, 233, 245),
                     PP_RGB(24, 54, 77),    PP_RGB(63, 98, 120),   PP_RGB(0, 105, 157),
                     PP_RGB(150, 213, 243), PP_RGB(175, 36, 46),   PP_RGB(149, 215, 240),
                     PP_RGB(125, 177, 204)};
}
static void fit(float x, float y, const char *s, int max, int scale, uint32_t color) {
    s = L(s);
    int budget = (int)fminf(max * 6 * scale, 470 - x) - 3;
    if (budget <= 0)
        return;
    if (gfx_text_width(s, scale) <= budget)
        gfx_text(x, y, s, scale, color);
    else {
        int mark = gfx_text_width("~", scale);
        gfx_text_clipped(x, y, s, scale, color, budget - mark - 1);
        gfx_text(x + budget - mark, y, "~", scale, color);
    }
}
static const char *page_name(int p) {
    static const char *s[] = {"Calculator",
                              "Graph / table",
                              "Tools",
                              "Unit converter",
                              "Programmer",
                              "History / favorites",
                              "Settings",
                              "Help",
                              "Laboratory",
                              "Matrices A / B",
                              "Study mode",
                              "Formula / 2D",
                              "Language / 26 languages"};
    return p >= 0 && p < CL_PAGE_COUNT ? L(s[p]) : "?";
}
static const char *angle_name(int a) { return a == 0 ? "DEG" : a == 1 ? "RAD" : "GRAD"; }
static void panel(float x, float y, float w, float h, Palette p) {
    gfx_round(x, y + 1, w, h, 6, PP_RGBA(11, 45, 66, 28));
    gfx_round_gradient(x, y, w, h, 6, gfx_mix(p.border, PP_RGB(255, 255, 255), .35f), p.border);
    gfx_round_gradient(x + 1, y + 1, w - 2, h - 2, 5, gfx_mix(p.panel, PP_RGB(255, 255, 255), .04f),
                       p.panel);
}
static void glass_row(float x, float y, float w, float h, int selected, Palette p) {
    uint32_t base = selected ? p.selected : p.panel;
    gfx_round_gradient(x, y, w, h, 4, gfx_mix(base, PP_RGB(255, 255, 255), .13f),
                       gfx_mix(base, p.key, .2f));
    if (selected) {
        gfx_line(x + 5, y + 1, x + w - 5, y + 1, 1, PP_RGBA(255, 255, 255, 170));
        gfx_line(x + 5, y + h - 1, x + w - 5, y + h - 1, 1, p.border);
    }
}
static void background(Palette p) {
    gfx_gradient(0, 0, 480, 272, p.sky, p.bg);
    /* Quiet, translucent curves inspired by Aero's glass wallpaper. */
    static float wave[3][25];
    static int initialized;
    if (!initialized) {
        for (int b = 0; b < 3; b++)
            for (int i = 0; i < 25; i++)
                wave[b][i] = 181 + b * 18 + 24 * sinf(i * .12f + b * .6f);
        initialized = 1;
    }
    for (int band = 0; band < 3; ++band) {
        uint32_t c = band == 0 ? PP_RGBA(233, 255, 245, 36) : PP_RGBA(255, 255, 255, 22);
        for (int i = 0; i < 24; ++i) {
            float x1 = i * 20, x2 = x1 + 20;
            float y1 = wave[band][i];
            float y2 = wave[band][i + 1];
            gfx_triangle(x1, y1, x2, y2, x2, 246, c);
            gfx_triangle(x1, y1, x2, 246, x1, 246, c);
        }
    }
    if (p.bg == PP_RGB(47, 17, 91)) {
        for (int i = 0; i < 9; i++)
            gfx_line(15 + i * 56, 37, 15 + i * 56, 246, 1, PP_RGBA(93, 255, 246, 24));
        for (int i = 0; i < 7; i++)
            gfx_line(0, 62 + i * 28, 480, 62 + i * 28, 1, PP_RGBA(254, 157, 238, 28));
    }
    if (p.bg == PP_RGB(12, 21, 29))
        for (int y = 40; y < 246; y += 4)
            gfx_rect(0, y, 480, 1, PP_RGBA(0, 0, 0, 18));
    if (p.bg == PP_RGB(24, 49, 58))
        for (int i = 0; i < 24; i++)
            gfx_rect(9 + i * 20, 38, 2, 2, PP_RGBA(255, 255, 255, 100));
    gfx_round(7, 39, 466, 204, 8, PP_RGBA(255, 255, 255, 110));
    uint32_t body = (p.panel & UINT32_C(0x00ffffff)) | UINT32_C(0xdf000000);
    gfx_round(8, 40, 464, 202, 7, body);
}
static void header(const CLApp *a, Palette p) {
    int night = (p.ink & 255) > 200;
    gfx_gradient(0, 0, 480, 36, PP_RGBA(255, 255, 255, night ? 20 : 90),
                 PP_RGBA(255, 255, 255, night ? 5 : 18));
    gfx_line(0, 36, 480, 36, 1, PP_RGBA(255, 255, 255, 120));
    gfx_round(11, 5, 29, 29, 8, PP_RGBA(12, 52, 74, 48));
    gfx_round_gradient(11, 4, 29, 29, 8, PP_RGB(204, 252, 231), PP_RGB(32, 155, 152));
    gfx_round_gradient(13, 6, 25, 12, 5, PP_RGBA(255, 255, 255, 190), PP_RGBA(255, 255, 255, 10));
    gfx_text(20, 3, "f", 2, PP_RGB(21, 91, 105));
    gfx_text(47, 3, "CalcAero", 2, p.ink);
    gfx_text(184, 22, "PSP / 1.3", 1, p.muted);
    char s[96];
    uint32_t pill = (p.panel & UINT32_C(0x00ffffff)) | UINT32_C(0xb0000000);
    gfx_round(282, 5, 46, 16, 5, pill);
    gfx_round(332, 5, 52, 16, 5, pill);
    gfx_round(388, 5, 77, 16, 5, pill);
    gfx_text(288, 6, angle_name(a->save.ctx.angle), 1, p.accent);
    gfx_text(339, 6, a->save.ctx.complex_mode ? "CPLX" : "REAL", 1, p.ink);
    snprintf(s, sizeof(s), "%d DIG", a->save.digits);
    gfx_text(402, 6, s, 1, p.ink);
    fit(282, 22, page_name(a->page), 31, 1, p.muted);
}
static void footer(const char *s1, const char *s2, Palette p) {
    gfx_gradient(0, 247, 480, 25, p.panel, gfx_mix(p.panel, p.key, .5f));
    gfx_line(0, 247, 480, 247, 1, PP_RGBA(255, 255, 255, 180));
    fit(10, 246, s1, 76, 1, p.ink);
    fit(10, 258, s2, 76, 1, p.muted);
}
static void editor(const CLApp *a, const char *s, int y, int rows, Palette p) {
    int len = (int)strlen(s), cell = gfx_mono_width(), width = 440 / cell, cur = a->cursor;
    if (cur < 0)
        cur = 0;
    if (cur > len)
        cur = len;
    int start = 0;
    if (cur >= width * rows)
        start = cur - width * rows + 1;
    char out[CL_EXPR];
    int n = len - start;
    if (n > width * rows)
        n = width * rows;
    memcpy(out, s + start, (size_t)n);
    out[n] = 0;
    for (int row = 0; row < rows; ++row) {
        char part[75];
        int offset = row * width, amount = n - offset;
        if (amount < 0)
            amount = 0;
        if (amount > width)
            amount = width;
        memcpy(part, out + offset, (size_t)amount);
        part[amount] = 0;
        gfx_text_mono(17, y + row * 13, part, p.ink);
    }
    int relative = cur - start;
    if (relative < width * rows)
        gfx_line(18 + (relative % width) * cell, y + (relative / width) * 13 + 12,
                 18 + (relative % width) * cell, y + (relative / width) * 13 + 2, 1, p.accent);
    if (!len)
        gfx_text(17, y, "Enter an expression...", 1, p.muted);
}
static void keypad(const CLApp *a, int y, Palette p) {
    for (int i = 0; i < 30; ++i) {
        int x = 11 + (i % 6) * 77, yy = y + (i / 6) * 22;
        const CLKey *k = cl_key(a->keypage, i);
        int selected = !a->editing && i == a->keysel;
        uint32_t base = selected ? p.selected : p.key;
        gfx_key_mask(x, yy + 1, 0, PP_RGBA(12, 50, 72, 40), PP_RGBA(12, 50, 72, 40));
        gfx_key_mask(x, yy, 0, selected ? p.accent : p.border, selected ? p.accent : p.border);
        gfx_key_mask(x + 1, yy + 1, 1, gfx_mix(base, p.panel, .78f), base);
        gfx_key_mask(x + 2, yy + 2, 2, PP_RGBA(255, 255, 255, 80), PP_RGBA(255, 255, 255, 5));
        int w = gfx_text_width(k->label, 1);
        gfx_text(x + (73 - w) / 2, yy + 2, k->label, 1, p.ink);
    }
}
static void calc(const CLApp *a, Palette p) {
    panel(11, 42, 458, 80, p);
    CLProgram program;
    CLError compile_error;
    int pretty = a->extra.pretty && !a->editing && a->save.expr[0] &&
                 aero_compile(a->save.expr, &a->extra, &program, &compile_error) &&
                 aero_draw_math(&program, 17, 45, 442, 34, p.ink);
    if (!pretty)
        editor(a, a->save.expr, 47, 2, p);
    if (a->error.code) {
        char msg[160];
        snprintf(msg, sizeof(msg), "%s", cl_error_text(a->error.code));
        fit(17, 82, msg, 73, 1, p.error);
        snprintf(msg, sizeof(msg), "Position: %d", a->error.pos + 1);
        gfx_text(17, 101, msg, 1, p.muted);
    } else if (a->has_result) {
        char s[160], prefixed[180];
        cl_format(a->result, a->save.digits, a->save.style, a->save.fraction, s, sizeof(s));
        int exact = a->result.exact && a->save.fraction;
        snprintf(prefixed, sizeof(prefixed), "%s %s", exact ? "=" : "~=", s);
        int scale = gfx_text_width(prefixed, 2) < 440 ? 2 : 1;
        fit(17, 82, prefixed, scale == 2 ? 36 : 73, scale, p.accent);
    } else
        fit(17, 91, a->notice[0] ? a->notice : "START: calculate; SELECT: angle units", 73, 1,
            p.muted);
    char label[128];
    snprintf(label, sizeof(label), "%d/6  %s%s", a->keypage + 1, cl_keypage_name(a->keypage),
             a->editing ? " / CURSOR" : "");
    fit(13, 122, label, 74, 1, p.muted);
    keypad(a, 136, p);
    footer("X: enter  O: delete  []: cursor  START: =",
           "L/R: keys  SELECT: DEG/RAD/GRAD  /\\: menu", p);
}
static void graph(const CLApp *a, Palette p) {
    const int *valid = a->graph_series ? a->other_valid[a->graph_series - 1] : a->graph_valid;
    const double *values = a->graph_series ? a->other_y[a->graph_series - 1] : a->graph_y;
    uint32_t colors[3] = {p.accent,
                          a->save.theme == 2 ? PP_RGB(123, 235, 169) : PP_RGB(20, 129, 66),
                          a->save.theme == 2 ? PP_RGB(255, 195, 126) : PP_RGB(192, 81, 28)};
    panel(11, 42, 458, 25, p);
    char expression[CL_EXPR + 32];
    snprintf(expression, sizeof(expression), "G%d: %s", a->graph_series + 1,
             cl_graph_expression(a, a->graph_series)[0] ? cl_graph_expression(a, a->graph_series)
                                                        : "[]: enter a new function");
    fit(17, 47, expression, 73, 1, colors[a->graph_series]);
    if (a->error.code) {
        fit(17, 91, cl_error_text(a->error.code), 73, 1, p.error);
        footer("[]: enter f(x)  START: plot", "/\\: menu  Angle mode: in settings", p);
        return;
    }
    if (a->graph_focus == 2) {
        panel(11, 74, 458, 165, p);
        char heading[100];
        snprintf(heading, sizeof(heading), "TABLE     x                    G%d(x)",
                 a->graph_series + 1);
        gfx_text(20, 78, heading, 1, colors[a->graph_series]);
        int start = a->graph_trace - 4;
        if (start < 0)
            start = 0;
        if (start > CL_GRAPH_POINTS - 9)
            start = CL_GRAPH_POINTS - 9;
        for (int row = 0; row < 9; ++row) {
            int i = start + row;
            double x = a->xmin + (a->xmax - a->xmin) * i / (CL_GRAPH_POINTS - 1);
            char s[160];
            if (valid[i])
                snprintf(s, sizeof(s), "%s %.12g       %.12g", i == a->graph_trace ? ">" : " ", x,
                         values[i]);
            else
                snprintf(s, sizeof(s), "%s %.12g       outside domain",
                         i == a->graph_trace ? ">" : " ", x);
            fit(20, 97 + row * 15, s, 72, 1, i == a->graph_trace ? p.accent : p.ink);
        }
    } else {
        const double left = 42, right = 458, top = 88, bottom = 216, w = right - left,
                     h = bottom - top;
        panel((float)left - 1, (float)top - 1, (float)w + 2, (float)h + 2, p);
        for (int j = 1; j < 8; ++j)
            gfx_line((float)(left + w * j / 8), (float)top, (float)(left + w * j / 8),
                     (float)bottom, 1, p.key);
        for (int j = 1; j < 4; ++j)
            gfx_line((float)left, (float)(top + h * j / 4), (float)right, (float)(top + h * j / 4),
                     1, p.key);
        if (a->xmin <= 0 && a->xmax >= 0) {
            float x = (float)(left + w * (-a->xmin) / (a->xmax - a->xmin));
            gfx_line(x, (float)top, x, (float)bottom, 1, p.muted);
        }
        if (a->ymin <= 0 && a->ymax >= 0) {
            float y = (float)(bottom - h * (-a->ymin) / (a->ymax - a->ymin));
            gfx_line((float)left, y, (float)right, y, 1, p.muted);
        }
        for (int curve = 0; curve < AERO_CURVES; ++curve) {
            const int *cv = curve ? a->other_valid[curve - 1] : a->graph_valid;
            const double *cy = curve ? a->other_y[curve - 1] : a->graph_y;
            for (int i = 1; i < a->graph_index; ++i) {
                if (!cv[i - 1] || !cv[i])
                    continue;
                double y1 = bottom - h * (cy[i - 1] - a->ymin) / (a->ymax - a->ymin),
                       y2 = bottom - h * (cy[i] - a->ymin) / (a->ymax - a->ymin);
                if (!isfinite(y1) || !isfinite(y2) || fabs(y1 - y2) > h * 1.5 ||
                    (y1 < top && y2 < top) || (y1 > bottom && y2 > bottom))
                    continue;
                y1 = fmin(bottom, fmax(top, y1));
                y2 = fmin(bottom, fmax(top, y2));
                gfx_line_smooth((float)(left + w * (i - 1) / (CL_GRAPH_POINTS - 1)), (float)y1,
                                (float)(left + w * i / (CL_GRAPH_POINTS - 1)), (float)y2,
                                curve == a->graph_series ? 2 : 1.5f, colors[curve]);
            }
        }
        float tx = (float)(left + w * a->graph_trace / (CL_GRAPH_POINTS - 1));
        gfx_line(tx, (float)top, tx, (float)bottom, 1, p.ink);
        char b[96];
        snprintf(b, sizeof(b), "y [%.5g; %.5g]", a->ymin, a->ymax);
        fit(14, 73, b, 73, 1, p.muted);
        for (int k = 0; k < 3; ++k) {
            char label[16];
            snprintf(label, sizeof(label), "%sG%d", k == a->graph_series ? ">" : "", k + 1);
            gfx_text(334 + k * 43, 72, label, 1, colors[k]);
        }
        snprintf(b, sizeof(b), "x [%.5g; %.5g]", a->xmin, a->xmax);
        fit(14, 217, b, 73, 1, p.muted);
        double x = a->xmin + (a->xmax - a->xmin) * a->graph_trace / (CL_GRAPH_POINTS - 1);
        if (valid[a->graph_trace])
            snprintf(b, sizeof(b), "x=%.9g   y=%.12g", x, values[a->graph_trace]);
        else
            snprintf(b, sizeof(b), "x=%.9g   y: outside domain / not calculated yet", x);
        fit(14, 231, b, 74, 1, p.accent);
    }
    footer(a->graph_focus == 1 ? "WINDOW: arrows move; L/R: zoom"
                               : "CURSOR: left/right; SELECT: window/table",
           "L/R: graph  []: input  X: analyze  /\\: menu", p);
}
static void tools(const CLApp *a, Palette p) {
    fit(14, 42, cl_tool_name(a->tool), 73, 1, p.accent);
    fit(14, 55, cl_tool_hint(a->tool), 73, 1, p.muted);
    panel(11, 72, 458, 47, p);
    editor(a, a->input[0], 77, 3, p);
    if (a->tool_result.count) {
        panel(11, 126, 458, 115, p);
        for (int i = 0; i < 8 && i + a->tool_scroll < a->tool_result.count; ++i)
            fit(17, 130 + i * 13, a->tool_result.line[i + a->tool_scroll], 73, 1,
                a->tool_result.ok ? p.ink : p.error);
        footer("X/O: return to input  up/down: report",
               "[]: other tool  START: calculate  /\\: menu", p);
    } else {
        fit(14, 122, cl_keypage_name(a->keypage), 74, 1, p.muted);
        keypad(a, 136, p);
        footer("[]: next tool  START: calculate", "L/R: keys  /\\ -> PSP keyboard: edit", p);
    }
}
static void units(const CLApp *a, Palette p) {
    char s[160];
    snprintf(s, sizeof(s), "%s %s", a->unit_row == 0 ? ">" : " ",
             cl_unit_group_name(a->unit_group));
    fit(14, 41, s, 74, 1, p.accent);
    snprintf(s, sizeof(s), "%s From: %s       %s To: %s", a->unit_row == 1 ? ">" : " ",
             cl_unit_name(a->unit_group, a->unit_from), a->unit_row == 2 ? ">" : " ",
             cl_unit_name(a->unit_group, a->unit_to));
    fit(14, 55, s, 74, 1, p.ink);
    panel(11, 73, 458, 48, p);
    editor(a, a->input[1], 77, 2, p);
    if (a->unit_ok) {
        panel(11, 128, 458, 112, p);
        gfx_text(17, 139, "RESULT", 1, p.muted);
        snprintf(s, sizeof(s), "~= %.12g %s", a->unit_result,
                 cl_unit_name(a->unit_group, a->unit_to));
        fit(17, 165, s, 36, 2, p.accent);
        fit(17, 205, "X: change value", 72, 1, p.muted);
    } else {
        fit(14, 123, a->notice, 73, 1, p.error);
        keypad(a, 136, p);
    }
    footer("[]: category/from/to  L/R: select  START: convert",
           "X: enter  O: delete  /\\: menu and keyboard", p);
}
static void base(const CLApp *a, Palette p) {
    static const char *ops[] = {"CONVERT", "AND", "OR",  "XOR", "NOT",          "SHL",
                                "SHR",     "ADD", "SUB", "MUL", "DIV unsigned", "MOD unsigned"};
    char s[160];
    snprintf(s, sizeof(s), "%s %d BIT      %s %s", a->base_row == 0 ? ">" : " ", a->base_bits,
             a->base_row == 1 ? ">" : " ", ops[a->base_op]);
    gfx_text(14, 43, s, 1, p.accent);
    gfx_text(14, 58, "Two operands: a;b. ADD/SUB/MUL: modulo 2^bits", 1, p.muted);
    panel(11, 73, 458, 47, p);
    editor(a, a->input[2], 77, 2, p);
    if (a->base_ok) {
        panel(11, 128, 458, 112, p);
        char v[100];
        int bases[] = {10, 10, 16, 8, 2};
        const char *names[] = {"DEC U", "DEC S", "HEX", "OCT", "BIN"};
        for (int i = 0; i < 5; ++i) {
            cl_base_format(a->base_result, a->base_bits, bases[i], i == 1, v, sizeof(v));
            if (i == 4 && strlen(v) > 32) {
                char first[33];
                memcpy(first, v, 32);
                first[32] = 0;
                gfx_text(17, 205, "BIN:", 1, p.ink);
                gfx_text_mono(48, 205, first, p.ink);
                gfx_text_mono(48, 219, v + 32, p.ink);
                continue;
            }
            snprintf(s, sizeof(s), "%s: %s", names[i], v);
            fit(17, 133 + i * 18, s, 73, 1, i == 2 ? p.accent : p.ink);
        }
    } else {
        fit(14, 123, a->notice, 73, 1, p.error);
        keypad(a, 136, p);
    }
    footer("[]: bits/operation  L/R: select  START: calculate",
           "X: enter/edit  O: delete  /\\: menu", p);
}
static void history(const CLApp *a, Palette p) {
    gfx_text(14, 43,
             a->history_bookmarks ? "FAVORITE FORMULAS"
                                  : "HISTORY: angle mode and memory are saved",
             1, p.accent);
    int count = a->history_bookmarks ? CL_BOOKMARKS : a->save.history_count,
        start = (a->history_row / 7) * 7;
    if (!count)
        gfx_text(17, 91, "History is empty", 1, p.muted);
    for (int row = 0; row < 7 && start + row < count; ++row) {
        int i = start + row, y = 61 + row * 25;
        glass_row(11, y, 458, 23, i == a->history_row, p);
        const char *text = a->history_bookmarks ? a->save.bookmark[i] : a->save.history[i].expr;
        char s[CL_EXPR + 64];
        snprintf(s, sizeof(s), "%02d  %s", i + 1, text[0] ? text : "(empty)");
        fit(17, y, s, 73, 1, i == a->history_row ? p.accent : p.ink);
        if (!a->history_bookmarks) {
            const CLHistory *h = &a->save.history[i];
            char v[160];
            cl_format(h->value, 12, 0, 1, v, sizeof(v));
            snprintf(s, sizeof(s), "%s  %s %s", angle_name(h->angle),
                     h->value.exact ? "=" : "~=", v);
            fit(17, y + 11, s, 73, 1, p.muted);
        }
    }
    footer("X: restore  SELECT: history/favorites", "[]: add/remove favorite  /\\: menu", p);
}
static void settings(const CLApp *a, Palette p) {
    char v[7][80];
    snprintf(v[0], 80, "%s", angle_name(a->save.ctx.angle));
    snprintf(v[1], 80, "%s", a->save.ctx.complex_mode ? "On" : "Off");
    snprintf(v[2], 80, "%d significant digits", a->save.digits);
    snprintf(v[3], 80, "%s",
             a->save.style == 0   ? "Auto"
             : a->save.style == 1 ? "Scientific"
                                  : "Engineering");
    snprintf(v[4], 80, "%s", a->save.fraction ? "Exact fraction" : "Decimal result");
    static const char *theme_names[] = {"Aero Blue",       "Aero Green",         "Vista Night",
                                        "Windows XP Luna", "Windows Vista Aero", "Vaporwave",
                                        "Night Retro",     "WebCore Math"};
    snprintf(v[5], 80, "%s", theme_names[a->save.theme % 8]);
    snprintf(v[6], 80, "%s", a->extra.pretty ? "Textbook notation" : "Input line");
    const char *names[] = {"Angle units", "Complex numbers", "Display",     "Number format",
                           "Fractions",   "Theme",           "Formula view"};
    for (int i = 0; i < 7; ++i) {
        int y = 48 + i * 24;
        glass_row(11, y, 458, 22, i == a->settings_row, p);
        fit(17, y + 5, names[i], 34, 1, p.ink);
        fit(240, y + 5, v[i], 36, 1, p.accent);
    }
    fit(17, 218, "Display rounding keeps internal precision.", 73, 1, p.muted);
    fit(17, 231, "Memory and history: autosaved to card.", 73, 1, p.muted);
    footer("Up/down: row  left/right/X: change", "/\\: menu", p);
}
static void help(const CLApp *a, Palette p) {
    static const char *pages[CL_HELP_PAGES][15] = {
        {"CONTROLS", "X selects a key, O deletes before the cursor.",
         "START calculates; SELECT changes DEG/RAD/GRAD.",
         "[] in calculator enables cursor movement.",
         "Cursor mode: left/right, up=start, down=end.", "L/R switch the six keypad pages.",
         "/\\ opens the menu of screens and actions.",
         "PSP keyboard: the system on-screen keyboard.", "UNDO: undo/redo the last edit.",
         "STO A..F stores the result in memory.",
         "History -> X restores formula, angle and memory.",
         "SELECT in history switches to favorites.", "HOME: exit; state will be saved.", ""},
        {"SYNTAX AND EXACT NUMBERS", "Separate function arguments with ;",
         "Example: log(100;10), ncr(5;2), dms(30;15;0).", "Decimal point or comma: 1.25 / 1,25.",
         "Powers associate right: 2^3^2 = 512.", "-2^2 = -4; (-2)^2 = 4; 2^-3 = 1/8.",
         "Implicit multiplication: 2pi, 2(3+4), 2sin(30).",
         "Multiplication/division: same priority, left to right.",
         "For 1/(2pi), always use parentheses.", "% means /100: 200*10% = 20.",
         "Exact fractions: int64 numerator/denominator.",
         "Fraction overflow: switch to approximation.", "~= means an approximate/decimal result.",
         "0^0 and division by zero produce errors."},
        {"FUNCTIONS AND ANGLES", "sin/cos/tan and inverses: current DEG/RAD/GRAD.",
         "Hyperbolic functions do not depend on angle units.",
         "sqrt, cbrt, root(number;integer root degree).",
         "ln: natural; log: base 10 or log(x;base).", "atan2(y;x); polar(magnitude;angle); arg(z).",
         "Complex mode: sqrt(-1), re/im/conj/abs.", "Complex powers use the principal branch.",
         "Real odd root: root(-8;3).", "! / fact: integer 0..170; ncr/npr: n up to 1000.",
         "mod: remainder with the first argument's sign.",
         "Memory A..F: uppercase; e: Euler's number.", "c0, hplanck, qe, kb, na, g0: SI constants.",
         "Very large angles are rejected for precision.",
         "Double precision; display 6..15 significant digits."},
        {"TOOLS", "[] selects a tool and inserts its example.",
         "Quadratic: a;b;c. Complex roots are supported.",
         "Systems: row coefficients, then right-hand side.",
         "Matrix: n;row-major elements; n from 1 to 4.",
         "Statistics: ; separated numbers, up to 64 values.", "sigma: divisor n; s: divisor n-1.",
         "Root: f(x)|a|b, continuous function, sign change.",
         "Derivative: f(x)|point; example sin(x)|30.", "Integral: f(x)|a|b; for smooth functions.",
         "Residual and error are reported with the result.",
         "Numerical error estimates are not proofs.", "Mini-CAS and explanations: Laboratory menu.",
         "Graph: 241 samples; discontinuities may be missed.",
         "SELECT in graph: cursor -> window -> table."},
        {"UNITS AND PROGRAMMER", "Units: [] selects category/from/to, L/R change.",
         "Value can be an expression: 1/3, 2*pi.", "12 categories; kB=1000 B, KiB=1024 B.",
         "C/F/K: absolute temperature, not differences.", "Programmer: 8/16/32/64 bits.",
         "Input DEC, 0xHEX, 0bBIN, 0oOCT. Operands: a;b.", "DIV/MOD and SHR use unsigned integers.",
         "SHR is logical; SHL discards high bits.", "ADD/SUB/MUL: result modulo 2^bits.",
         "DEC S shows the same bits as a signed integer.",
         "Shift: 0..bits-1; input overflow is rejected.",
         "X on the result screen returns to the keypad.", "Long binary numbers are shown in full.",
         ""},
        {"SAVING AND LIMITS", "32 history items, 8 favorites, 6 memory slots.",
         "History records Ans and variables before calculation.",
         "Settings/history: state.dat, backup state.dat.bak.",
         "Damaged files are rejected using CRC checks.",
         "If the main file is invalid, the backup is used.",
         "Menu -> Export history: UTF-8 TXT in reports/.",
         "Existing reports are never overwritten.",
         "Input limit: 511 characters; syntax tree: 256 nodes.",
         "Nesting is limited to protect the stack.",
         "Double precision is limited, especially when subtracting",
         "nearby numbers or solving ill-conditioned systems.",
         "NaN, infinity and detected underflow are rejected.",
         "Data is written only inside the application folder.",
         "Sources and tests are included in the ZIP."},
        {"LABORATORY AND MATRICES", "Laboratory: up/down selects a tool.",
         "X opens an example; START generates the report.",
         "SELECT returns to catalog; X/O: edit input.",
         "Formulas: L/R selects one of 12; unknown is ?.",
         "Custom function: f|x^2+1; call f(3). Also g and h.",
         "Recursion is forbidden; f| clears the definition.",
         "Dimensions: 60 km/(2 h)|km/h; case-sensitive.",
         "Mini-CAS: polynomials <=8, exact coefficients.",
         "Derivatives: basic rules and DEG/RAD/GRAD.",
         "Matrices: arrows select a cell, X edits it.",
         "[] changes size 1..4; SELECT switches A/B.",
         "L/R selects operation; START generates report.",
         "Study: START grades, SELECT changes topic.",
         "Wrong answer: O retry; X/[] next question."},
        {"GRAPHS AND EXPORTS", "Three graphs: L/R selects G1/G2/G3 in cursor mode.",
         "[] opens the keyboard for the selected formula.",
         "SELECT: cursor -> window -> selected curve table.", "In window mode, L/R changes zoom.",
         "X: roots/extrema, intersections with the next G.",
         "Analysis: 129 samples; discontinuities may be missed.",
         "Custom functions in graphs: f(x), g(x), h(x).",
         "Menu -> Formula: fractions, powers, square roots.",
         "Long 2D notation: multiline source view.",
         "CSV: graphs/matrix; other screens export history.",
         "SVG: graph image; BMP: 480x272 screenshot.", "Tool results: separate UTF-8 TXT reports.",
         "Extra state: aero.dat and aero.dat.bak, with CRC.",
         "Reports in reports/; older files are kept."}};
    char n[40];
    snprintf(n, sizeof(n), "HELP %d/%d", a->help_page + 1, CL_HELP_PAGES);
    gfx_text(14, 42, n, 1, p.accent);
    for (int i = 0; i < 15; ++i)
        if (pages[a->help_page][i])
            fit(14, 61 + i * 12, pages[a->help_page][i], 74, 1, i == 0 ? p.accent : p.ink);
    footer("L/R: help page", "/\\: menu", p);
}
#include "ui_aero.inc"
static void menu(const CLApp *a, Palette p) {
    gfx_rect(0, 37, 480, 209, PP_RGBA(15, 42, 61, 95));
    gfx_round(40, 40, 400, 204, 9, PP_RGBA(9, 30, 49, 70));
    panel(42, 39, 396, 204, p);
    gfx_round_gradient(44, 40, 392, 21, 5, gfx_mix(p.key, p.panel, .6f), p.key);
    fit(57, 44, "CALCAERO / MENU", 30, 1, p.accent);
    char language[96];
    snprintf(language, sizeof(language), "SELECT: %s", L("Language / 26 languages"));
    fit(245, 44, language, 29, 1, p.accent);
    int start = a->menu_row > 6 ? a->menu_row - 6 : 0;
    if (start > CL_MENU_COUNT - 7)
        start = CL_MENU_COUNT - 7;
    for (int i = 0; i < 7; ++i) {
        int row = start + i, y = 65 + i * 23;
        glass_row(53, y, 374, 20, row == a->menu_row, p);
        fit(63, y + 4, cl_menu_name(row), 58, 1, row == a->menu_row ? p.accent : p.ink);
    }
    footer("Up/down: item  X: open", "O or /\\: close menu", p);
}
static void languages(const CLApp *a, Palette p) {
    fit(15, 42, "Choose your language / 26", 73, 1, p.accent);
    for (int i = 0; i < LANG_COUNT; i++) {
        int x = 11 + (i / 13) * 232, y = 62 + (i % 13) * 13;
        int chosen = i == a->language_row;
        if (chosen)
            gfx_round(x, y, 226, 13, 3, p.selected);
        char label[96];
        snprintf(label, sizeof(label), "%02d %s%s", i + 1, ca_language_name(i),
                 i == ca_language() ? " *" : "");
        fit(x + 5, y, label, 36, 1, chosen ? p.accent : p.ink);
    }
    fit(15, 234, "X: select and save  O: calculator", 73, 1, p.muted);
    footer("Arrows: select  X/START: save language", "Language is saved in config.json", p);
}
void cl_ui(const CLApp *a) {
    Palette p = palette(a->save.theme);
    gfx_begin();
    background(p);
    header(a, p);
    switch (a->page) {
    case PAGE_LANGUAGE:
        languages(a, p);
        break;
    case PAGE_CALC:
        calc(a, p);
        break;
    case PAGE_GRAPH:
        graph(a, p);
        break;
    case PAGE_TOOLS:
        tools(a, p);
        break;
    case PAGE_UNITS:
        units(a, p);
        break;
    case PAGE_BASE:
        base(a, p);
        break;
    case PAGE_HISTORY:
        history(a, p);
        break;
    case PAGE_SETTINGS:
        settings(a, p);
        break;
    case PAGE_HELP:
        help(a, p);
        break;
    case PAGE_LAB:
        lab(a, p);
        break;
    case PAGE_MATRIX:
        matrix_editor(a, p);
        break;
    case PAGE_STUDY:
        study(a, p);
        break;
    case PAGE_FORMULA:
        formula_view(a, p);
        break;
    }
    if (a->menu)
        menu(a, p);
    else if (a->notice[0]) {
        gfx_rect(0, 259, 480, 13, p.panel);
        fit(10, 258, a->notice, 76, 1, p.accent);
    }
    if (a->save_error) {
        gfx_rect(0, 35, 480, 13, p.error);
        fit(11, 34, "Save failed: check the memory card", 76, 1, PP_RGB(255, 255, 255));
    }
}
