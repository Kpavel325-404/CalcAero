#include "calc.h"
#include <complex.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "l10n.h"
typedef struct {
    const char *name;
    int code, min, max;
} Function;
static const Function funcs[] = {
    {"sin", F_SIN, 1, 1},     {"cos", F_COS, 1, 1},     {"tan", F_TAN, 1, 1},
    {"asin", F_ASIN, 1, 1},   {"acos", F_ACOS, 1, 1},   {"atan", F_ATAN, 1, 1},
    {"atan2", F_ATAN2, 2, 2}, {"sinh", F_SINH, 1, 1},   {"cosh", F_COSH, 1, 1},
    {"tanh", F_TANH, 1, 1},   {"asinh", F_ASINH, 1, 1}, {"acosh", F_ACOSH, 1, 1},
    {"atanh", F_ATANH, 1, 1}, {"sqrt", F_SQRT, 1, 1},   {"cbrt", F_CBRT, 1, 1},
    {"root", F_ROOT, 2, 2},   {"ln", F_LN, 1, 1},       {"log", F_LOG, 1, 2},
    {"log2", F_LOG2, 1, 1},   {"exp", F_EXP, 1, 1},     {"abs", F_ABS, 1, 1},
    {"floor", F_FLOOR, 1, 1}, {"ceil", F_CEIL, 1, 1},   {"round", F_ROUND, 1, 1},
    {"trunc", F_TRUNC, 1, 1}, {"sign", F_SIGN, 1, 1},   {"frac", F_FRAC, 1, 1},
    {"inv", F_INV, 1, 1},     {"fact", F_FACT, 1, 1},   {"ncr", F_NCR, 2, 2},
    {"npr", F_NPR, 2, 2},     {"gcd", F_GCD, 2, 2},     {"lcm", F_LCM, 2, 2},
    {"mod", F_MOD, 2, 2},     {"min", F_MIN, 2, 2},     {"max", F_MAX, 2, 2},
    {"clamp", F_CLAMP, 3, 3}, {"re", F_RE, 1, 1},       {"im", F_IM, 1, 1},
    {"conj", F_CONJ, 1, 1},   {"arg", F_ARG, 1, 1},     {"polar", F_POLAR, 2, 2},
    {"d2r", F_D2R, 1, 1},     {"r2d", F_R2D, 1, 1},     {"d2g", F_D2G, 1, 1},
    {"g2d", F_G2D, 1, 1},     {"dms", F_DMS, 3, 3},     {"pow", F_POW, 2, 2},
    {"hypot", F_HYPOT, 2, 2}};
const char *cl_function_name(int code) {
    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); ++i)
        if (funcs[i].code == code) return funcs[i].name;
    return "?";
}
static uint64_t magnitude(int64_t x) { return x < 0 ? (uint64_t)(-(x + 1)) + 1 : (uint64_t)x; }
static uint64_t gcd64(uint64_t a, uint64_t b) {
    while (b) {
        uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}
static int mul64(int64_t a, int64_t b, int64_t *out) { return !__builtin_mul_overflow(a, b, out); }
static int add64(int64_t a, int64_t b, int64_t *out) { return !__builtin_add_overflow(a, b, out); }
CLValue cl_real(double x) { return (CLValue){x, 0, 0, 1, 0}; }
CLValue cl_integer(int64_t x) { return (CLValue){(double)x, 0, x, 1, 1}; }
CLValue cl_fraction(int64_t n, int64_t d) {
    CLValue r = cl_real(d ? (double)n / (double)d : NAN);
    if (!d || d == INT64_MIN || (d < 0 && n == INT64_MIN))
        return r;
    if (d < 0) {
        n = -n;
        d = -d;
    }
    uint64_t g = gcd64(magnitude(n), (uint64_t)d);
    if (g) {
        n /= (int64_t)g;
        d /= (int64_t)g;
    }
    r.num = n;
    r.den = d;
    r.exact = 1;
    r.re = (double)n / (double)d;
    return r;
}
int cl_value_valid(CLValue v) {
    return isfinite(v.re) && isfinite(v.im) &&
           (v.exact == 0 ||
            (v.exact == 1 && v.im == 0 && v.den > 0 && v.re == (double)v.num / (double)v.den));
}
void cl_context_init(CLContext *c) {
    memset(c, 0, sizeof(*c));
    c->angle = CL_DEG;
    c->ans = c->x = cl_integer(0);
    for (int i = 0; i < 6; ++i)
        c->mem[i] = cl_integer(0);
}
double cl_to_rad(double x, CLAngle a) {
    return a == CL_DEG ? x * (CL_PI / 180) : a == CL_GRAD ? x * (CL_PI / 200) : x;
}
double cl_from_rad(double x, CLAngle a) {
    return a == CL_DEG ? x * (180 / CL_PI) : a == CL_GRAD ? x * (200 / CL_PI) : x;
}
static CLValue from_z(double complex z) {
    CLValue r = cl_real(creal(z));
    r.im = cimag(z);
    return r;
}
static double complex to_z(CLValue v) { return __builtin_complex(v.re, v.im == 0 ? 0.0 : v.im); }
static CLValue negate(CLValue a) {
    a.re = -a.re;
    a.im = -a.im;
    if (a.exact) {
        if (a.num == INT64_MIN)
            a.exact = 0;
        else
            a.num = -a.num;
    }
    return a;
}
static CLValue plus(CLValue a, CLValue b, int sub) {
    if (sub)
        b = negate(b);
    if (a.exact && b.exact) {
        int64_t x, y, n, d;
        int64_t g = (int64_t)gcd64((uint64_t)a.den, (uint64_t)b.den);
        if (mul64(a.num, b.den / g, &x) && mul64(b.num, a.den / g, &y) && add64(x, y, &n) &&
            mul64(a.den, b.den / g, &d))
            return cl_fraction(n, d);
    }
    CLValue r = cl_real(a.re + b.re);
    r.im = a.im + b.im;
    return r;
}
static CLValue times(CLValue a, CLValue b, int div) {
    if (a.exact && b.exact && (!div || b.num)) {
        int64_t bn = div ? b.den : b.num, bd = div ? b.num : b.den;
        uint64_t g1 = gcd64(magnitude(a.num), magnitude(bd)),
                 g2 = gcd64(magnitude(bn), (uint64_t)a.den);
        int64_t n, d;
        if (mul64(a.num / (int64_t)g1, bn / (int64_t)g2, &n) &&
            mul64(a.den / (int64_t)g2, bd / (int64_t)g1, &d))
            return cl_fraction(n, d);
    }
    if (a.im == 0 && b.im == 0)
        return cl_real(div ? a.re / b.re : a.re * b.re);
    return from_z(div ? to_z(a) / to_z(b) : to_z(a) * to_z(b));
}
typedef struct {
    const char *s;
    int pos, depth;
    CLProgram *p;
    CLError *e;
} Parser;
static void perr(Parser *p, int code) {
    if (!p->e->code) {
        p->e->code = code;
        p->e->pos = p->pos;
    }
}
static void space(Parser *p) {
    while (isspace((unsigned char)p->s[p->pos]))
        ++p->pos;
}
static int node(Parser *p, int kind, int op, int a, int b, int c, CLValue value, int pos) {
    if (p->p->count >= CL_NODES) {
        perr(p, CL_LIMIT);
        return -1;
    }
    int n = p->p->count++;
    p->p->nodes[n] = (CLNode){kind, op, a, b, c, pos, value};
    return n;
}
static int expr(Parser *p);
static int unary(Parser *p);
static int primary(Parser *p) {
    space(p);
    int start = p->pos;
    unsigned char ch = (unsigned char)p->s[start];
    if (ch == '(') {
        ++p->pos;
        int a = expr(p);
        space(p);
        if (p->s[p->pos] != ')') {
            perr(p, CL_SYNTAX);
            return -1;
        }
        ++p->pos;
        return a;
    }
    if (isdigit(ch) || ch == '.' || ch == ',') {
        char number[96];
        int len = 0, seen = 0;
        while (isdigit((unsigned char)p->s[p->pos])) {
            if (len < 94)
                number[len++] = p->s[p->pos];
            else
                perr(p, CL_LIMIT);
            ++p->pos;
            seen = 1;
        }
        if (p->s[p->pos] == '.' || p->s[p->pos] == ',') {
            if (len < 94)
                number[len++] = '.';
            else
                perr(p, CL_LIMIT);
            ++p->pos;
            while (isdigit((unsigned char)p->s[p->pos])) {
                if (len < 94)
                    number[len++] = p->s[p->pos];
                else
                    perr(p, CL_LIMIT);
                ++p->pos;
                seen = 1;
            }
        }
        if (!seen) {
            perr(p, CL_SYNTAX);
            return -1;
        }
        if ((p->s[p->pos] == 'e' || p->s[p->pos] == 'E') &&
            (isdigit((unsigned char)p->s[p->pos + 1]) || p->s[p->pos + 1] == '+' ||
             p->s[p->pos + 1] == '-')) {
            if (len < 94)
                number[len++] = 'e';
            else
                perr(p, CL_LIMIT);
            ++p->pos;
            if (p->s[p->pos] == '+' || p->s[p->pos] == '-') {
                if (len < 94)
                    number[len++] = p->s[p->pos];
                else
                    perr(p, CL_LIMIT);
                ++p->pos;
            }
            int expstart = p->pos;
            while (isdigit((unsigned char)p->s[p->pos])) {
                if (len < 94)
                    number[len++] = p->s[p->pos];
                else
                    perr(p, CL_LIMIT);
                ++p->pos;
            }
            if (p->pos == expstart)
                perr(p, CL_SYNTAX);
        }
        number[len] = 0;
        errno = 0;
        double d = strtod(number, 0);
        CLValue v = cl_real(d);
        if (!isfinite(d)) {
            perr(p, CL_OVERFLOW);
            return -1;
        }
        if (errno == ERANGE && d == 0) {
            perr(p, CL_PRECISION);
            return -1;
        }
        /* Recover finite decimal literals as rational numbers, up to int64. */
        int64_t n = 0, den = 1;
        int decimals = 0, fraction = 0, exponent = 0, sign = 1, valid = 1;
        for (int i = 0; number[i] && number[i] != 'e'; ++i) {
            if (number[i] == '.') {
                fraction = 1;
                continue;
            }
            int64_t t;
            if (!mul64(n, 10, &t) || !add64(t, number[i] - '0', &n)) {
                valid = 0;
                break;
            }
            if (fraction)
                ++decimals;
        }
        const char *e = strchr(number, 'e');
        if (e) {
            ++e;
            if (*e == '-' || *e == '+') {
                sign = *e == '-' ? -1 : 1;
                ++e;
            }
            for (int i = 0; e[i]; ++i) {
                exponent = exponent * 10 + (e[i] - '0');
                if (exponent > 1000) {
                    valid = 0;
                    break;
                }
            }
        }
        int power = sign * exponent - decimals;
        if (abs(power) > 18)
            valid = 0;
        if (valid) {
            for (int i = 0; i < abs(power); ++i) {
                if (power >= 0) {
                    if (!mul64(n, 10, &n)) {
                        valid = 0;
                        break;
                    }
                } else if (!mul64(den, 10, &den)) {
                    valid = 0;
                    break;
                }
            }
        }
        if (valid)
            v = cl_fraction(n, den);
        return node(p, N_VALUE, 0, -1, -1, -1, v, start);
    }
    if (isalpha(ch) || ch == '_') {
        char name[32];
        int l = 0;
        while (isalnum((unsigned char)p->s[p->pos]) || p->s[p->pos] == '_') {
            if (l < 31)
                name[l++] = (char)tolower((unsigned char)p->s[p->pos]);
            else
                perr(p, CL_LIMIT);
            ++p->pos;
        }
        name[l] = 0;
        space(p);
        if (l == 1 && p->s[start] >= 'A' && p->s[start] <= 'F')
            return node(p, N_VAR, p->s[start] - 'A' + 1, -1, -1, -1, cl_real(0), start);
        if (!strcmp(name, "pi"))
            return node(p, N_VALUE, 0, -1, -1, -1, cl_real(CL_PI), start);
        if (!strcmp(name, "e"))
            return node(p, N_VALUE, 0, -1, -1, -1, cl_real(exp(1)), start);
        if (!strcmp(name, "i")) {
            CLValue v = cl_integer(0);
            v.im = 1;
            v.exact = 0;
            return node(p, N_VALUE, 0, -1, -1, -1, v, start);
        }
        if (!strcmp(name, "ans"))
            return node(p, N_VAR, 0, -1, -1, -1, cl_real(0), start);
        if (!strcmp(name, "x"))
            return node(p, N_VAR, 7, -1, -1, -1, cl_real(0), start);
        if (l == 1 && name[0] >= 'a' && name[0] <= 'f')
            return node(p, N_VAR, name[0] - 'a' + 1, -1, -1, -1, cl_real(0), start);
        struct Constant {
            const char *name;
            double v;
        };
        static const struct Constant constants[] = {
            {"c0", 299792458},    {"hplanck", 6.62607015e-34}, {"qe", 1.602176634e-19},
            {"kb", 1.380649e-23}, {"na", 6.02214076e23},       {"g0", 9.80665}};
        for (size_t k = 0; k < sizeof(constants) / sizeof(constants[0]); ++k)
            if (!strcmp(name, constants[k].name))
                return node(p, N_VALUE, 0, -1, -1, -1, cl_real(constants[k].v), start);
        const Function *fn = 0;
        for (size_t k = 0; k < sizeof(funcs) / sizeof(funcs[0]); ++k)
            if (!strcmp(name, funcs[k].name)) {
                fn = &funcs[k];
                break;
            }
        if (!fn) {
            perr(p, CL_UNKNOWN);
            p->e->pos = start;
            return -1;
        }
        if (p->s[p->pos] != '(') {
            perr(p, CL_SYNTAX);
            return -1;
        }
        ++p->pos;
        int args[3] = {-1, -1, -1}, n = 0;
        space(p);
        if (p->s[p->pos] != ')')
            while (!p->e->code) {
                if (n >= 3) {
                    perr(p, CL_ARGUMENTS);
                    break;
                }
                args[n++] = expr(p);
                space(p);
                if (p->s[p->pos] != ';')
                    break;
                ++p->pos;
            }
        if (p->s[p->pos] != ')') {
            perr(p, CL_SYNTAX);
            return -1;
        }
        ++p->pos;
        if (n < fn->min || n > fn->max) {
            perr(p, CL_ARGUMENTS);
            return -1;
        }
        return node(p, N_FUNC, fn->code, args[0], args[1], args[2], cl_real(0), start);
    }
    if (ch == 0xcf && (unsigned char)p->s[start + 1] == 0x80) {
        p->pos += 2;
        return node(p, N_VALUE, 0, -1, -1, -1, cl_real(CL_PI), start);
    }
    perr(p, CL_SYNTAX);
    return -1;
}
static int postfix(Parser *p) {
    int a = primary(p);
    space(p);
    while (!p->e->code && (p->s[p->pos] == '!' || p->s[p->pos] == '%')) {
        int start = p->pos;
        int k = p->s[p->pos++] == '!' ? N_FACT : N_PERCENT;
        a = node(p, k, 0, a, -1, -1, cl_real(0), start);
        space(p);
    }
    return a;
}
static int power(Parser *p) {
    int a = postfix(p);
    space(p);
    if (!p->e->code && p->s[p->pos] == '^') {
        int start = p->pos++;
        int b = unary(p);
        a = node(p, N_POW, 0, a, b, -1, cl_real(0), start);
    }
    return a;
}
static int unary(Parser *p) {
    if (++p->depth > 40) {
        perr(p, CL_LIMIT);
        --p->depth;
        return -1;
    }
    space(p);
    int start = p->pos, a;
    if (p->s[p->pos] == '+' || p->s[p->pos] == '-') {
        int neg = p->s[p->pos++] == '-';
        a = unary(p);
        if (neg)
            a = node(p, N_NEG, 0, a, -1, -1, cl_real(0), start);
    } else
        a = power(p);
    --p->depth;
    return a;
}
static int implicit(Parser *p) {
    unsigned char c = (unsigned char)p->s[p->pos];
    return c == '(' || isalpha(c) || c == '_' || c == 0xcf;
}
static int term(Parser *p) {
    int a = unary(p);
    space(p);
    while (!p->e->code && (p->s[p->pos] == '*' || p->s[p->pos] == '/' || implicit(p))) {
        int start = p->pos, k = N_MUL;
        if (p->s[p->pos] == '/')
            k = N_DIV;
        if (p->s[p->pos] == '*' || p->s[p->pos] == '/')
            ++p->pos;
        int b = unary(p);
        a = node(p, k, 0, a, b, -1, cl_real(0), start);
        space(p);
    }
    return a;
}
static int expr(Parser *p) {
    int a = term(p);
    space(p);
    while (!p->e->code && (p->s[p->pos] == '+' || p->s[p->pos] == '-')) {
        int start = p->pos, k = p->s[p->pos++] == '+' ? N_ADD : N_SUB;
        int b = term(p);
        a = node(p, k, 0, a, b, -1, cl_real(0), start);
        space(p);
    }
    return a;
}
int cl_compile(const char *s, CLProgram *p, CLError *e) {
    memset(p, 0, sizeof(*p));
    *e = (CLError){0, 0};
    if (!s || strlen(s) >= CL_EXPR) {
        e->code = CL_LIMIT;
        return 0;
    }
    Parser parser = {s, 0, 0, p, e};
    p->root = expr(&parser);
    space(&parser);
    if (!e->code && s[parser.pos])
        perr(&parser, CL_SYNTAX);
    return !e->code && p->root >= 0;
}
typedef struct {
    const CLProgram *p;
    const CLContext *ctx;
    CLError *e;
    int steps, limit;
} Eval;
static void fail(Eval *e, int code, int pos) {
    if (!e->e->code) {
        e->e->code = code;
        e->e->pos = pos;
    }
}
static int real_arg(Eval *e, CLValue v, int pos) {
    if (v.im) {
        fail(e, CL_DOMAIN, pos);
        return 0;
    }
    return 1;
}
static int integer_arg(Eval *e, CLValue v, int64_t *out, int pos) {
    if (!real_arg(e, v, pos))
        return 0;
    if (v.exact && v.den == 1) {
        *out = v.num;
        return 1;
    }
    if (v.re < -0x1p63 || v.re >= 0x1p63 || trunc(v.re) != v.re) {
        fail(e, CL_DOMAIN, pos);
        return 0;
    }
    *out = (int64_t)v.re;
    return 1;
}
static uint64_t isqrt64(uint64_t x) {
    uint64_t r = (uint64_t)sqrt((double)x);
    while (r && r > x / r)
        --r;
    while (r < UINT64_MAX && r + 1 <= x / (r + 1))
        ++r;
    return r;
}
static CLValue factorial(Eval *e, CLValue v, int pos) {
    int64_t n;
    if (!integer_arg(e, v, &n, pos))
        return cl_real(0);
    if (n < 0 || n > 170) {
        fail(e, n < 0 ? CL_DOMAIN : CL_OVERFLOW, pos);
        return cl_real(0);
    }
    CLValue r = cl_integer(1);
    for (int64_t i = 2; i <= n; ++i) {
        if (++e->steps > e->limit) {
            fail(e, CL_LIMIT, pos);
            break;
        }
        r = times(r, cl_integer(i), 0);
    }
    return r;
}
static CLValue exponentiate(Eval *e, CLValue a, CLValue b, int pos) {
    if (a.re == 0 && a.im == 0 && (b.im || b.re <= 0)) {
        fail(e, b.re < 0 ? CL_ZERO : CL_DOMAIN, pos);
        return cl_real(0);
    }
    if (a.exact && b.exact && b.den == 1 && b.num >= -63 && b.num <= 63) {
        int64_t n = b.num;
        CLValue r = cl_integer(1), base = a;
        uint64_t k = magnitude(n);
        while (k) {
            if (k & 1)
                r = times(r, base, 0);
            k >>= 1;
            if (k)
                base = times(base, base, 0);
        }
        return n < 0 ? times(cl_integer(1), r, 1) : r;
    }
    if (a.im == 0 && b.im == 0) {
        if (a.re >= 0 || trunc(b.re) == b.re)
            return cl_real(pow(a.re, b.re));
        if (!e->ctx->complex_mode && b.exact && (b.den & 1)) {
            double r = pow(-a.re, b.re);
            return cl_real((magnitude(b.num) & 1) ? -r : r);
        }
    }
    if (!e->ctx->complex_mode) {
        fail(e, CL_DOMAIN, pos);
        return cl_real(0);
    }
    return from_z(cpow(to_z(a), to_z(b)));
}
static CLValue trig(Eval *e, int fn, CLValue v, int pos) {
    if (fabs(v.re) > 1e12 || fabs(v.im) > 1e6) {
        fail(e, CL_PRECISION, pos);
        return cl_real(0);
    }
    double period = e->ctx->angle == CL_DEG ? 360 : e->ctx->angle == CL_GRAD ? 400 : 2 * CL_PI;
    double reduced = remainder(v.re, period), quarter = period / 4, delta;
    int quadrant;
    if (v.im == 0 && e->ctx->angle != CL_RAD && v.exact) {
        int64_t pd, qd;
        if (mul64(v.den, (int64_t)period, &pd) && mul64(v.den, (int64_t)quarter, &qd)) {
            int64_t rn = v.num % pd, q = rn / qd, rem = rn % qd;
            if (magnitude(rem) > (uint64_t)qd / 2) {
                int sign = rem < 0 ? -1 : 1;
                q += sign;
                rem -= sign * qd;
            }
            quadrant = (int)q;
            delta = (double)rem / (double)v.den;
        } else {
            quadrant = (int)nearbyint(reduced / quarter);
            delta = reduced - quadrant * quarter;
        }
    } else {
        quadrant = (int)nearbyint(reduced / quarter);
        delta = reduced - quadrant * quarter;
    }
    int q = (quadrant % 4 + 4) % 4;
    if (v.im == 0 && delta == 0) {
        if (fn == F_TAN && (q & 1)) {
            fail(e, CL_DOMAIN, pos);
            return cl_real(0);
        }
        if (fn == F_SIN)
            return cl_integer(q == 1 ? 1 : q == 3 ? -1 : 0);
        if (fn == F_COS)
            return cl_integer(q == 0 ? 1 : q == 2 ? -1 : 0);
        return cl_integer(0);
    }
    if (v.im == 0 && e->ctx->angle == CL_RAD) {
        return cl_real(fn == F_SIN ? sin(v.re) : fn == F_COS ? cos(v.re) : tan(v.re));
    }
    if (v.im == 0) {
        double d = cl_to_rad(delta, e->ctx->angle), sn = sin(d), cs = cos(d);
        double sv = q == 0 ? sn : q == 1 ? cs : q == 2 ? -sn : -cs;
        double cv = q == 0 ? cs : q == 1 ? -sn : q == 2 ? -cs : sn;
        return cl_real(fn == F_SIN ? sv : fn == F_COS ? cv : sv / cv);
    }
    double re = e->ctx->angle == CL_RAD ? v.re : cl_to_rad(reduced, e->ctx->angle);
    double complex z = re + I * cl_to_rad(v.im, e->ctx->angle);
    return from_z(fn == F_SIN ? csin(z) : fn == F_COS ? ccos(z) : ctan(z));
}
static CLValue function(Eval *e, int f, CLValue a, CLValue b, CLValue c, int has_b, int pos) {
    double x = a.re, y = b.re;
    double complex z = to_z(a);
    CLValue r = cl_real(0);
    int64_t n, k;
    switch (f) {
    case F_SIN:
    case F_COS:
    case F_TAN:
        return trig(e, f, a, pos);
    case F_ASIN:
    case F_ACOS:
    case F_ATAN:
        if (a.im == 0 && (f == F_ATAN || fabs(x) <= 1))
            return cl_real(cl_from_rad(f == F_ASIN   ? asin(x)
                                       : f == F_ACOS ? acos(x)
                                                     : atan(x),
                                       e->ctx->angle));
        if (!e->ctx->complex_mode) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        r = from_z(f == F_ASIN ? casin(z) : f == F_ACOS ? cacos(z) : catan(z));
        r.re = cl_from_rad(r.re, e->ctx->angle);
        r.im = cl_from_rad(r.im, e->ctx->angle);
        return r;
    case F_ATAN2:
        if (!real_arg(e, a, pos) || !real_arg(e, b, pos))
            return r;
        if (x == 0 && y == 0) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        return cl_real(cl_from_rad(atan2(x, y), e->ctx->angle));
    case F_SINH:
        return a.im == 0 ? cl_real(sinh(x)) : from_z(csinh(z));
    case F_COSH:
        return a.im == 0 ? cl_real(cosh(x)) : from_z(ccosh(z));
    case F_TANH:
        return a.im == 0 ? cl_real(tanh(x)) : from_z(ctanh(z));
    case F_ASINH:
        return a.im == 0 ? cl_real(asinh(x)) : from_z(casinh(z));
    case F_ACOSH:
        if (a.im == 0 && x >= 1)
            return cl_real(acosh(x));
        if (!e->ctx->complex_mode) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        return from_z(cacosh(z));
    case F_ATANH:
        if (a.im == 0 && fabs(x) < 1)
            return cl_real(atanh(x));
        if (a.im == 0 && fabs(x) == 1) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        if (!e->ctx->complex_mode) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        return from_z(catanh(z));
    case F_SQRT:
        if (a.exact && a.num >= 0) {
            uint64_t nr = isqrt64((uint64_t)a.num), dr = isqrt64((uint64_t)a.den);
            if (nr * nr == (uint64_t)a.num && dr * dr == (uint64_t)a.den)
                return cl_fraction((int64_t)nr, (int64_t)dr);
        }
        if (a.im == 0 && x >= 0)
            return cl_real(sqrt(x));
        if (!e->ctx->complex_mode) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        return from_z(csqrt(z));
    case F_CBRT:
        if (!real_arg(e, a, pos))
            return r;
        return cl_real(cbrt(x));
    case F_ROOT:
        if (!integer_arg(e, b, &n, pos) || !real_arg(e, a, pos))
            return r;
        if (n == 0 || magnitude(n) > 1000000) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        if (x == 0 && n < 0) {
            fail(e, CL_ZERO, pos);
            return r;
        }
        if (x < 0 && (n & 1))
            return cl_real(-pow(-x, 1.0 / (double)n));
        return exponentiate(e, a, cl_fraction(1, n), pos);
    case F_LN:
    case F_LOG:
    case F_LOG2:
        if (a.re == 0 && a.im == 0) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        if (a.im == 0 && x > 0)
            r = cl_real(log(x));
        else {
            if (!e->ctx->complex_mode) {
                fail(e, CL_DOMAIN, pos);
                return r;
            }
            r = from_z(clog(z));
        }
        if (f == F_LOG && has_b) {
            if (!real_arg(e, b, pos) || y <= 0 || y == 1) {
                fail(e, CL_DOMAIN, pos);
                return cl_real(0);
            }
            return times(r, cl_real(log(y)), 1);
        }
        return f == F_LN ? r : times(r, cl_real(f == F_LOG ? log(10) : log(2)), 1);
    case F_EXP:
        return a.im == 0 ? cl_real(exp(x)) : from_z(cexp(z));
    case F_ABS:
        if (a.im)
            return cl_real(hypot(x, a.im));
        return x < 0 ? negate(a) : a;
    case F_RE:
        return a.im ? cl_real(x) : a;
    case F_IM:
        return cl_real(a.im);
    case F_CONJ:
        a.im = -a.im;
        return a;
    case F_ARG:
        if (x == 0 && a.im == 0) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        return cl_real(cl_from_rad(atan2(a.im, x), e->ctx->angle));
    case F_POLAR:
        if (!real_arg(e, a, pos) || !real_arg(e, b, pos) || x < 0) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        r = trig(e, F_COS, b, pos);
        r.re *= x;
        r.im = trig(e, F_SIN, b, pos).re * x;
        r.exact = 0;
        if (r.im && !e->ctx->complex_mode)
            fail(e, CL_COMPLEX, pos);
        return r;
    case F_INV:
        if (x == 0 && a.im == 0) {
            fail(e, CL_ZERO, pos);
            return r;
        }
        return times(cl_integer(1), a, 1);
    case F_FACT:
        return factorial(e, a, pos);
    case F_POW:
        return exponentiate(e, a, b, pos);
    case F_NCR:
    case F_NPR:
        if (!integer_arg(e, a, &n, pos) || !integer_arg(e, b, &k, pos))
            return r;
        if (n < 0 || k < 0 || k > n || n > 1000) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        if (f == F_NCR && k > n - k)
            k = n - k;
        r = cl_integer(1);
        for (int64_t j = 1; j <= k; ++j) {
            if (++e->steps > e->limit) {
                fail(e, CL_LIMIT, pos);
                break;
            }
            CLValue factor = f == F_NCR ? cl_fraction(n - k + j, j) : cl_integer(n - j + 1);
            r = times(r, factor, 0);
            if (!isfinite(r.re))
                break;
        }
        return r;
    case F_GCD:
    case F_LCM:
        if (!integer_arg(e, a, &n, pos) || !integer_arg(e, b, &k, pos))
            return r;
        {
            uint64_t g = gcd64(magnitude(n), magnitude(k));
            if (g > INT64_MAX) {
                fail(e, CL_OVERFLOW, pos);
                return r;
            }
            if (f == F_GCD)
                return cl_integer((int64_t)g);
            if (!n || !k)
                return cl_integer(0);
            CLValue t = times(cl_integer(n / (int64_t)g), cl_integer(k), 0);
            return t.re < 0 ? negate(t) : t;
        }
    case F_MOD:
        if (!real_arg(e, a, pos) || !real_arg(e, b, pos))
            return r;
        if (y == 0) {
            fail(e, CL_ZERO, pos);
            return r;
        }
        if (a.exact && b.exact && a.den == 1 && b.den == 1) {
            if (a.num == INT64_MIN && b.num == -1)
                return cl_integer(0);
            return cl_integer(a.num % b.num);
        }
        return cl_real(fmod(x, y));
    case F_MIN:
    case F_MAX:
    case F_CLAMP:
        if (!real_arg(e, a, pos) || !real_arg(e, b, pos) || (f == F_CLAMP && !real_arg(e, c, pos)))
            return r;
        if (f == F_MIN)
            return x < y ? a : b;
        if (f == F_MAX)
            return x > y ? a : b;
        if (y > c.re) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        return x < y ? b : x > c.re ? c : a;
    case F_FLOOR:
    case F_CEIL:
    case F_ROUND:
    case F_TRUNC:
    case F_SIGN:
    case F_FRAC:
        if (!real_arg(e, a, pos))
            return r;
        if (f == F_SIGN)
            return cl_integer((x > 0) - (x < 0));
        if (a.exact) {
            n = a.num / a.den;
            k = a.num % a.den;
            if (f == F_FLOOR && k < 0)
                --n;
            if (f == F_CEIL && k > 0)
                ++n;
            if (f == F_ROUND && magnitude(k) >= (uint64_t)a.den / 2 + (uint64_t)a.den % 2)
                n += a.num < 0 ? -1 : 1;
            return f == F_FRAC ? cl_fraction(k, a.den) : cl_integer(n);
        }
        if (f == F_FRAC)
            return cl_real(x - trunc(x));
        return cl_real(f == F_FLOOR   ? floor(x)
                       : f == F_CEIL  ? ceil(x)
                       : f == F_ROUND ? round(x)
                                      : trunc(x));
    case F_D2R:
    case F_R2D:
    case F_D2G:
    case F_G2D:
        if (!real_arg(e, a, pos))
            return r;
        return f == F_D2R   ? cl_real(x * (CL_PI / 180))
               : f == F_R2D ? cl_real(x * (180 / CL_PI))
                            : times(a, cl_fraction(f == F_D2G ? 10 : 9, f == F_D2G ? 9 : 10), 0);
    case F_DMS:
        if (!real_arg(e, a, pos) || !real_arg(e, b, pos) || !real_arg(e, c, pos) || y < 0 ||
            y >= 60 || c.re < 0 || c.re >= 60) {
            fail(e, CL_DOMAIN, pos);
            return r;
        }
        r = plus(times(b, cl_integer(60), 1), times(c, cl_integer(3600), 1), 0);
        return plus(a, r, x < 0);
    case F_HYPOT:
        if (!real_arg(e, a, pos) || !real_arg(e, b, pos))
            return r;
        return cl_real(hypot(x, y));
    default:
        fail(e, CL_UNKNOWN, pos);
        return r;
    }
}
static CLValue evalnode(Eval *e, int idx, int depth) {
    CLValue r = cl_real(0);
    if (e->e->code)
        return r;
    if (idx < 0 || idx >= e->p->count || depth > CL_NODES || ++e->steps > e->limit) {
        fail(e, CL_LIMIT, 0);
        return r;
    }
    const CLNode *n = &e->p->nodes[idx];
    CLValue a = r, b = r, c = r;
    if (n->kind == N_VALUE)
        r = n->value;
    else if (n->kind == N_VAR)
        r = n->op == 0 ? e->ctx->ans : n->op == 7 ? e->ctx->x : e->ctx->mem[n->op - 1];
    else {
        a = evalnode(e, n->a, depth + 1);
        if (n->b >= 0)
            b = evalnode(e, n->b, depth + 1);
        if (n->c >= 0)
            c = evalnode(e, n->c, depth + 1);
        if (e->e->code)
            return r;
        errno = 0;
        switch (n->kind) {
        case N_NEG:
            r = negate(a);
            break;
        case N_ADD:
            r = plus(a, b, 0);
            break;
        case N_SUB:
            r = plus(a, b, 1);
            break;
        case N_MUL:
            r = times(a, b, 0);
            break;
        case N_DIV:
            if (b.re == 0 && b.im == 0) {
                fail(e, CL_ZERO, n->pos);
                break;
            }
            r = times(a, b, 1);
            break;
        case N_POW:
            r = exponentiate(e, a, b, n->pos);
            break;
        case N_FACT:
            r = factorial(e, a, n->pos);
            break;
        case N_PERCENT:
            r = times(a, cl_integer(100), 1);
            break;
        case N_FUNC:
            r = function(e, n->op, a, b, c, n->b >= 0, n->pos);
            break;
        default:
            fail(e, CL_SYNTAX, n->pos);
            break;
        }
        if (r.re == 0 && r.im == 0 && a.im == 0 && b.im == 0 && a.re != 0 &&
            (((n->kind == N_MUL || n->kind == N_DIV) && b.re != 0) || n->kind == N_POW ||
             (n->kind == N_FUNC && (n->op == F_EXP || n->op == F_POW))))
            fail(e, CL_PRECISION, n->pos);
    }
    if (!isfinite(r.re) || !isfinite(r.im))
        fail(e, CL_OVERFLOW, n->pos);
    if (!e->ctx->complex_mode && r.im)
        fail(e, CL_COMPLEX, n->pos);
    return r;
}
int cl_eval_budget(const CLProgram *p, const CLContext *ctx, CLValue *out, CLError *err,
                   int *budget) {
    *err = (CLError){0, 0};
    if (ctx->angle < CL_DEG || ctx->angle > CL_GRAD) {
        err->code = CL_DOMAIN;
        return 0;
    }
    Eval e = {p, ctx, err, 0, *budget < 12000 ? *budget : 12000};
    CLValue r = evalnode(&e, p->root, 0);
    *budget -= e.steps;
    if (err->code)
        return 0;
    *out = r;
    return 1;
}
int cl_eval(const CLProgram *p, const CLContext *ctx, CLValue *out, CLError *err) {
    int budget = 12000;
    return cl_eval_budget(p, ctx, out, err, &budget);
}
int cl_calculate(const char *s, const CLContext *ctx, CLValue *out, CLError *err) {
    CLProgram p;
    if (!cl_compile(s, &p, err))
        return 0;
    return cl_eval(&p, ctx, out, err);
}
static void decimal(double x, int digits, int style, char *s, size_t cap) {
    if (x == 0)
        x = 0; /* remove a signed zero only, never small nonzero values */
    if (style == 1)
        snprintf(s, cap, "%.*e", digits - 1, x);
    else if (style == 2 && x != 0) {
        int exp3 = (int)floor(log10(fabs(x)) / 3) * 3;
        double scale = pow(10, exp3);
        if (scale == 0 || !isfinite(scale))
            snprintf(s, cap, "%.*g", digits, x);
        else
            snprintf(s, cap, "%.*ge%+d", digits, x / scale, exp3);
    } else
        snprintf(s, cap, "%.*g", digits, x);
}
void cl_format(CLValue v, int digits, int style, int fraction, char *s, size_t cap) {
    if (!cap)
        return;
    if (digits < 6)
        digits = 6;
    if (digits > 15)
        digits = 15;
    if (fraction && v.exact) {
        if (v.den == 1)
            snprintf(s, cap, "%lld", (long long)v.num);
        else
            snprintf(s, cap, "%lld/%lld", (long long)v.num, (long long)v.den);
        return;
    }
    char re[64], im[64];
    decimal(v.re, digits, style, re, sizeof(re));
    if (v.im) {
        decimal(fabs(v.im), digits, style, im, sizeof(im));
        snprintf(s, cap, "%s %c %si", re, v.im < 0 ? '-' : '+', im);
    } else
        snprintf(s, cap, "%s", re);
}
const char *cl_error_text(int c) {
    static const char *s[] = {"Done",
                              "Check expression and parentheses",
                              "Unknown function or name",
                              "Wrong number of arguments",
                              "Argument outside function domain",
                              "Division by zero",
                              "Number too large",
                              "Input/calculation limit reached",
                              "Insufficient numerical precision",
                              "Enable complex mode"};
    return c >= 0 && c < (int)(sizeof(s) / sizeof(s[0])) ? L(s[c]) : L("Error");
}
