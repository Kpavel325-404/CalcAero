#include "aero.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "l10n.h"
typedef struct {
    double v;
    int d[8];
} Quantity;
typedef struct {
    const char *s;
    int at, depth, steps;
    const char *error;
} QParser;
typedef struct {
    const char *name;
    double scale;
    int d[8];
} Unit;
static const Unit units[] = {{"m", 1, {1}},
                             {"km", 1000, {1}},
                             {"cm", .01, {1}},
                             {"mm", .001, {1}},
                             {"um", 1e-6, {1}},
                             {"nm", 1e-9, {1}},
                             {"in", .0254, {1}},
                             {"ft", .3048, {1}},
                             {"kg", 1, {0, 1}},
                             {"g", .001, {0, 1}},
                             {"mg", 1e-6, {0, 1}},
                             {"s", 1, {0, 0, 1}},
                             {"ms", .001, {0, 0, 1}},
                             {"min", 60, {0, 0, 1}},
                             {"h", 3600, {0, 0, 1}},
                             {"day", 86400, {0, 0, 1}},
                             {"A", 1, {0, 0, 0, 1}},
                             {"K", 1, {0, 0, 0, 0, 1}},
                             {"mol", 1, {0, 0, 0, 0, 0, 1}},
                             {"cd", 1, {0, 0, 0, 0, 0, 0, 1}},
                             {"B", 1, {0, 0, 0, 0, 0, 0, 0, 1}},
                             {"kB", 1000, {0, 0, 0, 0, 0, 0, 0, 1}},
                             {"MB", 1e6, {0, 0, 0, 0, 0, 0, 0, 1}},
                             {"KiB", 1024, {0, 0, 0, 0, 0, 0, 0, 1}},
                             {"MiB", 1048576, {0, 0, 0, 0, 0, 0, 0, 1}},
                             {"Hz", 1, {0, 0, -1}},
                             {"N", 1, {1, 1, -2}},
                             {"J", 1, {2, 1, -2}},
                             {"W", 1, {2, 1, -3}},
                             {"Pa", 1, {-1, 1, -2}},
                             {"kPa", 1000, {-1, 1, -2}},
                             {"C", 1, {0, 0, 1, 1}},
                             {"V", 1, {2, 1, -3, -1}},
                             {"Ohm", 1, {2, 1, -3, -2}},
                             {"F", 1, {-2, -1, 4, 2}},
                             {"H", 1, {2, 1, -2, -2}},
                             {"L", .001, {3}},
                             {"mL", 1e-6, {3}},
                             {"rad", 1, {0}},
                             {"deg", CL_PI / 180, {0}},
                             {"percent", .01, {0}}};
static void space(QParser *p) {
    while (isspace((unsigned char)p->s[p->at]))
        ++p->at;
}
static void fail(QParser *p, const char *e) {
    if (!p->error)
        p->error = e;
}
static Quantity expression(QParser *p);
static Quantity atom(QParser *p) {
    Quantity q = {0, {0}};
    space(p);
    if (++p->steps > 256 || ++p->depth > 32) {
        fail(p, "Unit complexity limit");
        --p->depth;
        return q;
    }
    char c = p->s[p->at];
    if (c == '(') {
        ++p->at;
        q = expression(p);
        space(p);
        if (p->s[p->at] != ')')
            fail(p, "Expected closing parenthesis");
        else
            ++p->at;
    } else if (isdigit((unsigned char)c) || c == '.') {
        char *end;
        errno = 0;
        q.v = strtod(p->s + p->at, &end);
        if (end == p->s + p->at || errno || !isfinite(q.v))
            fail(p, "Invalid number");
        p->at = (int)(end - p->s);
    } else if (isalpha((unsigned char)c)) {
        char name[24];
        int n = 0;
        while (isalnum((unsigned char)p->s[p->at])) {
            if (n < 23)
                name[n++] = p->s[p->at];
            else
                fail(p, "Unit name too long");
            ++p->at;
        }
        name[n] = 0;
        int found = 0;
        if (!strcmp(name, "pi")) {
            q.v = CL_PI;
            found = 1;
        }
        for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i)
            if (!strcmp(name, units[i].name)) {
                q.v = units[i].scale;
                memcpy(q.d, units[i].d, sizeof(q.d));
                found = 1;
                break;
            }
        if (!found)
            fail(p, "Unknown unit (case-sensitive)");
    } else {
        fail(p, "Expected number, unit or parenthesis");
    }
    --p->depth;
    return q;
}
static Quantity unary(QParser *p);
static Quantity power(QParser *p) {
    Quantity q = atom(p);
    space(p);
    if (p->s[p->at] == '^' && !p->error) {
        ++p->at;
        space(p);
        char *end;
        errno = 0;
        long exponent = strtol(p->s + p->at, &end, 10);
        if (end == p->s + p->at || errno || exponent < -16 || exponent > 16) {
            fail(p, "Unit exponent: integer from -16 to 16");
            return q;
        }
        p->at = (int)(end - p->s);
        if (q.v == 0 && exponent <= 0)
            fail(p, "Zero power base");
        double old = q.v;
        q.v = pow(q.v, (double)exponent);
        if (!isfinite(q.v) || (old != 0 && q.v == 0))
            fail(p, "Power overflow or loss of precision");
        for (int i = 0; i < 8; ++i) {
            q.d[i] *= (int)exponent;
            if (abs(q.d[i]) > 32)
                fail(p, "Dimension too large");
        }
        space(p);
        if (p->s[p->at] == '^')
            fail(p, "Put compound exponents in parentheses");
    }
    return q;
}
static Quantity unary(QParser *p) {
    space(p);
    int sign = 1, n = 0;
    while (p->s[p->at] == '+' || p->s[p->at] == '-') {
        if (++n > 32) {
            fail(p, "Too many symbols");
            break;
        }
        if (p->s[p->at++] == '-')
            sign = -sign;
        space(p);
    }
    Quantity q = power(p);
    q.v *= sign;
    return q;
}
static Quantity product(QParser *p) {
    Quantity q = unary(p);
    while (!p->error) {
        space(p);
        char op = p->s[p->at];
        int implicit = isalpha((unsigned char)op) || op == '(';
        if (op != '*' && op != '/' && !implicit)
            break;
        if (!implicit)
            ++p->at;
        Quantity b = unary(p);
        if (op == '/' && b.v == 0) {
            fail(p, "Division by zero");
            break;
        }
        double old = q.v;
        q.v = op == '/' ? q.v / b.v : q.v * b.v;
        if (!isfinite(q.v) || (q.v == 0 && old != 0 && b.v != 0))
            fail(p, "Overflow or loss of precision");
        for (int i = 0; i < 8; ++i) {
            q.d[i] += op == '/' ? -b.d[i] : b.d[i];
            if (abs(q.d[i]) > 32)
                fail(p, "Dimension too large");
        }
    }
    return q;
}
static Quantity expression(QParser *p) {
    Quantity q = product(p);
    while (!p->error) {
        space(p);
        char op = p->s[p->at];
        if (op != '+' && op != '-')
            break;
        ++p->at;
        Quantity b = product(p);
        if (memcmp(q.d, b.d, sizeof(q.d))) {
            fail(p, "Cannot add different dimensions");
            break;
        }
        q.v += op == '+' ? b.v : -b.v;
        if (!isfinite(q.v))
            fail(p, "Overflow");
    }
    return q;
}
int aero_dimensions(const char *input, double *value, int dims[8], char *unit, size_t cap,
                    char *error, size_t error_cap) {
    char fields[2][CL_EXPR];
    int n = cl_split_values(input, '|', fields, 2);
    if (n < 1 || n > 2) {
        snprintf(error, error_cap, "Input: expression | result unit");
        return 0;
    }
    QParser p = {fields[0], 0, 0, 0, NULL};
    Quantity q = expression(&p);
    space(&p);
    if (!p.error && p.s[p.at])
        fail(&p, "Extra characters or unsupported function");
    if (!p.error && n == 2) {
        QParser target = {fields[1], 0, 0, 0, NULL};
        Quantity t = expression(&target);
        space(&target);
        if (target.error || target.s[target.at] || t.v <= 0)
            fail(&p, "Invalid result unit");
        else if (memcmp(q.d, t.d, sizeof(q.d)))
            fail(&p, "Result unit has a different dimension");
        else {
            double old = q.v;
            q.v /= t.v;
            if (old != 0 && q.v == 0)
                fail(&p, "Result too small for double precision");
            snprintf(unit, cap, "%s", fields[1]);
        }
    } else if (!p.error) {
        const char *names[] = {"m", "kg", "s", "A", "K", "mol", "cd", "B"};
        size_t at = 0;
        unit[0] = 0;
        for (int i = 0; i < 8; ++i)
            if (q.d[i]) {
                int added =
                    snprintf(unit + at, cap - at, "%s%s^%d", at ? "*" : "", names[i], q.d[i]);
                if (added < 0 || (size_t)added >= cap - at) {
                    fail(&p, "Dimension text too long");
                    break;
                }
                at += (size_t)added;
            }
        if (!at)
            snprintf(unit, cap, "dimensionless");
    }
    if (!p.error && !isfinite(q.v))
        fail(&p, "Result overflow");
    if (p.error) {
        snprintf(error, error_cap, "%s", p.error);
        return 0;
    }
    *value = q.v;
    memcpy(dims, q.d, sizeof(q.d));
    error[0] = 0;
    return 1;
}
