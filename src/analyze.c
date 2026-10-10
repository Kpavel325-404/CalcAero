#include "aero.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "l10n.h"
typedef struct {
    CLProgram p, q;
    CLContext ctx;
    int second, budget, limit;
} Analyze;
static int sample(Analyze *a, double x, int difference, double *out) {
    CLContext c = a->ctx;
    c.x = cl_real(x);
    CLValue v, w;
    CLError e;
    if (!cl_eval_budget(&a->p, &c, &v, &e, &a->budget) || v.im) {
        if (e.code == CL_LIMIT)
            a->limit = 1;
        return 0;
    }
    *out = v.re;
    if (difference) {
        if (!cl_eval_budget(&a->q, &c, &w, &e, &a->budget) || w.im) {
            if (e.code == CL_LIMIT)
                a->limit = 1;
            return 0;
        }
        *out -= w.re;
    }
    return isfinite(*out);
}
static int root(Analyze *a, double left, double right, double fl, double fr, int difference,
                double *out) {
    double scale = fmax(1, fmin(fabs(fl), fabs(fr)));
    if (!fl) {
        *out = left;
        return 1;
    }
    if (!fr) {
        *out = right;
        return 1;
    }
    if (signbit(fl) == signbit(fr))
        return 0;
    double x = left / 2 + right / 2, value = 0;
    for (int i = 0; i < 60; ++i) {
        x = left / 2 + right / 2;
        if (!sample(a, x, difference, &value))
            return 0;
        if (value == 0 || fabs(value) <= 32 * DBL_EPSILON * scale) {
            *out = x;
            return 1;
        }
        if (signbit(value) == signbit(fl)) {
            left = x;
            fl = value;
        } else {
            right = x;
            fr = value;
        }
        if (right - left <= 8 * DBL_EPSILON * fmax(1, fabs(x)))
            break;
    }
    /* A tiny x interval alone is not evidence of a root: reject poles. */
    if (fabs(value) > 1e-9 * scale)
        return 0;
    *out = x;
    return 1;
}
static int extremum(Analyze *a, double left, double right, int minimum, double *out,
                    double *height) {
    const double ratio = .6180339887498948482;
    double x1 = right - ratio * (right - left), x2 = left + ratio * (right - left), f1, f2;
    if (!sample(a, x1, 0, &f1) || !sample(a, x2, 0, &f2))
        return 0;
    for (int i = 0; i < 48; ++i) {
        if ((minimum && f1 < f2) || (!minimum && f1 > f2)) {
            right = x2;
            x2 = x1;
            f2 = f1;
            x1 = right - ratio * (right - left);
            if (!sample(a, x1, 0, &f1))
                return 0;
        } else {
            left = x1;
            x1 = x2;
            f1 = f2;
            x2 = left + ratio * (right - left);
            if (!sample(a, x2, 0, &f2))
                return 0;
        }
    }
    *out = left / 2 + right / 2;
    return sample(a, *out, 0, height);
}
int aero_analyze(const char *input, const CLContext *ctx, CLToolResult *r) {
    char f[4][CL_EXPR];
    int fields = cl_split_values(input, '|', f, 4);
    if (fields != 3 && fields != 4)
        return aero_problem(r, "Input: f(x)|a|b or f(x)|g(x)|a|b");
    Analyze a;
    a.ctx = *ctx;
    a.second = fields == 4;
    a.budget = 220000;
    a.limit = 0;
    CLError e;
    CLValue lo, hi;
    if (!cl_compile(f[0], &a.p, &e) || (a.second && !cl_compile(f[1], &a.q, &e)) ||
        !cl_calculate(f[fields - 2], ctx, &lo, &e) || !cl_calculate(f[fields - 1], ctx, &hi, &e) ||
        lo.im || hi.im || lo.re >= hi.re || !isfinite(hi.re - lo.re))
        return aero_problem(r, "Check functions and finite interval a<b");
    const int points = 129;
    double y[129], difference[129];
    int valid[129], both[129];
    double step = (hi.re - lo.re) / (points - 1);
    for (int i = 0; i < points; ++i) {
        double x = lo.re + step * i;
        valid[i] = sample(&a, x, 0, &y[i]);
        both[i] = a.second && sample(&a, x, 1, &difference[i]);
    }
    if (a.limit)
        return aero_problem(r, "Operation limit: shorten formula or interval");
    aero_line(r, "Numerical search on [%.9g; %.9g]", lo.re, hi.re);
    int roots = 0, crossings = 0, extrema = 0;
    for (int curve = 0; curve <= a.second; ++curve) {
        double previous = -INFINITY;
        int count = 0, all_zero = 1;
        for (int i = 0; i < points; ++i)
            if (!(curve ? both[i] : valid[i]) || (curve ? difference[i] : y[i]) != 0)
                all_zero = 0;
        if (all_zero) {
            aero_line(r, curve ? "Function difference is zero at every sample"
                               : "f(x)=0 at every sample");
            continue;
        }
        for (int i = 0; i < points - 1; ++i) {
            if (!(curve ? both[i] && both[i + 1] : valid[i] && valid[i + 1]))
                continue;
            double f1 = curve ? difference[i] : y[i], f2 = curve ? difference[i + 1] : y[i + 1], x;
            if (!root(&a, lo.re + step * i, lo.re + step * (i + 1), f1, f2, curve, &x))
                continue;
            if (isfinite(previous) && fabs(x - previous) < step * .001)
                continue;
            previous = x;
            ++count;
            if (count <= 10) {
                if (curve) {
                    double height;
                    if (sample(&a, x, 0, &height))
                        aero_line(r, "Intersection: x~=%.12g, y~=%.9g", x, height);
                } else
                    aero_line(r, "Root: x ~= %.14g", x);
            }
        }
        if (curve)
            crossings = count;
        else
            roots = count;
        if (count > 10)
            aero_line(r, "Found %d; first 10 shown", count);
    }
    for (int i = 1; i < points - 1; ++i)
        if (valid[i - 1] && valid[i] && valid[i + 1]) {
            int minimum =
                y[i] < y[i - 1] && y[i]<y[i + 1], maximum = y[i]> y[i - 1] && y[i] > y[i + 1];
            if (!minimum && !maximum)
                continue;
            double x, height;
            if (!extremum(&a, lo.re + step * (i - 1), lo.re + step * (i + 1), minimum, &x, &height))
                continue;
            if (++extrema <= 10)
                aero_line(r, "%s: x~=%.11g; y~=%.11g", minimum ? "min" : "max", x, height);
        }
    if (a.limit)
        return aero_problem(r, "Operation limit reached; use a simpler graph");
    aero_line(r, "Roots: %d; intersections: %d; extrema: %d", roots, crossings, extrema);
    aero_line(r, "129 samples; discontinuities may be missed");
    aero_line(r, "Roots: sign changes / exact sampled zeros");
    r->ok = 1;
    r->approx = 1;
    r->value = roots;
    return 1;
}
