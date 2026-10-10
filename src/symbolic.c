#include "aero.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "l10n.h"
static int text(char out[CL_EXPR], const char *a, const char *op, const char *b) {
    int n = snprintf(out, CL_EXPR, "(%s%s%s)", a, op, b);
    return n >= 0 && n < CL_EXPR;
}
static int literal(CLValue v, char out[CL_EXPR]) {
    if (v.im)
        return 0;
    if (v.exact && v.num == INT64_MIN) {
        snprintf(out, CL_EXPR, "((-9223372036854775807-1)/%lld)", (long long)v.den);
    } else if (v.exact && v.den != 1)
        snprintf(out, CL_EXPR, "(%lld/%lld)", (long long)v.num, (long long)v.den);
    else if (v.exact)
        snprintf(out, CL_EXPR, "%lld", (long long)v.num);
    else if (v.re == CL_PI)
        snprintf(out, CL_EXPR, "pi");
    else if (v.re == exp(1))
        snprintf(out, CL_EXPR, "e");
    else
        snprintf(out, CL_EXPR, "%.17g", v.re);
    return 1;
}
static int node_text(const CLProgram *p, int i, char out[CL_EXPR], int depth) {
    if (i < 0 || i >= p->count || depth > 32)
        return 0;
    const CLNode *n = &p->nodes[i];
    char a[CL_EXPR], b[CL_EXPR], c[CL_EXPR];
    if (n->kind == N_VALUE)
        return literal(n->value, out);
    if (n->kind == N_VAR) {
        snprintf(out, CL_EXPR, n->op == 7 ? "x" : n->op == 0 ? "Ans" : "%c", 'A' + n->op - 1);
        return 1;
    }
    if (!node_text(p, n->a, a, depth + 1))
        return 0;
    if (n->b >= 0 && !node_text(p, n->b, b, depth + 1))
        return 0;
    if (n->c >= 0 && !node_text(p, n->c, c, depth + 1))
        return 0;
    int count;
    if (n->kind == N_NEG)
        count = snprintf(out, CL_EXPR, "(-%s)", a);
    else if (n->kind == N_FACT)
        count = snprintf(out, CL_EXPR, "(%s)!", a);
    else if (n->kind == N_PERCENT)
        count = snprintf(out, CL_EXPR, "(%s)%%", a);
    else if (n->kind == N_FUNC) {
        if (n->c >= 0)
            count = snprintf(out, CL_EXPR, "%s(%s;%s;%s)", cl_function_name(n->op), a, b, c);
        else if (n->b >= 0)
            count = snprintf(out, CL_EXPR, "%s(%s;%s)", cl_function_name(n->op), a, b);
        else
            count = snprintf(out, CL_EXPR, "%s(%s)", cl_function_name(n->op), a);
    } else {
        const char *ops[] = {"+", "-", "*", "/", "^"};
        if (n->kind < N_ADD || n->kind > N_POW)
            return 0;
        return text(out, a, ops[n->kind - N_ADD], b);
    }
    return count >= 0 && count < CL_EXPR;
}
int aero_node_text(const CLProgram *p, int i, char out[CL_EXPR]) { return node_text(p, i, out, 0); }
static int op_value(int kind, CLValue a, CLValue b, CLValue *out) {
    CLProgram p;
    CLContext c;
    CLError e;
    cl_context_init(&c);
    p.count = 3;
    p.root = 2;
    p.nodes[0] = (CLNode){N_VALUE, 0, -1, -1, -1, 0, a};
    p.nodes[1] = (CLNode){N_VALUE, 0, -1, -1, -1, 0, b};
    p.nodes[2] = (CLNode){kind, 0, 0, 1, -1, 0, cl_integer(0)};
    return cl_eval(&p, &c, out, &e) && out->exact && !out->im;
}
#define POLY_DEGREE 8
typedef struct {
    CLValue coefficient[POLY_DEGREE + 1];
    int degree;
} Polynomial;
static void poly_zero(Polynomial *p) {
    p->degree = 0;
    for (int i = 0; i <= POLY_DEGREE; ++i)
        p->coefficient[i] = cl_integer(0);
}
static void poly_trim(Polynomial *p) {
    while (p->degree && p->coefficient[p->degree].num == 0)
        --p->degree;
}
static int poly_mul(const Polynomial *a, const Polynomial *b, Polynomial *out) {
    if (a->degree + b->degree > POLY_DEGREE)
        return 0;
    poly_zero(out);
    out->degree = a->degree + b->degree;
    for (int i = 0; i <= a->degree; ++i)
        for (int j = 0; j <= b->degree; ++j) {
            CLValue v;
            if (!op_value(N_MUL, a->coefficient[i], b->coefficient[j], &v) ||
                !op_value(N_ADD, out->coefficient[i + j], v, &out->coefficient[i + j]))
                return 0;
        }
    poly_trim(out);
    return 1;
}
static int polynomial(const CLProgram *p, int i, const CLContext *ctx, Polynomial *out, int depth) {
    if (depth > 32 || i < 0 || i >= p->count)
        return 0;
    const CLNode *n = &p->nodes[i];
    poly_zero(out);
    if (n->kind == N_VALUE) {
        if (!n->value.exact || n->value.im)
            return 0;
        out->coefficient[0] = n->value;
        return 1;
    }
    if (n->kind == N_VAR) {
        if (n->op == 7) {
            out->coefficient[1] = cl_integer(1);
            out->degree = 1;
        } else {
            CLValue v = n->op == 0 ? ctx->ans : ctx->mem[n->op - 1];
            if (!v.exact || v.im)
                return 0;
            out->coefficient[0] = v;
        }
        return 1;
    }
    Polynomial a, b;
    if (!polynomial(p, n->a, ctx, &a, depth + 1))
        return 0;
    if (n->kind == N_NEG || n->kind == N_PERCENT) {
        *out = a;
        for (int j = 0; j <= a.degree; ++j)
            if (!op_value(n->kind == N_NEG ? N_MUL : N_DIV, a.coefficient[j],
                          cl_integer(n->kind == N_NEG ? -1 : 100), &out->coefficient[j]))
                return 0;
        return 1;
    }
    if (n->b < 0 || !polynomial(p, n->b, ctx, &b, depth + 1))
        return 0;
    if (n->kind == N_ADD || n->kind == N_SUB) {
        out->degree = a.degree > b.degree ? a.degree : b.degree;
        for (int j = 0; j <= out->degree; ++j)
            if (!op_value(n->kind, a.coefficient[j], b.coefficient[j], &out->coefficient[j]))
                return 0;
        poly_trim(out);
        return 1;
    }
    if (n->kind == N_MUL)
        return poly_mul(&a, &b, out);
    if (n->kind == N_DIV && b.degree == 0 && b.coefficient[0].num) {
        *out = a;
        for (int j = 0; j <= a.degree; ++j)
            if (!op_value(N_DIV, a.coefficient[j], b.coefficient[0], &out->coefficient[j]))
                return 0;
        return 1;
    }
    if ((n->kind == N_POW || (n->kind == N_FUNC && n->op == F_POW)) && b.degree == 0 &&
        b.coefficient[0].den == 1) {
        int64_t exponent = b.coefficient[0].num;
        if (exponent < 0 || exponent > POLY_DEGREE || a.degree * exponent > POLY_DEGREE)
            return 0;
        /* Do not erase the undefined 0^0 domain of a variable base. */
        if (!exponent && (a.degree || !a.coefficient[0].num))
            return 0;
        out->coefficient[0] = cl_integer(1);
        for (int64_t j = 0; j < exponent; ++j) {
            Polynomial product;
            if (!poly_mul(out, &a, &product))
                return 0;
            *out = product;
        }
        return 1;
    }
    return 0;
}
static int poly_text(const Polynomial *p, char out[CL_EXPR]) {
    size_t at = 0;
    out[0] = 0;
    for (int i = p->degree; i >= 0; --i) {
        CLValue v = p->coefficient[i];
        if (!v.num)
            continue;
        char term[CL_EXPR], value[CL_EXPR];
        literal(v, value);
        int n;
        if (i == 0)
            n = snprintf(term, sizeof(term), "%s", value);
        else if (v.num == v.den)
            n = snprintf(term, sizeof(term), i == 1 ? "x" : "x^%d", i);
        else if (v.num == -v.den)
            n = snprintf(term, sizeof(term), i == 1 ? "(-x)" : "(-x^%d)", i);
        else
            n = snprintf(term, sizeof(term), i == 1 ? "(%s)*x" : "(%s)*x^%d", value, i);
        if (n < 0 || (size_t)n >= sizeof(term) || at + (size_t)n + (at ? 1 : 0) >= CL_EXPR)
            return 0;
        if (at)
            out[at++] = '+';
        memcpy(out + at, term, (size_t)n + 1);
        at += (size_t)n;
    }
    if (!at)
        strcpy(out, "0");
    return 1;
}
static int contains_x(const CLProgram *p, int i, int depth) {
    if (i < 0)
        return 0;
    if (depth > 64)
        return 1;
    const CLNode *n = &p->nodes[i];
    if (n->kind == N_VAR)
        return n->op == 7;
    return contains_x(p, n->a, depth + 1) || contains_x(p, n->b, depth + 1) ||
           contains_x(p, n->c, depth + 1);
}
static int combine(char out[CL_EXPR], const char *a, const char *op, const char *b) {
    if (!strcmp(op, "+")) {
        if (!strcmp(a, "0")) {
            strcpy(out, b);
            return 1;
        }
        if (!strcmp(b, "0")) {
            strcpy(out, a);
            return 1;
        }
    }
    if (!strcmp(op, "-") && !strcmp(b, "0")) {
        strcpy(out, a);
        return 1;
    }
    if (!strcmp(op, "*")) {
        if (!strcmp(a, "0") || !strcmp(b, "0")) {
            strcpy(out, "0");
            return 1;
        }
        if (!strcmp(a, "1")) {
            strcpy(out, b);
            return 1;
        }
        if (!strcmp(b, "1")) {
            strcpy(out, a);
            return 1;
        }
    }
    if (!strcmp(op, "/")) {
        if (!strcmp(a, "0")) {
            strcpy(out, "0");
            return 1;
        }
        if (!strcmp(b, "1")) {
            strcpy(out, a);
            return 1;
        }
    }
    return text(out, a, op, b);
}
static int call(char out[CL_EXPR], const char *name, const char *a) {
    int n = snprintf(out, CL_EXPR, "%s(%s)", name, a);
    return n >= 0 && n < CL_EXPR;
}
static int eval_at(const CLProgram *p, int i, const CLContext *ctx, CLValue *v, CLError *e) {
    CLProgram q = *p;
    q.root = i;
    return cl_eval(&q, ctx, v, e);
}
static int derivative(const CLProgram *p, int i, const CLContext *ctx, char out[CL_EXPR],
                      int depth) {
    if (depth > 24 || i < 0 || i >= p->count)
        return 0;
    const CLNode *n = &p->nodes[i];
    char a[CL_EXPR], b[CL_EXPR], da[CL_EXPR], db[CL_EXPR], u[CL_EXPR], v[CL_EXPR], w[CL_EXPR];
    if (!contains_x(p, i, 0)) {
        strcpy(out, "0");
        return 1;
    }
    if (n->kind == N_VAR) {
        strcpy(out, "1");
        return 1;
    }
    if (!aero_node_text(p, n->a, a) || !derivative(p, n->a, ctx, da, depth + 1))
        return 0;
    if (n->b >= 0 && (!aero_node_text(p, n->b, b) || !derivative(p, n->b, ctx, db, depth + 1)))
        return 0;
    if (n->kind == N_NEG)
        return combine(out, "-1", "*", da);
    if (n->kind == N_PERCENT)
        return combine(out, da, "/", "100");
    if (n->kind == N_ADD || n->kind == N_SUB)
        return combine(out, da, n->kind == N_ADD ? "+" : "-", db);
    if (n->kind == N_MUL)
        return combine(u, da, "*", b) && combine(v, a, "*", db) && combine(out, u, "+", v);
    if (n->kind == N_DIV)
        return combine(u, da, "*", b) && combine(v, a, "*", db) && combine(w, u, "-", v) &&
               text(v, b, "^", "2") && combine(out, w, "/", v);
    if (n->kind == N_POW || (n->kind == N_FUNC && n->op == F_POW)) {
        if (contains_x(p, n->b, 0))
            return 0;
        CLValue exponent;
        CLError e;
        if (!eval_at(p, n->b, ctx, &exponent, &e) || exponent.im)
            return 0;
        if (exponent.re == 0) {
            strcpy(out, "0");
            return 1;
        }
        if (exponent.re == 1) {
            strcpy(out, da);
            return 1;
        }
        if (!combine(u, b, "-", "1") || !text(v, a, "^", u) || !combine(w, b, "*", v))
            return 0;
        return combine(out, w, "*", da);
    }
    if (n->kind != N_FUNC)
        return 0;
    char factor[CL_EXPR];
    double angular = ctx->angle == CL_DEG ? CL_PI / 180 : ctx->angle == CL_GRAD ? CL_PI / 200 : 1;
    snprintf(factor, sizeof(factor), "%.17g", angular);
    switch (n->op) {
    case F_SIN:
        return call(u, "cos", a) && combine(v, u, "*", da) && combine(out, v, "*", factor);
    case F_COS:
        return call(u, "sin", a) && combine(v, "-1", "*", u) && combine(w, v, "*", da) &&
               combine(out, w, "*", factor);
    case F_TAN:
        return call(u, "cos", a) && text(v, u, "^", "2") && combine(w, da, "*", factor) &&
               combine(out, w, "/", v);
    case F_LN:
        return combine(out, da, "/", a);
    case F_EXP:
        return call(u, "exp", a) && combine(out, u, "*", da);
    case F_SQRT:
        return call(u, "sqrt", a) && combine(v, "2", "*", u) && combine(out, da, "/", v);
    case F_CBRT:
        return call(u, "cbrt", a) && text(v, u, "^", "2") && combine(w, "3", "*", v) &&
               combine(out, da, "/", w);
    case F_LOG:
    case F_LOG2:
        if (n->b >= 0 && contains_x(p, n->b, 0))
            return 0;
        if (n->b < 0)
            strcpy(b, n->op == F_LOG2 ? "2" : "10");
        return call(u, "ln", b) && combine(v, a, "*", u) && combine(out, da, "/", v);
    case F_ASIN:
    case F_ACOS:
    case F_ATAN:
        if (n->op == F_ATAN) {
            if (!text(u, a, "^", "2") || !combine(v, "1", "+", u))
                return 0;
        } else if (!text(u, a, "^", "2") || !combine(v, "1", "-", u) || !call(w, "sqrt", v))
            return 0;
        if (n->op != F_ATAN)
            strcpy(v, w);
        snprintf(factor, sizeof(factor), "%.17g", 1 / angular);
        if (!combine(u, da, "*", factor) || !combine(w, u, "/", v))
            return 0;
        if (n->op == F_ACOS)
            return combine(out, "-1", "*", w);
        strcpy(out, w);
        return 1;
    case F_SINH:
        return call(u, "cosh", a) && combine(out, u, "*", da);
    case F_COSH:
        return call(u, "sinh", a) && combine(out, u, "*", da);
    case F_TANH:
        return call(u, "cosh", a) && text(v, u, "^", "2") && combine(out, da, "/", v);
    case F_ABS:
        return call(u, "sign", a) && combine(out, u, "*", da);
    case F_INV:
        return text(u, a, "^", "2") && combine(v, "-1", "*", da) && combine(out, v, "/", u);
    case F_D2R:
        return combine(out, da, "*", "(pi/180)");
    case F_R2D:
        return combine(out, da, "*", "(180/pi)");
    case F_D2G:
        return combine(out, da, "*", "(10/9)");
    case F_G2D:
        return combine(out, da, "*", "(9/10)");
    default:
        return 0;
    }
}
int aero_symbolic(const char *input, const CLContext *ctx, int diff, char out[CL_EXPR], char *error,
                  size_t cap) {
    CLProgram p;
    CLError e;
    if (!cl_compile(input, &p, &e)) {
        snprintf(error, cap, "%s", cl_error_text(e.code));
        return 0;
    }
    Polynomial poly;
    if (polynomial(&p, p.root, ctx, &poly, 0)) {
        if (diff) {
            for (int i = 0; i < poly.degree; ++i)
                if (!op_value(N_MUL, poly.coefficient[i + 1], cl_integer(i + 1),
                              &poly.coefficient[i]))
                    goto unsupported;
            if (poly.degree) {
                poly.coefficient[poly.degree] = cl_integer(0);
                --poly.degree;
            } else
                poly.coefficient[0] = cl_integer(0);
            poly_trim(&poly);
        }
        if (poly_text(&poly, out)) {
            error[0] = 0;
            return 1;
        }
    } else if (diff && derivative(&p, p.root, ctx, out, 0)) {
        CLProgram result;
        if (cl_compile(out, &result, &e)) {
            error[0] = 0;
            return 1;
        }
    }
unsupported:
    snprintf(error, cap,
             diff ? "Formula exceeds supported rules or length limit"
                  : "Requires degree <=8 polynomial with exact int64 coefficients");
    return 0;
}
int aero_steps(const char *input, const CLContext *ctx, CLToolResult *r) {
    CLProgram p;
    CLError e;
    CLValue result;
    if (!cl_compile(input, &p, &e) || !cl_eval(&p, ctx, &result, &e))
        return aero_problem(r, cl_error_text(e.code));
    int operations = 0;
    for (int i = 0; i < p.count; ++i)
        if (p.nodes[i].kind > N_VAR)
            ++operations;
    aero_line(r, "Evaluation order: parentheses -> operations");
    for (int i = 0, number = 1; i < p.count; ++i)
        if (p.nodes[i].kind > N_VAR) {
            if (r->count >= CL_TOOL_LINES - 3) {
                aero_line(r, "First steps of %d operations shown", operations);
                break;
            }
            char expression[CL_EXPR], answer[160];
            p.root = i;
            CLValue v;
            if (!cl_eval(&p, ctx, &v, &e))
                return aero_problem(r, cl_error_text(e.code));
            if (!aero_node_text(&p, i, expression))
                snprintf(expression, sizeof(expression), "operation at position %d",
                         p.nodes[i].pos + 1);
            cl_format(v, 15, 0, 1, answer, sizeof(answer));
            aero_line(r, "%d. %.85s %s %.55s", number++, expression, v.exact ? "=" : "~=", answer);
        }
    char answer[160];
    cl_format(result, 15, 0, 1, answer, sizeof(answer));
    aero_line(r, "Result %s %s", result.exact ? "=" : "~=", answer);
    r->value = result.re;
    r->ok = 1;
    r->approx = !result.exact;
    return 1;
}
