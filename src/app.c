#include "app.h"
#include "l10n.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { A_NONE, A_CLEAR, A_UNDO, A_COMPUTE, A_FRACTION, A_STORE, A_RECALL };
#define K(label, text) {label, text, A_NONE}
#define ACT(label, action) {label, "", action}
static const CLKey keys[6][30] = {
    {K("7", "7"),
     K("8", "8"),
     K("9", "9"),
     K("/", "/"),
     K("(", "("),
     K(")", ")"),
     K("4", "4"),
     K("5", "5"),
     K("6", "6"),
     K("*", "*"),
     K("^", "^"),
     K("sqrt", "sqrt("),
     K("1", "1"),
     K("2", "2"),
     K("3", "3"),
     K("-", "-"),
     K(".", "."),
     K("Ans", "Ans"),
     K("0", "0"),
     K("+", "+"),
     K("pi", "pi"),
     K("e", "e"),
     K("E+", "e+"),
     K(";", ";"),
     ACT("AC", A_CLEAR),
     ACT("UNDO", A_UNDO),
     K("%", "%"),
     K("!", "!"),
     ACT("a/b", A_FRACTION),
     ACT("=", A_COMPUTE)},
    {K("sin", "sin("),     K("cos", "cos("),     K("tan", "tan("),     K("asin", "asin("),
     K("acos", "acos("),   K("atan", "atan("),   K("sinh", "sinh("),   K("cosh", "cosh("),
     K("tanh", "tanh("),   K("asinh", "asinh("), K("acosh", "acosh("), K("atanh", "atanh("),
     K("atan2", "atan2("), K("d2r", "d2r("),     K("r2d", "r2d("),     K("d2g", "d2g("),
     K("g2d", "g2d("),     K("dms", "dms("),     K("ln", "ln("),       K("log", "log("),
     K("log2", "log2("),   K("exp", "exp("),     K("root", "root("),   K("cbrt", "cbrt("),
     K("(", "("),          K(")", ")"),          K(";", ";"),          K("pi", "pi"),
     K("Ans", "Ans"),      ACT("=", A_COMPUTE)},
    {K("abs", "abs("),
     K("floor", "floor("),
     K("ceil", "ceil("),
     K("round", "round("),
     K("trunc", "trunc("),
     K("frac", "frac("),
     K("nCr", "ncr("),
     K("nPr", "npr("),
     K("fact", "fact("),
     K("gcd", "gcd("),
     K("lcm", "lcm("),
     K("mod", "mod("),
     K("min", "min("),
     K("max", "max("),
     K("clamp", "clamp("),
     K("sign", "sign("),
     K("inv", "inv("),
     K("hypot", "hypot("),
     K("re", "re("),
     K("im", "im("),
     K("conj", "conj("),
     K("arg", "arg("),
     K("polar", "polar("),
     K("i", "i"),
     K("(", "("),
     K(")", ")"),
     K(";", ";"),
     K("^", "^"),
     ACT("a/b", A_FRACTION),
     ACT("=", A_COMPUTE)},
    {K("A", "A"),
     K("B", "B"),
     K("C", "C"),
     K("D", "D"),
     K("E", "E"),
     K("F", "F"),
     {"STO A", "A", A_STORE},
     {"STO B", "B", A_STORE},
     {"STO C", "C", A_STORE},
     {"STO D", "D", A_STORE},
     {"STO E", "E", A_STORE},
     {"STO F", "F", A_STORE},
     K("x", "x"),
     K("Ans", "Ans"),
     K("pi", "pi"),
     K("e", "e"),
     K("i", "i"),
     K("|", "|"),
     K("c0", "c0"),
     K("h", "hplanck"),
     K("qe", "qe"),
     K("kB", "kb"),
     K("NA", "na"),
     K("g0", "g0"),
     K("(", "("),
     K(")", ")"),
     K(";", ";"),
     ACT("UNDO", A_UNDO),
     ACT("AC", A_CLEAR),
     ACT("=", A_COMPUTE)},
    {K("0", "0"),         K("1", "1"),   K("2", "2"),   K("3", "3"),   K("4", "4"),
     K("5", "5"),         K("6", "6"),   K("7", "7"),   K("8", "8"),   K("9", "9"),
     K("A", "A"),         K("B", "B"),   K("C", "C"),   K("D", "D"),   K("E", "E"),
     K("F", "F"),         K("0x", "0x"), K("0b", "0b"), K("0o", "0o"), K(";", ";"),
     K("-", "-"),         K("+", "+"),   K("(", "("),   K(")", ")"),   ACT("AC", A_CLEAR),
     ACT("UNDO", A_UNDO), K("|", "|"),   K("x", "x"),   K(".", "."),   ACT("=", A_COMPUTE)},
    {K("7", "7"),        K("8", "8"),         K("9", "9"),        K("/", "/"),
     K("(", "("),        K(")", ")"),         K("4", "4"),        K("5", "5"),
     K("6", "6"),        K("*", "*"),         K("^", "^"),        K("x", "x"),
     K("1", "1"),        K("2", "2"),         K("3", "3"),        K("-", "-"),
     K(".", "."),        K("pi", "pi"),       K("0", "0"),        K("+", "+"),
     K("f(x)", "f("),    K("g(x)", "g("),     K("|", "|"),        K(";", ";"),
     ACT("AC", A_CLEAR), ACT("UNDO", A_UNDO), K("sqrt", "sqrt("), K("h(x)", "h("),
     K("Ans", "Ans"),    ACT("=", A_COMPUTE)}};
const CLKey *cl_key(int page, int index) {
    return &keys[(page % 6 + 6) % 6][(index % 30 + 30) % 30];
}
const char *cl_keypage_name(int p) {
    static const char *n[] = {"Numbers",     "Angles / logarithms",
                              "Functions",   "Memory / constants",
                              "HEX / input", "Graph / x"};
    return L(n[(p % 6 + 6) % 6]);
}
static void say(CLApp *a, const char *s) { snprintf(a->notice, sizeof(a->notice), "%s", L(s)); }
static const int menu_pages[] = {PAGE_CALC,    PAGE_GRAPH,    PAGE_LAB,   PAGE_MATRIX,
                                 PAGE_STUDY,   PAGE_TOOLS,    PAGE_UNITS, PAGE_BASE,
                                 PAGE_HISTORY, PAGE_SETTINGS, PAGE_HELP};
int cl_menu_index(int p) {
    if (p == PAGE_LANGUAGE)
        return 21;
    if (p == PAGE_FORMULA)
        return 20;
    for (size_t i = 0; i < sizeof(menu_pages) / sizeof(menu_pages[0]); ++i)
        if (menu_pages[i] == p)
            return (int)i;
    return 0;
}
const char *cl_menu_name(int row) {
    static const char *names[] = {"Calculator",
                                  "Three graphs / table",
                                  "CalcAero Laboratory",
                                  "Matrix editor A / B",
                                  "Study mode",
                                  "Tools: equations / integrals",
                                  "Unit converter",
                                  "Programmer",
                                  "History / favorites",
                                  "Settings",
                                  "Help",
                                  "PSP keyboard",
                                  "Undo / redo input",
                                  "Add formula to favorites",
                                  "Export history to TXT",
                                  "Export table / history to CSV",
                                  "Export graph to SVG",
                                  "Screenshot to BMP",
                                  "Export tool result to TXT",
                                  "Save and exit",
                                  "Textbook formula: full screen",
                                  "Language / 28 languages"};
    return row >= 0 && row < CL_MENU_COUNT ? L(names[row]) : "?";
}
const char *cl_graph_expression(const CLApp *a, int series) {
    return series > 0 && series < AERO_CURVES ? a->extra.graph[series - 1] : a->save.expr;
}
const char *cl_lab_example(const CLApp *a) {
    return a->lab_tool == AE_FORMULA ? aero_formula_example(a->lab_variant % AERO_FORMULAS)
                                     : aero_lab_example(a->lab_tool);
}
static void study_next(CLApp *a) {
    aero_study_question(&a->extra, a->study_question, &a->study_answer, a->study_explanation);
    a->input[5][0] = 0;
    a->study_graded = a->study_counted = a->study_right = 0;
    a->cursor = 0;
    a->editing = 0;
    a->undo_ready = 0;
    a->dirty = 1;
    a->notice[0] = 0;
}
static void matrix_edit(CLApp *a) {
    snprintf(a->input[4], CL_EXPR, "%.17g",
             a->extra.matrix[a->matrix_slot][a->matrix_row * 4 + a->matrix_col]);
    a->matrix_editing = 1;
    a->matrix_show = 0;
    a->cursor = (int)strlen(a->input[4]);
    a->editing = 0;
    a->undo_ready = 0;
}
void cl_app_init(CLApp *a) {
    memset(a, 0, sizeof(*a));
    a->request_language = -1;
    cl_context_init(&a->save.ctx);
    a->save.digits = 12;
    a->save.fraction = 1;
    a->xmin = -10;
    a->xmax = 10;
    a->ymin = -5;
    a->ymax = 5;
    a->graph_trace = CL_GRAPH_POINTS / 2;
    a->base_bits = 32;
    strcpy(a->input[0], cl_tool_example(0));
    strcpy(a->input[1], "1");
    strcpy(a->input[2], "0xFF");
    a->undo_cursor = -1;
    aero_defaults(&a->extra);
    strcpy(a->input[3], aero_lab_example(0));
    strcpy(a->input[4], "0");
    aero_study_question(&a->extra, a->study_question, &a->study_answer, a->study_explanation);
    say(a, "Welcome to CalcAero");
}
char *cl_app_editor(CLApp *a) {
    if (a->page == PAGE_LAB)
        return a->input[3];
    if (a->page == PAGE_MATRIX)
        return a->input[4];
    if (a->page == PAGE_STUDY)
        return a->input[5];
    if (a->page == PAGE_GRAPH && a->graph_series > 0)
        return a->extra.graph[a->graph_series - 1];
    return a->page == PAGE_TOOLS   ? a->input[0]
           : a->page == PAGE_UNITS ? a->input[1]
           : a->page == PAGE_BASE  ? a->input[2]
                                   : a->save.expr;
}
static void checkpoint(CLApp *a) {
    snprintf(a->undo, sizeof(a->undo), "%s", cl_app_editor(a));
    a->undo_cursor = a->cursor;
    a->undo_ready = 1;
}
static void changed(CLApp *a) {
    a->has_result = 0;
    a->error.code = 0;
    a->dirty = 1;
    a->notice[0] = 0;
    a->tool_result.count = 0;
    a->unit_ok = a->base_ok = 0;
    if (a->page == PAGE_LAB) {
        a->lab_result.count = 0;
        a->lab_mode = 1;
    }
    if (a->page == PAGE_STUDY)
        a->study_graded = 0;
    if (a->page == PAGE_GRAPH)
        cl_app_graph_reset(a);
}
static void insert(CLApp *a, const char *s) {
    char *text = cl_app_editor(a);
    size_t len = strlen(text), extra = strlen(s);
    if (len + extra >= CL_EXPR) {
        say(a, "Expression limit: 511 characters");
        return;
    }
    if (a->cursor < 0)
        a->cursor = 0;
    if (a->cursor > (int)len)
        a->cursor = (int)len;
    checkpoint(a);
    memmove(text + a->cursor + extra, text + a->cursor, len - (size_t)a->cursor + 1);
    memcpy(text + a->cursor, s, extra);
    a->cursor += (int)extra;
    changed(a);
}
void cl_app_input(CLApp *a, const char *s) {
    if (!s)
        return;
    size_t n = strlen(s);
    if (n >= CL_EXPR) {
        say(a, "Input too long");
        return;
    }
    char staged[CL_EXPR];
    memcpy(staged, s, n + 1);
    checkpoint(a);
    memcpy(cl_app_editor(a), staged, n + 1);
    a->cursor = (int)n;
    a->editing = 0;
    changed(a);
}
static void undo(CLApp *a) {
    if (!a->undo_ready) {
        say(a, "Nothing to undo");
        return;
    }
    char tmp[CL_EXPR];
    int cursor = a->cursor;
    snprintf(tmp, sizeof(tmp), "%s", cl_app_editor(a));
    snprintf(cl_app_editor(a), CL_EXPR, "%s", a->undo);
    a->cursor = a->undo_cursor;
    snprintf(a->undo, sizeof(a->undo), "%s", tmp);
    a->undo_cursor = cursor;
    changed(a);
}
void cl_app_graph_reset(CLApp *a) {
    a->graph_index = 0;
    memset(a->graph_valid, 0, sizeof(a->graph_valid));
    CLError errors[3];
    a->graph_compiled = aero_compile(a->save.expr, &a->extra, &a->graph_program, &errors[0]);
    for (int k = 0; k < 2; ++k) {
        memset(a->other_valid[k], 0, sizeof(a->other_valid[k]));
        errors[k + 1] = (CLError){0, 0};
        a->other_compiled[k] =
            a->extra.graph[k][0] &&
            aero_compile(a->extra.graph[k], &a->extra, &a->other_graph[k], &errors[k + 1]);
    }
    a->error = errors[a->graph_series];
    if (!a->graph_compiled && !a->other_compiled[0] && !a->other_compiled[1])
        a->graph_index = CL_GRAPH_POINTS;
}
void cl_app_evaluate(CLApp *a) {
    a->notice[0] = 0;
    if (a->page == PAGE_LAB) {
        if (a->lab_mode == 0) {
            a->lab_mode = 1;
            snprintf(a->input[3], CL_EXPR, "%s", cl_lab_example(a));
            a->cursor = (int)strlen(a->input[3]);
            return;
        }
        aero_lab_run(a->lab_tool, a->input[3], &a->save.ctx, &a->extra, &a->lab_result);
        a->lab_mode = 2;
        a->lab_scroll = 0;
        if (a->lab_tool == AE_FUNCTION && a->lab_result.ok)
            a->dirty = 1;
        return;
    }
    if (a->page == PAGE_MATRIX) {
        if (a->matrix_editing) {
            CLValue v;
            CLError e;
            if (!aero_calculate(a->input[4], &a->extra, &a->save.ctx, &v, &e) || v.im) {
                say(a, "Cell needs a real value");
                return;
            }
            a->extra.matrix[a->matrix_slot][a->matrix_row * 4 + a->matrix_col] = v.re;
            a->matrix_editing = 0;
            a->dirty = 1;
            say(a, "Cell saved");
        } else {
            aero_matrix_run(&a->extra, a->matrix_op, &a->matrix_result);
            a->matrix_show = 1;
            a->matrix_scroll = 0;
        }
        return;
    }
    if (a->page == PAGE_STUDY) {
        CLValue v;
        CLError e;
        CLContext c = a->save.ctx;
        if (a->extra.study_topic == 2)
            c.angle = CL_DEG;
        if (!aero_calculate(a->input[5], &a->extra, &c, &v, &e) || v.im) {
            say(a, "Enter a real number or expression");
            return;
        }
        if (v.exact && a->study_answer.exact)
            a->study_right = v.num == a->study_answer.num && v.den == a->study_answer.den;
        else
            a->study_right =
                fabs(v.re - a->study_answer.re) <= 1e-10 * fmax(1, fabs(a->study_answer.re));
        if (!a->study_counted && a->extra.study_total < 1000000000) {
            ++a->extra.study_total;
            if (a->study_right)
                ++a->extra.study_correct;
            a->study_counted = 1;
        }
        a->study_graded = 1;
        a->dirty = 1;
        return;
    }
    if (a->page == PAGE_GRAPH) {
        cl_app_graph_reset(a);
        return;
    }
    if (a->page == PAGE_TOOLS) {
        char expanded[CL_EXPR];
        CLError e;
        if (aero_expand(a->input[0], &a->extra, expanded, &e))
            cl_tool_run(a->tool, expanded, &a->save.ctx, &a->tool_result);
        else
            aero_problem(&a->tool_result, cl_error_text(e.code));
        a->tool_scroll = 0;
        return;
    }
    if (a->page == PAGE_UNITS) {
        CLValue v;
        a->unit_ok =
            aero_calculate(a->input[1], &a->extra, &a->save.ctx, &v, &a->error) && !v.im &&
            cl_convert_unit(a->unit_group, a->unit_from, a->unit_to, v.re, &a->unit_result);
        if (!a->unit_ok)
            say(a, "Check value and selected units");
        return;
    }
    if (a->page == PAGE_BASE) {
        a->base_ok = cl_programmer(a->input[2], a->base_bits, a->base_op, &a->base_result,
                                   a->notice, sizeof(a->notice));
        return;
    }
    CLValue result;
    char expanded[CL_EXPR];
    a->has_result = aero_expand(a->save.expr, &a->extra, expanded, &a->error) &&
                    cl_calculate(expanded, &a->save.ctx, &result, &a->error);
    if (!a->has_result)
        return;
    a->result = result;
    CLHistory h;
    memset(&h, 0, sizeof(h));
    snprintf(h.expr, sizeof(h.expr), "%s", expanded);
    h.value = result;
    h.angle = a->save.ctx.angle;
    h.complex_mode = a->save.ctx.complex_mode;
    h.context = a->save.ctx;
    if (!a->save.history_count || strcmp(a->save.history[0].expr, h.expr) ||
        a->save.history[0].value.re != h.value.re || a->save.history[0].value.im != h.value.im ||
        a->save.history[0].angle != h.angle) {
        int count = a->save.history_count;
        if (count >= CL_HISTORY)
            count = CL_HISTORY - 1;
        memmove(a->save.history + 1, a->save.history, (size_t)count * sizeof(h));
        a->save.history[0] = h;
        a->save.history_count = count + 1;
    } else
        a->save.history[0] = h;
    a->save.ctx.ans = result;
    a->dirty = 1;
}
void cl_app_tick(CLApp *a) {
    if (a->page != PAGE_GRAPH || a->graph_index >= CL_GRAPH_POINTS)
        return;
    for (int j = 0; j < 12 && a->graph_index < CL_GRAPH_POINTS; ++j) {
        int i = a->graph_index++;
        CLContext ctx = a->save.ctx;
        ctx.x = cl_real(a->xmin + (a->xmax - a->xmin) * i / (CL_GRAPH_POINTS - 1));
        CLValue v;
        CLError e;
        a->graph_valid[i] =
            a->graph_compiled && cl_eval(&a->graph_program, &ctx, &v, &e) && v.im == 0;
        a->graph_y[i] = a->graph_valid[i] ? v.re : 0;
        for (int k = 0; k < 2; ++k) {
            a->other_valid[k][i] =
                a->other_compiled[k] && cl_eval(&a->other_graph[k], &ctx, &v, &e) && v.im == 0;
            a->other_y[k][i] = a->other_valid[k][i] ? v.re : 0;
        }
    }
}
void cl_app_page(CLApp *a, int target) {
    if (target < 0 || target >= CL_PAGE_COUNT)
        return;
    a->page = target;
    a->menu = 0;
    a->editing = 0;
    a->cursor = (int)strlen(cl_app_editor(a));
    a->view = 0;
    a->undo_ready = 0;
    a->notice[0] = 0;
    if (target == PAGE_LAB) {
        a->lab_mode = 0;
        a->lab_scroll = 0;
    }
    if (target == PAGE_MATRIX) {
        a->matrix_show = 0;
        a->matrix_editing = 0;
    }
    if (target == PAGE_STUDY)
        study_next(a);
    if (target == PAGE_GRAPH) {
        a->keypage = 5;
        cl_app_graph_reset(a);
    } else if (target == PAGE_BASE)
        a->keypage = 4;
}
static void recall(CLApp *a, int idx) {
    if (a->history_bookmarks) {
        if (!a->save.bookmark[idx][0])
            return;
        snprintf(a->save.expr, CL_EXPR, "%s", a->save.bookmark[idx]);
    } else {
        CLHistory *h = &a->save.history[idx];
        memcpy(a->save.expr, h->expr, CL_EXPR);
        a->save.ctx = h->context;
    }
    cl_app_page(a, PAGE_CALC);
    a->has_result = 0;
    a->dirty = 1;
    say(a, "Expression restored; START: calculate");
}
static void setting(CLApp *a, int delta) {
    switch (a->settings_row) {
    case 0:
        a->save.ctx.angle = (CLAngle)((a->save.ctx.angle + delta + 3) % 3);
        break;
    case 1:
        a->save.ctx.complex_mode = !a->save.ctx.complex_mode;
        break;
    case 2:
        a->save.digits += delta;
        if (a->save.digits < 6)
            a->save.digits = 15;
        if (a->save.digits > 15)
            a->save.digits = 6;
        break;
    case 3:
        a->save.style = (a->save.style + delta + 3) % 3;
        break;
    case 4:
        a->save.fraction = !a->save.fraction;
        break;
    case 5:
        a->save.theme = (a->save.theme + delta + 8) % 8;
        break;
    case 6:
        a->extra.pretty = !a->extra.pretty;
        break;
    }
    a->dirty = 1;
}
static void choose(CLApp *a) {
    const CLKey *k = cl_key(a->keypage, a->keysel);
    switch (k->action) {
    case A_CLEAR:
        checkpoint(a);
        cl_app_editor(a)[0] = 0;
        a->cursor = 0;
        changed(a);
        break;
    case A_UNDO:
        undo(a);
        break;
    case A_COMPUTE:
        cl_app_evaluate(a);
        break;
    case A_FRACTION:
        a->save.fraction = !a->save.fraction;
        a->dirty = 1;
        break;
    case A_STORE:
        if (a->has_result && a->page == PAGE_CALC) {
            a->save.ctx.mem[k->insert[0] - 'A'] = a->result;
            a->dirty = 1;
            say(a, "Result stored in memory");
        } else
            say(a, "Calculate a result first");
        break;
    default:
        insert(a, k->insert);
        break;
    }
}
static void graph_nav(CLApp *a, unsigned k) {
    if ((k & (KEY_L | KEY_R)) && a->graph_focus != 1) {
        a->graph_series = (a->graph_series + (k & KEY_R ? 1 : 2)) % AERO_CURVES;
        cl_app_graph_reset(a);
        return;
    }
    if (k & KEY_CROSS) {
        const char *first = cl_graph_expression(a, a->graph_series), *second = NULL;
        for (int offset = 1; offset < AERO_CURVES; ++offset) {
            int series = (a->graph_series + offset) % AERO_CURVES;
            if (cl_graph_expression(a, series)[0]) {
                second = cl_graph_expression(a, series);
                break;
            }
        }
        char text[CL_EXPR];
        int n = second ? snprintf(text, sizeof(text), "%s|%s|%.15g|%.15g", first, second, a->xmin,
                                  a->xmax)
                       : snprintf(text, sizeof(text), "%s|%.15g|%.15g", first, a->xmin, a->xmax);
        if (n < 0 || n >= CL_EXPR) {
            say(a, "Formulas too long for combined analysis");
            return;
        }
        cl_app_page(a, PAGE_LAB);
        a->lab_tool = AE_ANALYZE;
        a->lab_mode = 1;
        cl_app_input(a, text);
        cl_app_evaluate(a);
        return;
    }
    if (k & KEY_START) {
        cl_app_graph_reset(a);
        return;
    }
    if (k & KEY_SELECT) {
        a->graph_focus = (a->graph_focus + 1) % 3;
        return;
    }
    if (k & KEY_SQUARE) {
        a->request_keyboard = 1;
        return;
    }
    if (a->graph_focus != 1) {
        if (k & KEY_LEFT)
            a->graph_trace -= 5;
        if (k & KEY_RIGHT)
            a->graph_trace += 5;
        if (a->graph_trace < 0)
            a->graph_trace = 0;
        if (a->graph_trace >= CL_GRAPH_POINTS)
            a->graph_trace = CL_GRAPH_POINTS - 1;
    } else {
        double wx = a->xmax - a->xmin, wy = a->ymax - a->ymin, dx = 0, dy = 0;
        if (k & KEY_L) {
            wx *= 1.4;
            wy *= 1.4;
        }
        if (k & KEY_R) {
            wx /= 1.4;
            wy /= 1.4;
        }
        if (k & KEY_LEFT)
            dx = -wx * .1;
        if (k & KEY_RIGHT)
            dx = wx * .1;
        if (k & KEY_UP)
            dy = wy * .1;
        if (k & KEY_DOWN)
            dy = -wy * .1;
        double cx = a->xmin / 2 + a->xmax / 2 + dx, cy = a->ymin / 2 + a->ymax / 2 + dy;
        if (wx > 1e-9 && wy > 1e-9 && wx < 1e12 && wy < 1e12) {
            a->xmin = cx - wx / 2;
            a->xmax = cx + wx / 2;
            a->ymin = cy - wy / 2;
            a->ymax = cy + wy / 2;
            cl_app_graph_reset(a);
        }
    }
}
void cl_app_key(CLApp *a, unsigned k) {
    if (a->menu) {
        if (k & KEY_SELECT) {
            cl_app_page(a, PAGE_LANGUAGE);
            a->language_row = ca_language();
            return;
        }
        if (k & KEY_TRIANGLE || k & KEY_CIRCLE) {
            a->menu = 0;
            return;
        }
        if (k & KEY_UP)
            a->menu_row = (a->menu_row + CL_MENU_COUNT - 1) % CL_MENU_COUNT;
        if (k & KEY_DOWN)
            a->menu_row = (a->menu_row + 1) % CL_MENU_COUNT;
        if (k & KEY_CROSS) {
            int r = a->menu_row;
            if (r < 11)
                cl_app_page(a, menu_pages[r]);
            else if (r == 11) {
                a->menu = 0;
                if (a->page == PAGE_LAB && a->lab_mode == 0) {
                    a->lab_mode = 1;
                    snprintf(a->input[3], CL_EXPR, "%s", cl_lab_example(a));
                    a->cursor = (int)strlen(a->input[3]);
                }
                if (a->page == PAGE_MATRIX && !a->matrix_editing)
                    matrix_edit(a);
                if (a->page == PAGE_SETTINGS || a->page == PAGE_HISTORY || a->page == PAGE_HELP)
                    cl_app_page(a, PAGE_CALC);
                a->request_keyboard = 1;
            } else if (r == 12) {
                a->menu = 0;
                undo(a);
            } else if (r == 13) {
                if (a->page != PAGE_CALC && a->page != PAGE_GRAPH) {
                    a->menu = 0;
                    say(a, "Favorites: calculator or graph formulas");
                    return;
                }
                int slot = 0;
                while (slot < CL_BOOKMARKS && a->save.bookmark[slot][0])
                    ++slot;
                if (slot == CL_BOOKMARKS) {
                    say(a, "Favorites full: remove an item in history");
                    a->menu = 0;
                } else {
                    snprintf(a->save.bookmark[slot], CL_EXPR, "%s", cl_app_editor(a));
                    a->dirty = 1;
                    a->menu = 0;
                    say(a, "Formula added to favorites");
                }
            } else if (r == 14) {
                a->menu = 0;
                a->request_export = 1;
            } else if (r == 15) {
                a->menu = 0;
                a->request_csv = 1;
            } else if (r == 16) {
                a->menu = 0;
                if (a->page == PAGE_GRAPH)
                    a->request_svg = 1;
                else
                    say(a, "Open a graph to export SVG");
            } else if (r == 17) {
                a->menu = 0;
                a->request_capture = 1;
            } else if (r == 18) {
                a->menu = 0;
                a->request_report = 1;
            } else if (r == 20)
                cl_app_page(a, PAGE_FORMULA);
            else if (r == 21) {
                cl_app_page(a, PAGE_LANGUAGE);
                a->language_row = ca_language();
            } else
                a->request_exit = 1;
        }
        return;
    }
    if (k & KEY_TRIANGLE) {
        a->menu = 1;
        a->menu_row = cl_menu_index(a->page);
        return;
    }
    if (a->page == PAGE_LANGUAGE) {
        if (k & KEY_UP)
            a->language_row = (a->language_row + LANG_COUNT - 1) % LANG_COUNT;
        if (k & KEY_DOWN)
            a->language_row = (a->language_row + 1) % LANG_COUNT;
        if (k & (KEY_LEFT | KEY_RIGHT)) {
            int row = a->language_row % CA_LANGUAGE_ROWS;
            int target = a->language_row < CA_LANGUAGE_ROWS ? row + CA_LANGUAGE_ROWS : row;
            a->language_row = target < LANG_COUNT ? target : LANG_COUNT - 1;
        }
        if (k & (KEY_CROSS | KEY_START))
            a->request_language = a->language_row;
        if (k & KEY_CIRCLE)
            cl_app_page(a, PAGE_CALC);
        return;
    }
    if (a->page == PAGE_SETTINGS) {
        if (k & KEY_UP)
            a->settings_row = (a->settings_row + 6) % 7;
        if (k & KEY_DOWN)
            a->settings_row = (a->settings_row + 1) % 7;
        if (k & KEY_LEFT)
            setting(a, -1);
        if (k & (KEY_RIGHT | KEY_CROSS))
            setting(a, 1);
        return;
    }
    if (a->page == PAGE_HELP) {
        if (k & KEY_L)
            a->help_page = (a->help_page + CL_HELP_PAGES - 1) % CL_HELP_PAGES;
        if (k & KEY_R)
            a->help_page = (a->help_page + 1) % CL_HELP_PAGES;
        return;
    }
    if (a->page == PAGE_HISTORY) {
        int count = a->history_bookmarks ? CL_BOOKMARKS : a->save.history_count;
        if (k & KEY_SELECT) {
            a->history_bookmarks = !a->history_bookmarks;
            a->history_row = 0;
            return;
        }
        if (count) {
            if (k & KEY_UP)
                a->history_row = (a->history_row + count - 1) % count;
            if (k & KEY_DOWN)
                a->history_row = (a->history_row + 1) % count;
            if (k & KEY_CROSS)
                recall(a, a->history_row);
            if (k & KEY_SQUARE) {
                if (a->history_bookmarks) {
                    a->save.bookmark[a->history_row][0] = 0;
                    a->dirty = 1;
                    say(a, "Favorite removed");
                } else {
                    int slot = 0;
                    while (slot < CL_BOOKMARKS && a->save.bookmark[slot][0])
                        ++slot;
                    if (slot < CL_BOOKMARKS) {
                        memcpy(a->save.bookmark[slot], a->save.history[a->history_row].expr,
                               CL_EXPR);
                        a->dirty = 1;
                        say(a, "Formula added to favorites");
                    } else
                        say(a, "Favorites are full");
                }
            }
        }
        return;
    }
    if (a->page == PAGE_GRAPH) {
        graph_nav(a, k);
        return;
    }
    if (a->page == PAGE_FORMULA) {
        if (k & KEY_SQUARE)
            a->request_keyboard = 1;
        if (k & KEY_UP && a->view > 0)
            --a->view;
        if (k & KEY_DOWN && a->view < 9)
            ++a->view;
        if (k & KEY_START)
            cl_app_page(a, PAGE_CALC);
        return;
    }
    if (a->page == PAGE_LAB) {
        if (a->lab_mode == 0) {
            if (k & KEY_UP) {
                a->lab_tool = (a->lab_tool + AERO_LABS - 1) % AERO_LABS;
                a->lab_variant = 0;
            }
            if (k & KEY_DOWN) {
                a->lab_tool = (a->lab_tool + 1) % AERO_LABS;
                a->lab_variant = 0;
            }
            if (k & (KEY_L | KEY_R)) {
                int d = k & KEY_R ? 1 : -1;
                if (a->lab_tool == AE_FORMULA)
                    a->lab_variant = (a->lab_variant + d + AERO_FORMULAS) % AERO_FORMULAS;
                else {
                    a->lab_tool = (a->lab_tool + d * 7 + AERO_LABS) % AERO_LABS;
                    a->lab_variant = 0;
                }
            }
            if (k & (KEY_CROSS | KEY_START)) {
                a->lab_mode = 1;
                cl_app_input(a, cl_lab_example(a));
            }
            return;
        }
        if (k & KEY_SELECT) {
            a->lab_mode = 0;
            a->editing = 0;
            return;
        }
        if (a->lab_mode == 2) {
            if (k & KEY_UP && a->lab_scroll > 0)
                --a->lab_scroll;
            if (k & KEY_DOWN && a->lab_scroll < a->lab_result.count - 10)
                ++a->lab_scroll;
            if (k & (KEY_CROSS | KEY_CIRCLE)) {
                a->lab_mode = 1;
                a->lab_result.count = 0;
            }
            if (k & KEY_START)
                cl_app_evaluate(a);
            return;
        }
    }
    if (a->page == PAGE_MATRIX) {
        if (a->matrix_show) {
            if (k & KEY_UP && a->matrix_scroll > 0)
                --a->matrix_scroll;
            if (k & KEY_DOWN && a->matrix_scroll < a->matrix_result.count - 10)
                ++a->matrix_scroll;
            if (k & (KEY_CROSS | KEY_CIRCLE))
                a->matrix_show = 0;
            return;
        }
        if (!a->matrix_editing) {
            int n = a->extra.matrix_n;
            if (k & KEY_UP)
                a->matrix_row = (a->matrix_row + n - 1) % n;
            if (k & KEY_DOWN)
                a->matrix_row = (a->matrix_row + 1) % n;
            if (k & KEY_LEFT)
                a->matrix_col = (a->matrix_col + n - 1) % n;
            if (k & KEY_RIGHT)
                a->matrix_col = (a->matrix_col + 1) % n;
            if (k & KEY_SELECT)
                a->matrix_slot = 1 - a->matrix_slot;
            if (k & KEY_SQUARE) {
                a->extra.matrix_n = n % 4 + 1;
                a->matrix_row = a->matrix_col = 0;
                a->dirty = 1;
            }
            if (k & KEY_L)
                a->matrix_op = (a->matrix_op + AERO_MATRIX_OPS - 1) % AERO_MATRIX_OPS;
            if (k & KEY_R)
                a->matrix_op = (a->matrix_op + 1) % AERO_MATRIX_OPS;
            if (k & KEY_CROSS)
                matrix_edit(a);
            if (k & KEY_START)
                cl_app_evaluate(a);
            return;
        }
        if (k & KEY_SELECT) {
            a->matrix_editing = 0;
            a->editing = 0;
            return;
        }
    }
    if (a->page == PAGE_STUDY) {
        if (k & KEY_SELECT) {
            a->extra.study_topic = (a->extra.study_topic + 1) % AERO_STUDY_TOPICS;
            study_next(a);
            return;
        }
        if (a->study_graded) {
            if (k & (KEY_CROSS | KEY_SQUARE)) {
                study_next(a);
                return;
            }
            if (k & KEY_CIRCLE) {
                a->study_graded = 0;
                return;
            }
            return;
        }
    }
    if (k & KEY_SELECT) {
        a->save.ctx.angle = (CLAngle)((a->save.ctx.angle + 1) % 3);
        a->dirty = 1;
        a->has_result = 0;
        say(a, "Angle mode changed");
        return;
    }
    if (a->page == PAGE_TOOLS && (k & KEY_SQUARE)) {
        a->tool = (a->tool + 1) % CL_TOOL_COUNT;
        strcpy(a->input[0], cl_tool_example(a->tool));
        a->cursor = (int)strlen(a->input[0]);
        a->tool_result.count = 0;
        a->undo_ready = 0;
        return;
    }
    if (a->page == PAGE_UNITS && (k & KEY_SQUARE)) {
        a->unit_row = (a->unit_row + 1) % 3;
        return;
    }
    if (a->page == PAGE_BASE && (k & KEY_SQUARE)) {
        a->base_row = (a->base_row + 1) % 2;
        return;
    }
    if ((a->page == PAGE_UNITS || a->page == PAGE_BASE) && (k & (KEY_L | KEY_R))) {
        int d = k & KEY_R ? 1 : -1;
        if (a->page == PAGE_UNITS) {
            if (a->unit_row == 0) {
                a->unit_group = (a->unit_group + d + cl_unit_groups()) % cl_unit_groups();
                a->unit_from = a->unit_to = 0;
            } else {
                int *target = a->unit_row == 1 ? &a->unit_from : &a->unit_to;
                *target =
                    (*target + d + cl_unit_count(a->unit_group)) % cl_unit_count(a->unit_group);
            }
            a->unit_ok = 0;
        } else {
            if (a->base_row == 0) {
                int index = a->base_bits == 8    ? 0
                            : a->base_bits == 16 ? 1
                            : a->base_bits == 32 ? 2
                                                 : 3;
                static const int widths[] = {8, 16, 32, 64};
                a->base_bits = widths[(index + d + 4) % 4];
            } else
                a->base_op = (a->base_op + d + 12) % 12;
            a->base_ok = 0;
        }
        return;
    }
    if (k & KEY_START) {
        cl_app_evaluate(a);
        return;
    }
    if (a->page == PAGE_TOOLS && a->tool_result.count) {
        if (k & KEY_UP && a->tool_scroll > 0)
            --a->tool_scroll;
        if (k & KEY_DOWN && a->tool_scroll < a->tool_result.count - 8)
            ++a->tool_scroll;
        if (k & (KEY_CROSS | KEY_CIRCLE))
            a->tool_result.count = 0;
        return;
    }
    if (((a->page == PAGE_BASE && a->base_ok) || (a->page == PAGE_UNITS && a->unit_ok)) &&
        (k & KEY_CROSS)) {
        a->base_ok = a->unit_ok = 0;
        return;
    }
    if (k & KEY_CIRCLE) {
        char *s = cl_app_editor(a);
        if (a->cursor > 0) {
            checkpoint(a);
            memmove(s + a->cursor - 1, s + a->cursor, strlen(s + a->cursor) + 1);
            --a->cursor;
            changed(a);
        }
        return;
    }
    if (k & KEY_SQUARE) {
        a->editing = !a->editing;
        return;
    }
    if (a->editing) {
        int len = (int)strlen(cl_app_editor(a));
        if (k & KEY_LEFT && a->cursor > 0)
            --a->cursor;
        if (k & KEY_RIGHT && a->cursor < len)
            ++a->cursor;
        if (k & KEY_UP)
            a->cursor = 0;
        if (k & KEY_DOWN)
            a->cursor = len;
        if (k & KEY_CROSS)
            a->editing = 0;
    } else {
        if (k & KEY_UP)
            a->keysel = (a->keysel + 24) % 30;
        if (k & KEY_DOWN)
            a->keysel = (a->keysel + 6) % 30;
        if (k & KEY_LEFT)
            a->keysel = (a->keysel / 6) * 6 + (a->keysel % 6 + 5) % 6;
        if (k & KEY_RIGHT)
            a->keysel = (a->keysel / 6) * 6 + (a->keysel % 6 + 1) % 6;
        if (k & KEY_CROSS)
            choose(a);
    }
    if (k & KEY_L)
        a->keypage = (a->keypage + 5) % 6;
    if (k & KEY_R)
        a->keypage = (a->keypage + 1) % 6;
}
