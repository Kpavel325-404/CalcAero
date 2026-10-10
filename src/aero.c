#include "aero.h"
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "l10n.h"
void aero_line(CLToolResult *r, const char *format, ...) {
    if (r->count >= CL_TOOL_LINES)
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(r->line[r->count++], sizeof(r->line[0]), format, args);
    va_end(args);
}
int aero_problem(CLToolResult *r, const char *message) {
    r->count = 0;
    r->ok = 0;
    aero_line(r, "%s", message);
    return 0;
}
void aero_defaults(AeroState *s) {
    memset(s, 0, sizeof(*s));
    s->pretty = 1;
    s->matrix_n = 2;
    s->study_seed = 20261009;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < 4; ++i)
            s->matrix[k][i * 4 + i] = 1;
}
static void trim(char *text) {
    size_t begin = 0, n = strlen(text);
    while (isspace((unsigned char)text[begin]))
        ++begin;
    while (n > begin && isspace((unsigned char)text[n - 1]))
        --n;
    memmove(text, text + begin, n - begin);
    text[n - begin] = 0;
}
static int split(const char *s, char separator, char out[][CL_EXPR], int maximum) {
    int n = cl_split_values(s, separator, out, maximum);
    for (int i = 0; i < n; ++i) {
        trim(out[i]);
        if (!out[i][0])
            return -1;
    }
    return n;
}
static int append(char out[CL_EXPR], size_t *at, const char *text, size_t n, CLError *e) {
    if (*at + n >= CL_EXPR) {
        *e = (CLError){CL_LIMIT, 0};
        return 0;
    }
    memcpy(out + *at, text, n);
    *at += n;
    out[*at] = 0;
    return 1;
}
static int expand_inner(const char *text, const AeroState *s, unsigned mask, int depth,
                        const char *argument, char out[CL_EXPR], CLError *e) {
    size_t at = 0;
    out[0] = 0;
    if (depth > 8 || strlen(text) >= CL_EXPR) {
        *e = (CLError){CL_LIMIT, 0};
        return 0;
    }
    for (size_t i = 0; text[i];) {
        if (isalpha((unsigned char)text[i]) || text[i] == '_') {
            size_t begin = i;
            while (isalnum((unsigned char)text[i]) || text[i] == '_')
                ++i;
            size_t len = i - begin, next = i;
            while (isspace((unsigned char)text[next]))
                ++next;
            int slot =
                len == 1 && text[begin] >= 'f' && text[begin] <= 'h' ? text[begin] - 'f' : -1;
            if (slot >= 0 && text[next] == '(') {
                if (!s->function[slot][0] || (mask & (1u << slot))) {
                    *e = (CLError){CL_UNKNOWN, (int)begin};
                    return 0;
                }
                size_t start = next + 1, end = start;
                int nesting = 1;
                while (text[end] && nesting) {
                    if (text[end] == '(')
                        ++nesting;
                    else if (text[end] == ')')
                        --nesting;
                    if (nesting)
                        ++end;
                }
                if (nesting) {
                    *e = (CLError){CL_SYNTAX, (int)next};
                    return 0;
                }
                char raw[CL_EXPR], arg[CL_EXPR], body[CL_EXPR];
                size_t n = end - start;
                memcpy(raw, text + start, n);
                raw[n] = 0;
                if (!expand_inner(raw, s, mask, depth + 1, argument, arg, e) ||
                    !expand_inner(s->function[slot], s, mask | (1u << slot), depth + 1, arg, body,
                                  e))
                    return 0;
                if (!append(out, &at, "(", 1, e) || !append(out, &at, body, strlen(body), e) ||
                    !append(out, &at, ")", 1, e))
                    return 0;
                i = end + 1;
            } else if (argument && len == 1 && text[begin] == 'x') {
                if (!append(out, &at, "(", 1, e) ||
                    !append(out, &at, argument, strlen(argument), e) ||
                    !append(out, &at, ")", 1, e))
                    return 0;
            } else if (!append(out, &at, text + begin, len, e))
                return 0;
        } else {
            if (!append(out, &at, text + i, 1, e))
                return 0;
            ++i;
        }
    }
    return 1;
}
int aero_expand(const char *text, const AeroState *s, char out[CL_EXPR], CLError *e) {
    *e = (CLError){0, 0};
    return expand_inner(text, s, 0, 0, NULL, out, e);
}
int aero_compile(const char *text, const AeroState *s, CLProgram *p, CLError *e) {
    char expanded[CL_EXPR];
    return aero_expand(text, s, expanded, e) && cl_compile(expanded, p, e);
}
int aero_calculate(const char *text, const AeroState *s, const CLContext *ctx, CLValue *v,
                   CLError *e) {
    char expanded[CL_EXPR];
    return aero_expand(text, s, expanded, e) && cl_calculate(expanded, ctx, v, e);
}
int aero_define(AeroState *s, int slot, const char *body, CLError *e) {
    if (slot < 0 || slot >= AERO_FUNCTIONS || strlen(body) >= CL_EXPR) {
        *e = (CLError){CL_LIMIT, 0};
        return 0;
    }
    AeroState candidate = *s;
    snprintf(candidate.function[slot], CL_EXPR, "%s", body);
    if (body[0]) {
        char call[8];
        CLProgram p;
        snprintf(call, sizeof(call), "%c(x)", 'f' + slot);
        if (!aero_compile(call, &candidate, &p, e))
            return 0;
    }
    /* Removing a definition must not leave other saved functions broken. */
    for (int i = 0; i < AERO_FUNCTIONS; ++i)
        if (candidate.function[i][0]) {
            char call[8];
            CLProgram p;
            snprintf(call, sizeof(call), "%c(x)", 'f' + i);
            if (!aero_compile(call, &candidate, &p, e))
                return 0;
        }
    *s = candidate;
    *e = (CLError){0, 0};
    return 1;
}
const char *aero_lab_name(int t) {
    static const char *names[] = {
        "Calculation steps",          "Linear equation: steps",     "Quadratic: steps",
        "2D / 3D vectors",          "Regression and correlation",       "Dimensional calculation",
        "Formula library",        "Custom functions f / g / h",       "Binomial distribution",
        "Normal distribution", "IEEE 754: float32 / float64",  "Expand / collect terms",
        "Symbolic derivative",   "Roots, intersections, extrema"};
    return t >= 0 && t < AERO_LABS ? L(names[t]) : "?";
}
const char *aero_lab_hint(int t) {
    static const char *h[] = {"Expression; steps in evaluation order",
                              "a;b for a*x+b=0",
                              "a;b;c for a*x^2+b*x+c=0",
                              "a1;a2[;a3] | b1;b2[;b3]",
                              "linear/exp/log/power | x1;y1;x2;y2... | prediction x",
                              "60 km / (2 h) | km/h; case-sensitive units",
                              "Index | ; separated values, one unknown ?; L/R: formula",
                              "f|expression with x; likewise g/h; f| clears",
                              "n;p;k, n=0..5000, p=0..1; PMF and two probabilities",
                              "mu;sigma;lower;upper; sigma>0",
                              "32/64 | expression or 0xHEX: raw bits",
                              "Polynomial in x, degree <=8; exact coefficients",
                              "With respect to x; smooth functions, chain rule",
                              "f(x)|a|b or f(x)|g(x)|a|b; smooth functions"};
    return t >= 0 && t < AERO_LABS ? L(h[t]) : "";
}
const char *aero_lab_example(int t) {
    static const char *e[] = {"1/3+1/6",
                              "2;-3",
                              "1;-3;2",
                              "1;2;3|4;5;6",
                              "linear|0;1;1;3;2;5|3",
                              "60 km / (2 h)|km/h",
                              "0|3;?",
                              "f|x^2+2*x+1",
                              "10;0.5;3",
                              "0;1;-1;1",
                              "32|1/3",
                              "(x+1)^3-(x-1)^2",
                              "sin(x^2)+x^3",
                              "x^2-1|0|-2|2"};
    return t >= 0 && t < AERO_LABS ? e[t] : "";
}
static int reals(const char *input, const CLContext *ctx, double *v, int minimum, int maximum) {
    char fields[64][CL_EXPR];
    int n = split(input, ';', fields, maximum);
    if (n < minimum || n > maximum)
        return -1;
    for (int i = 0; i < n; ++i) {
        CLValue x;
        CLError e;
        if (!cl_calculate(fields[i], ctx, &x, &e) || x.im || !isfinite(x.re))
            return -1;
        v[i] = x.re;
    }
    return n;
}
static int vector(const char *input, const CLContext *ctx, CLToolResult *r) {
    char f[2][CL_EXPR];
    double a[3], b[3];
    if (split(input, '|', f, 2) != 2)
        return aero_problem(r, "Two vectors separated by | are required");
    int n = reals(f[0], ctx, a, 2, 3), m = reals(f[1], ctx, b, 2, 3);
    if (n < 0 || m != n)
        return aero_problem(r, "Both vectors need 2 or 3 real components of equal size");
    double la = 0, lb = 0, dot = 0;
    for (int i = 0; i < n; ++i) {
        la = hypot(la, a[i]);
        lb = hypot(lb, b[i]);
        dot += a[i] * b[i];
    }
    if (!isfinite(la) || !isfinite(lb) || !isfinite(dot))
        return aero_problem(r, "Vector calculation overflow");
    aero_line(r, "|A| ~= %.15g", la);
    aero_line(r, "|B| ~= %.15g", lb);
    aero_line(r, "A . B ~= %.15g", dot);
    if (la > 0 && lb > 0) {
        double c = 0;
        for (int i = 0; i < n; ++i)
            c += (a[i] / la) * (b[i] / lb);
        aero_line(r, "Angle ~= %.15g %s", cl_from_rad(acos(fmax(-1, fmin(1, c))), ctx->angle),
                  ctx->angle == CL_RAD   ? "RAD"
                  : ctx->angle == CL_DEG ? "DEG"
                                         : "GRAD");
    } else
        aero_line(r, "Angle with a zero vector is undefined");
    if (n == 2) {
        double c = a[0] * b[1] - a[1] * b[0];
        if (!isfinite(c))
            return aero_problem(r, "Cross product overflow");
        aero_line(r, "A x B (z) ~= %.15g", c);
    } else {
        double c[] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                      a[0] * b[1] - a[1] * b[0]};
        for (int i = 0; i < 3; ++i)
            if (!isfinite(c[i]))
                return aero_problem(r, "Cross product overflow");
        for (int i = 0; i < 3; ++i)
            aero_line(r, "(A x B)[%d] ~= %.15g", i + 1, c[i]);
    }
    for (int i = 0; i < n; ++i) {
        double sum = a[i] + b[i];
        if (!isfinite(sum))
            return aero_problem(r, "Vector sum overflow");
        aero_line(r, "(A+B)[%d] ~= %.15g", i + 1, sum);
    }
    r->ok = 1;
    r->value = dot;
    r->approx = 1;
    return 1;
}
static int regression(const char *input, const CLContext *ctx, CLToolResult *r) {
    char f[3][CL_EXPR];
    int fields = split(input, '|', f, 3);
    double data[64], x[32], y[32];
    if (fields < 2 || fields > 3)
        return aero_problem(r, "Input: linear/exp/log/power | x;y pairs | prediction x");
    int mode = !strcmp(f[0], "linear")  ? 0
               : !strcmp(f[0], "exp")   ? 1
               : !strcmp(f[0], "log")   ? 2
               : !strcmp(f[0], "power") ? 3
                                        : -1;
    int count = reals(f[1], ctx, data, 4, 64);
    if (mode < 0 || count < 4 || count % 2)
        return aero_problem(r, "Select model type and 2 to 32 x;y pairs");
    int n = count / 2;
    for (int i = 0; i < n; ++i) {
        x[i] = data[2 * i];
        y[i] = data[2 * i + 1];
        if ((mode == 1 || mode == 3) && y[i] <= 0)
            return aero_problem(r, "exp/power requires y>0");
        if ((mode == 2 || mode == 3) && x[i] <= 0)
            return aero_problem(r, "log/power requires x>0");
        if (mode == 1 || mode == 3)
            y[i] = log(y[i]);
        if (mode == 2 || mode == 3)
            x[i] = log(x[i]);
    }
    double mx = x[0], my = y[0], dx = 0, dy = 0;
    for (int i = 0; i < n; ++i) {
        dx += (x[i] - x[0]) / n;
        dy += (y[i] - y[0]) / n;
    }
    mx += dx;
    my += dy;
    double sx = 0, sy = 0;
    for (int i = 0; i < n; ++i) {
        sx = fmax(sx, fabs(x[i] - mx));
        sy = fmax(sy, fabs(y[i] - my));
    }
    if (!isfinite(mx) || !isfinite(my) || !isfinite(sx) || !isfinite(sy) || sx == 0)
        return aero_problem(r, "Identical x values or insufficient numerical precision");
    double xx = 0, yy = 0, xy = 0;
    for (int i = 0; i < n; ++i) {
        double u = (x[i] - mx) / sx, v = sy ? (y[i] - my) / sy : 0;
        xx += u * u;
        yy += v * v;
        xy += u * v;
    }
    double slope = (xy / xx) * (sy / sx), intercept = my - slope * mx;
    if (!isfinite(slope) || !isfinite(intercept))
        return aero_problem(r, "Coefficient overflow");
    double a = (mode == 1 || mode == 3) ? exp(intercept) : intercept;
    if (!isfinite(a) || ((mode == 1 || mode == 3) && a == 0))
        return aero_problem(r, "Model scale exceeds double precision");
    const char *models[] = {"y=a+b*x", "y=a*exp(b*x)", "y=a+b*ln(x)", "y=a*x^b"};
    aero_line(r, "%s; n=%d", models[mode], n);
    aero_line(r, "a ~= %.15g", a);
    aero_line(r, "b ~= %.15g", slope);
    if (sy) {
        double corr = xy / sqrt(xx * yy);
        corr = fmax(-1, fmin(1, corr));
        aero_line(r, "r ~= %.15g", corr);
        aero_line(r, "R^2 ~= %.15g", corr * corr);
    } else
        aero_line(r, "Constant y: correlation is undefined");
    if (mode)
        aero_line(r, "r/R^2 refer to transformed data");
    if (fields == 3) {
        double query[1];
        if (reals(f[2], ctx, query, 1, 1) < 0)
            return aero_problem(r, "Invalid prediction point");
        if ((mode == 2 || mode == 3) && query[0] <= 0)
            return aero_problem(r, "Prediction point must be >0");
        double u = (mode == 2 || mode == 3) ? log(query[0]) : query[0];
        double pred = my + slope * (u - mx);
        if (mode == 1 || mode == 3)
            pred = exp(pred);
        if (!isfinite(pred))
            return aero_problem(r, "Prediction overflow");
        aero_line(r, "y(%.10g) ~= %.15g", query[0], pred);
    }
    r->ok = 1;
    r->value = slope;
    r->approx = 1;
    return 1;
}
static double binomial_logpmf(int n, int k, double p) {
    return lgamma(n + 1.0) - lgamma(k + 1.0) - lgamma(n - k + 1.0) + k * log(p) +
           (n - k) * log1p(-p);
}
double aero_binomial_pmf(int n, int k, double p) {
    if (n < 0 || n > 5000 || !isfinite(p) || p < 0 || p > 1)
        return NAN;
    if (k < 0 || k > n)
        return 0;
    if (p == 0)
        return k == 0 ? 1 : 0;
    if (p == 1)
        return k == n ? 1 : 0;
    return exp(binomial_logpmf(n, k, p));
}
static double binomial_tail(int n, int k, double p, int lower) {
    if (lower) {
        if (k < 0)
            return 0;
        if (k >= n)
            return 1;
    } else {
        if (k < 0)
            return 1;
        if (k >= n)
            return 0;
    }
    if (p == 0)
        return lower ? 1 : 0;
    if (p == 1)
        return lower ? 0 : 1;
    int j = lower ? k : k + 1;
    double sum = 1, term = 1;
    if (lower)
        for (int i = j; i > 0; --i) {
            term *= ((double)i / (n - i + 1)) * ((1 - p) / p);
            sum += term;
            if (term < DBL_EPSILON * sum)
                break;
        }
    else
        for (int i = j; i < n; ++i) {
            term *= ((double)(n - i) / (i + 1)) * (p / (1 - p));
            sum += term;
            if (term < DBL_EPSILON * sum)
                break;
        }
    return fmin(1, exp(binomial_logpmf(n, j, p)) * sum);
}
double aero_binomial_cdf(int n, int k, double p) {
    if (n < 0 || n > 5000 || !isfinite(p) || p < 0 || p > 1)
        return NAN;
    return k < n * p ? binomial_tail(n, k, p, 1) : 1 - binomial_tail(n, k, p, 0);
}
double aero_normal_interval(double mu, double sigma, double lo, double hi) {
    if (!isfinite(mu) || !isfinite(sigma) || sigma <= 0 || isnan(lo) || isnan(hi) || lo > hi)
        return NAN;
    double delta_a = lo - mu, delta_b = hi - mu;
    double a = (isfinite(delta_a) ? delta_a / sigma : ((lo / 2 - mu / 2) / sigma) * 2) / sqrt(2);
    double b = (isfinite(delta_b) ? delta_b / sigma : ((hi / 2 - mu / 2) / sigma) * 2) / sqrt(2), p;
    if (a >= 0)
        p = .5 * (erfc(a) - erfc(b));
    else if (b <= 0)
        p = .5 * (erfc(-b) - erfc(-a));
    else
        p = .5 * (erf(b) - erf(a));
    return fmax(0, fmin(1, p));
}
static int probability(int t, const char *input, const CLContext *ctx, CLToolResult *r) {
    double v[4];
    int n = reals(input, ctx, v, t == AE_BINOMIAL ? 3 : 4, t == AE_BINOMIAL ? 3 : 4);
    if (n < 0)
        return aero_problem(r, "Check distribution parameters");
    if (t == AE_BINOMIAL) {
        if (v[0] != floor(v[0]) || v[0] < 0 || v[0] > 5000 || v[2] != floor(v[2]) || v[2] < 0 ||
            v[2] > v[0] || v[1] < 0 || v[1] > 1)
            return aero_problem(r, "n=0..5000, integer k=0..n, p=0..1");
        int trials = (int)v[0], k = (int)v[2];
        double cdf = aero_binomial_cdf(trials, k, v[1]);
        double sf = k < trials * v[1] ? 1 - cdf : binomial_tail(trials, k, v[1], 0);
        aero_line(r, "P(X=k) ~= %.15g", aero_binomial_pmf(trials, k, v[1]));
        aero_line(r, "P(X<=k) ~= %.15g", cdf);
        aero_line(r, "P(X>k) ~= %.15g", sf);
        aero_line(r, "Mean = n*p ~= %.15g", v[0] * v[1]);
        aero_line(r, "Variance ~= %.15g", v[0] * v[1] * (1 - v[1]));
        r->value = cdf;
    } else {
        double p = aero_normal_interval(v[0], v[1], v[2], v[3]);
        if (!isfinite(p))
            return aero_problem(r, "sigma>0; lower bound <= upper bound");
        aero_line(r, "P(%.9g<=X<=%.9g) ~= %.15g", v[2], v[3], p);
        aero_line(r, "mu=%.12g; sigma=%.12g", v[0], v[1]);
        aero_line(r, "CDF(lower) ~= %.15g", aero_normal_interval(v[0], v[1], -INFINITY, v[2]));
        aero_line(r, "CDF(upper) ~= %.15g", aero_normal_interval(v[0], v[1], -INFINITY, v[3]));
        r->value = p;
    }
    r->ok = 1;
    r->approx = 1;
    return 1;
}
static int equation_steps(int tool, const char *input, const CLContext *ctx, CLToolResult *r) {
    char f[3][CL_EXPR];
    int count = tool == AE_LINEAR ? 2 : 3;
    if (split(input, ';', f, count) != count)
        return aero_problem(r, "Coefficients separated by ; are required");
    CLValue v[3];
    CLError e;
    for (int i = 0; i < count; ++i)
        if (!cl_calculate(f[i], ctx, &v[i], &e) || v[i].im)
            return aero_problem(r, "Coefficients must be real");
    if (tool == AE_LINEAR || v[0].re == 0) {
        CLValue a = tool == AE_LINEAR ? v[0] : v[1], b = tool == AE_LINEAR ? v[1] : v[2];
        char sa[160], sb[160], expr[CL_EXPR], result[160];
        cl_format(a, 15, 0, 1, sa, sizeof(sa));
        cl_format(b, 15, 0, 1, sb, sizeof(sb));
        aero_line(r, "(%s)*x+(%s)=0", sa, sb);
        if (a.re == 0) {
            aero_line(r, b.re == 0 ? "Any real x" : "No solutions");
            r->ok = 1;
            return 1;
        }
        int length = snprintf(expr, sizeof(expr), "(-(%s))/(%s)", tool == AE_LINEAR ? f[1] : f[2],
                              tool == AE_LINEAR ? f[0] : f[1]);
        CLValue answer;
        if (length < 0 || length >= CL_EXPR || !cl_calculate(expr, ctx, &answer, &e))
            return aero_problem(r, "Result outside calculation limits");
        cl_format(answer, 15, 0, 1, result, sizeof(result));
        aero_line(r, "1. Rearrange: (%s)*x=-(%s)", sa, sb);
        aero_line(r, "2. Divide both sides by %s", sa);
        aero_line(r, "x %s %s", answer.exact ? "=" : "~=", result);
        r->value = answer.re;
        r->approx = !answer.exact;
        r->ok = 1;
        return 1;
    }
    CLToolResult roots;
    if (!cl_tool_run(0, input, ctx, &roots))
        return aero_problem(r, roots.line[0]);
    aero_line(r, "1. a*x^2+b*x+c=0, a!=0");
    aero_line(r, "2. D=b^2-4*a*c");
    char expr[CL_EXPR], formatted[160];
    int length = snprintf(expr, sizeof(expr), "(%s)^2-4*(%s)*(%s)", f[1], f[0], f[2]);
    CLValue d;
    if (length > 0 && length < CL_EXPR && cl_calculate(expr, ctx, &d, &e)) {
        cl_format(d, 15, 0, 1, formatted, sizeof(formatted));
        aero_line(r, "D %s %s", d.exact ? "=" : "~=", formatted);
    } else
        aero_line(r, "Roots below use scaling");
    aero_line(r, "3. x=(-b +/- sqrt(D))/(2*a)");
    aero_line(r, "Numerically stable formula: q/a and c/q");
    for (int i = 0; i < roots.count; ++i)
        aero_line(r, "%s", roots.line[i]);
    r->ok = 1;
    r->value = roots.value;
    r->approx = roots.approx;
    return 1;
}
typedef struct {
    const char *name, *variables, *example, *solve[3];
    int count, positive;
} Formula;
static const Formula formulas[AERO_FORMULAS] = {
    {"Circle area", "r [m]; area [m^2]", "0|3;?", {"sqrt(B/pi)", "pi*A^2", NULL}, 2, 3},
    {"Rectangle area",
     "width [m]; height [m]; area [m^2]",
     "1|3;4;?",
     {"C/B", "C/A", "A*B"},
     3,
     7},
    {"Sphere volume", "r [m]; volume [m^3]", "2|2;?", {"cbrt(3*B/(4*pi))", "4*pi*A^3/3", NULL}, 2, 3},
    {"Pythagorean theorem",
     "a [m]; b [m]; c [m]",
     "3|3;4;?",
     {"sqrt(C^2-B^2)", "sqrt(C^2-A^2)", "hypot(A;B)"},
     3,
     7},
    {"Newton's second law",
     "mass [kg]; acceleration [m/s^2]; force [N]",
     "4|2;3;?",
     {"C/B", "C/A", "A*B"},
     3,
     1},
    {"Ohm's law",
     "voltage [V]; current [A]; resistance [Ohm]",
     "5|12;?;4",
     {"B*C", "A/C", "A/B"},
     3,
     4},
    {"Electric power",
     "voltage [V]; current [A]; power [W]",
     "6|12;2;?",
     {"C/B", "C/A", "A*B"},
     3,
     0},
    {"Uniform motion",
     "distance [m]; time [s]; speed [m/s]",
     "7|100;5;?",
     {"B*C", "A/C", "A/B"},
     3,
     2},
    {"Kinetic energy",
     "mass [kg]; speed [m/s]; energy [J]",
     "8|2;3;?",
     {"2*C/B^2", "sqrt(2*C/A)", "A*B^2/2"},
     3,
     7},
    {"Density",
     "mass [kg]; volume [m^3]; density [kg/m^3]",
     "9|10;2;?",
     {"B*C", "A/C", "A/B"},
     3,
     7},
    {"Period and frequency", "period [s]; frequency [Hz]", "10|0.02;?", {"1/B", "1/A", NULL}, 2, 3},
    {"Potential energy",
     "mass [kg]; height [m]; energy [J], g=g0",
     "11|2;3;?",
     {"C/(g0*B)", "C/(g0*A)", "g0*A*B"},
     3,
     1}};
const char *aero_formula_name(int id) {
    return id >= 0 && id < AERO_FORMULAS ? L(formulas[id].name) : "?";
}
const char *aero_formula_example(int id) {
    return id >= 0 && id < AERO_FORMULAS ? formulas[id].example : "";
}
const char *aero_formula_variables(int id) {
    return id >= 0 && id < AERO_FORMULAS ? L(formulas[id].variables) : "";
}
static int formula(const char *input, const CLContext *ctx, CLToolResult *r) {
    char f[2][CL_EXPR], v[3][CL_EXPR];
    CLValue index;
    CLError e;
    if (split(input, '|', f, 2) != 2 || !cl_calculate(f[0], ctx, &index, &e) || index.im ||
        index.re != floor(index.re) || index.re < 0 || index.re >= AERO_FORMULAS)
        return aero_problem(r, "Formula index 0..11 | values with one ?");
    const Formula *formula = &formulas[(int)index.re];
    if (split(f[1], ';', v, 3) != formula->count)
        return aero_problem(r, formula->variables);
    CLContext c = *ctx;
    int missing = -1;
    for (int i = 0; i < formula->count; ++i) {
        if (!strcmp(v[i], "?")) {
            if (missing != -1)
                return aero_problem(r, "Exactly one unknown ? is required");
            missing = i;
        } else {
            if (!cl_calculate(v[i], ctx, &c.mem[i], &e) || c.mem[i].im)
                return aero_problem(r, "Invalid known quantity");
            if ((formula->positive & (1 << i)) && c.mem[i].re < 0)
                return aero_problem(r, "This quantity must be nonnegative");
        }
    }
    if (missing < 0)
        return aero_problem(r, "Mark the unknown with ?");
    CLValue answer;
    if (!cl_calculate(formula->solve[missing], &c, &answer, &e) || answer.im)
        return aero_problem(r, "No unique admissible result (check zeros and data)");
    if ((formula->positive & (1 << missing)) && answer.re < 0)
        return aero_problem(r, "Physically inadmissible quantity");
    char value[160];
    cl_format(answer, 15, 0, 1, value, sizeof(value));
    aero_line(r, "%s", formula->name);
    aero_line(r, "Quantity order: %s", formula->variables);
    aero_line(r, "Unknown #%d: %s", missing + 1, formula->solve[missing]);
    for (int i = 0; i < formula->count; ++i)
        if (i != missing) {
            char known[80];
            cl_format(c.mem[i], 12, 0, 1, known, sizeof(known));
            aero_line(r, "%c = %s", 'A' + i, known);
        }
    aero_line(r, "Result %s %s", answer.exact ? "=" : "~=", value);
    aero_line(r, "Result uses the unknown's unit in the list above");
    r->value = answer.re;
    r->approx = !answer.exact;
    r->ok = 1;
    return 1;
}
static int ieee(const char *input, const CLContext *ctx, CLToolResult *r) {
    char fields[2][CL_EXPR];
    double width[1];
    uint64_t raw = 0;
    if (split(input, '|', fields, 2) != 2 || reals(fields[0], ctx, width, 1, 1) < 0 ||
        (width[0] != 32 && width[0] != 64))
        return aero_problem(r, "Input: 32/64 | number or 0xHEX");
    int bits = (int)width[0], exponent_bits = bits == 32 ? 8 : 11,
        fraction_bits = bits == 32 ? 23 : 52, bias = bits == 32 ? 127 : 1023;
    if (!strncmp(fields[1], "0x", 2) || !strncmp(fields[1], "0X", 2)) {
        char error[100];
        if (!cl_programmer(fields[1], bits, 0, &raw, error, sizeof(error)))
            return aero_problem(r, error);
    } else {
        CLValue v;
        CLError e;
        if (!cl_calculate(fields[1], ctx, &v, &e) || v.im)
            return aero_problem(r, "A real number is required");
        if (bits == 32) {
            float f = (float)v.re;
            if (!isfinite(f))
                return aero_problem(r, "Number overflows float32");
            uint32_t b;
            memcpy(&b, &f, 4);
            raw = b;
        } else
            memcpy(&raw, &v.re, 8);
    }
    uint64_t fraction = raw & ((UINT64_C(1) << fraction_bits) - 1),
             exponent = (raw >> fraction_bits) & ((UINT64_C(1) << exponent_bits) - 1);
    int sign = (int)(raw >> (bits - 1)), maximum = (1 << exponent_bits) - 1;
    const char *category = exponent == (uint64_t)maximum ? (fraction ? "NaN" : "Infinity")
                           : exponent == 0               ? (fraction ? "Subnormal" : "Zero")
                                                         : "Normal";
    aero_line(r, "IEEE 754 float%d: %s", bits, category);
    aero_line(r, bits == 32 ? "HEX: %08llX" : "HEX: %016llX", (unsigned long long)raw);
    aero_line(r, "Sign: %d", sign);
    aero_line(r, "Exponent bits: %llu, bias: %d", (unsigned long long)exponent, bias);
    if (exponent != (uint64_t)maximum) {
        aero_line(r, "Power of 2: %d", exponent ? (int)exponent - bias : 1 - bias);
        double value;
        if (bits == 32) {
            uint32_t b = (uint32_t)raw;
            float f;
            memcpy(&f, &b, 4);
            value = f;
        } else
            memcpy(&value, &raw, 8);
        aero_line(r, "Value: %.17g", value);
        r->value = value;
        aero_line(r, "Significand: %.17g",
                  (exponent ? 1.0 : 0.0) + (double)fraction / ldexp(1, fraction_bits));
    } else
        aero_line(r, "Special value; no normal exponent");
    char binary[65];
    for (int i = 0; i < fraction_bits; ++i)
        binary[i] = (fraction >> (fraction_bits - 1 - i)) & 1 ? '1' : '0';
    binary[fraction_bits] = 0;
    aero_line(r, "Fraction bits (%d):", fraction_bits);
    aero_line(r, "%s", binary);
    r->ok = 1;
    return 1;
}
const char *aero_matrix_name(int op) {
    static const char *names[] = {"A+B",    "A-B", "A*B", "A^T", "Rank of A (numerical)",
                                  "det(A)", "A^-1"};
    return op >= 0 && op < AERO_MATRIX_OPS ? L(names[op]) : "?";
}
int aero_matrix_run(const AeroState *s, int op, CLToolResult *r) {
    memset(r, 0, sizeof(*r));
    int n = s->matrix_n;
    if (n < 1 || n > 4 || op < 0 || op >= AERO_MATRIX_OPS)
        return aero_problem(r, "Invalid matrix");
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < 16; ++i)
            if (!isfinite(s->matrix[k][i]))
                return aero_problem(r, "Matrix contains an invalid number");
    if (op == AM_DET || op == AM_INVERSE) {
        if (op == AM_DET) {
            int nonzero = 0;
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    nonzero |= s->matrix[0][i * 4 + j] != 0;
            if (!nonzero) {
                aero_line(r, "det = 0");
                r->ok = 1;
                return 1;
            }
        }
        char input[CL_EXPR];
        int at = snprintf(input, sizeof(input), "%d", n);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                int added = snprintf(input + at, sizeof(input) - (size_t)at, ";%.17g",
                                     s->matrix[0][i * 4 + j]);
                if (added < 0 || added >= (int)sizeof(input) - at)
                    return aero_problem(r, "Matrix too large for calculation");
                at += added;
            }
        CLContext c;
        cl_context_init(&c);
        return cl_tool_run(op == AM_DET ? 3 : 4, input, &c, r);
    }
    if (op == AM_RANK) {
        double a[4][4], scale = 0;
        int rank = 0;
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                scale = fmax(scale, fabs(s->matrix[0][i * 4 + j]));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                a[i][j] = scale ? s->matrix[0][i * 4 + j] / scale : 0;
        for (int col = 0; col < n && rank < n; ++col) {
            int best = rank;
            for (int row = rank + 1; row < n; ++row)
                if (fabs(a[row][col]) > fabs(a[best][col]))
                    best = row;
            if (fabs(a[best][col]) <= 64 * DBL_EPSILON)
                continue;
            for (int j = 0; j < n; ++j) {
                double tmp = a[rank][j];
                a[rank][j] = a[best][j];
                a[best][j] = tmp;
            }
            for (int row = rank + 1; row < n; ++row) {
                double factor = a[row][col] / a[rank][col];
                for (int j = col; j < n; ++j)
                    a[row][j] -= factor * a[rank][j];
            }
            ++rank;
        }
        aero_line(r, "Numerical rank of A = %d", rank);
        aero_line(r, "Relative threshold: 64*epsilon(double)");
        r->value = rank;
        r->ok = 1;
        return 1;
    }
    double out[4][4];
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            double a = s->matrix[0][i * 4 + j], b = s->matrix[1][i * 4 + j];
            if (op == AM_ADD)
                out[i][j] = a + b;
            else if (op == AM_SUB)
                out[i][j] = a - b;
            else if (op == AM_TRANSPOSE)
                out[i][j] = s->matrix[0][j * 4 + i];
            else {
                out[i][j] = 0;
                for (int k = 0; k < n; ++k)
                    out[i][j] += s->matrix[0][i * 4 + k] * s->matrix[1][k * 4 + j];
            }
            if (!isfinite(out[i][j]))
                return aero_problem(r, "Matrix calculation overflow");
        }
    aero_line(r, "%s, %d x %d", aero_matrix_name(op), n, n);
    for (int i = 0; i < n; ++i) {
        char row[160];
        size_t at = 0;
        row[0] = 0;
        for (int j = 0; j < n; ++j) {
            int added = snprintf(row + at, sizeof(row) - at, "%s%.12g", j ? " ; " : "", out[i][j]);
            if (added < 0 || (size_t)added >= sizeof(row) - at)
                return aero_problem(r, "Result line too long");
            at += (size_t)added;
        }
        aero_line(r, "%s", row);
    }
    r->ok = 1;
    r->approx = 1;
    r->value = out[0][0];
    return 1;
}
static uint32_t random32(AeroState *s) {
    uint32_t x = s->study_seed ? s->study_seed : 20261009;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->study_seed = x;
    return x;
}
void aero_study_question(AeroState *s, char question[CL_EXPR], CLValue *answer,
                         char explanation[CL_EXPR]) {
    int a = 1 + (int)(random32(s) % 9), b = 1 + (int)(random32(s) % 9),
        c = 1 + (int)(random32(s) % 9), d = 1 + (int)(random32(s) % 9);
    CLContext ctx;
    cl_context_init(&ctx);
    CLError e;
    char expression[80];
    switch (s->study_topic % AERO_STUDY_TOPICS) {
    case 0:
        snprintf(expression, sizeof(expression), "%d/%d+%d/%d", a, b, c, d);
        cl_calculate(expression, &ctx, answer, &e);
        snprintf(question, CL_EXPR, "Find the exact sum: %s", expression);
        snprintf(explanation, CL_EXPR, "Common denominator %d; numerator %d+%d. Reduce the fraction.",
                 b * d, a * d, c * b);
        break;
    case 1:
        *answer = cl_fraction(b, a);
        snprintf(question, CL_EXPR, "Solve: %d*x-%d=0", a, b);
        snprintf(explanation, CL_EXPR, "Move %d to the right; divide both sides by %d.", b, a);
        break;
    case 2: {
        const int angles[] = {0, 30, 90};
        int id = a % 3;
        *answer = cl_fraction(id == 0 ? 0 : 1, id == 1 ? 2 : 1);
        snprintf(question, CL_EXPR, "Calculate sin(%d) in DEG", angles[id]);
        snprintf(explanation, CL_EXPR, "sin(0)=0; sin(30)=1/2; sin(90)=1 (degrees).");
        break;
    }
    case 3: {
        int power = 1 + a % 4, point = 1 + b % 5;
        *answer = cl_integer((int64_t)(power * pow(point, power - 1)));
        snprintf(question, CL_EXPR, "Find derivative of x^%d at x=%d", power, point);
        snprintf(explanation, CL_EXPR, "Power rule: %d*x^%d. Substitute x=%d.", power, power - 1,
                 point);
        break;
    }
    case 4:
        *answer = cl_integer(a * d - b * c);
        snprintf(question, CL_EXPR, "Determinant: [%d %d; %d %d]", a, b, c, d);
        snprintf(explanation, CL_EXPR, "det=a*d-b*c: %d*%d-%d*%d.", a, d, b, c);
        break;
    default: {
        int n = 2 + a % 7;
        *answer = cl_fraction(1, 1 << n);
        snprintf(question, CL_EXPR, "Fair coin: probability of %d heads in a row?", n);
        snprintf(explanation, CL_EXPR, "Independent tosses: (1/2)^%d. Enter number or fraction.", n);
        break;
    }
    }
}
int aero_analyze(const char *input, const CLContext *ctx, CLToolResult *r);
int aero_lab_run(int t, const char *input, const CLContext *ctx, AeroState *s, CLToolResult *r) {
    memset(r, 0, sizeof(*r));
    if (t == AE_FUNCTION) {
        char fields[2][CL_EXPR];
        CLError e;
        const char *pipe = strchr(input, '|');
        if (!pipe || pipe - input >= CL_EXPR || strlen(pipe + 1) >= CL_EXPR)
            return aero_problem(r, "Input: f|formula with x; f,g,h allowed");
        memcpy(fields[0], input, (size_t)(pipe - input));
        fields[0][pipe - input] = 0;
        snprintf(fields[1], CL_EXPR, "%s", pipe + 1);
        trim(fields[0]);
        trim(fields[1]);
        if (strlen(fields[0]) != 1 || fields[0][0] < 'f' || fields[0][0] > 'h')
            return aero_problem(r, "Input: f|formula with x; f,g,h allowed");
        int slot = fields[0][0] - 'f';
        if (!aero_define(s, slot, fields[1], &e))
            return aero_problem(r, "Invalid formula, unknown function or recursion");
        aero_line(r, "%c(x): %s", 'f' + slot, fields[1][0] ? "saved" : "cleared");
        aero_line(r, "In calculator and graph: %c(expression)", 'f' + slot);
        r->ok = 1;
        return 1;
    }
    if (t == AE_DIMENSIONS) {
        char unit[160], error[160];
        int dimensions[8];
        double value;
        if (!aero_dimensions(input, &value, dimensions, unit, sizeof(unit), error, sizeof(error)))
            return aero_problem(r, error);
        aero_line(r, "Result ~= %.15g", value);
        aero_line(r, "Units: %s", unit);
        aero_line(r, "Dimensions checked");
        aero_line(r, "K: absolute scale; use converter for C/F");
        r->value = value;
        r->ok = 1;
        r->approx = 1;
        return 1;
    }
    char expanded[CL_EXPR];
    CLError e;
    if (!aero_expand(input, s, expanded, &e))
        return aero_problem(r, cl_error_text(e.code));
    switch (t) {
    case AE_STEPS:
        return aero_steps(expanded, ctx, r);
    case AE_LINEAR:
    case AE_QUADRATIC:
        return equation_steps(t, expanded, ctx, r);
    case AE_VECTOR:
        return vector(expanded, ctx, r);
    case AE_REGRESSION:
        return regression(expanded, ctx, r);
    case AE_FORMULA:
        return formula(expanded, ctx, r);
    case AE_BINOMIAL:
    case AE_NORMAL:
        return probability(t, expanded, ctx, r);
    case AE_IEEE:
        return ieee(expanded, ctx, r);
    case AE_EXPAND:
    case AE_DERIVATIVE: {
        char result[CL_EXPR], error[160];
        if (!aero_symbolic(expanded, ctx, t == AE_DERIVATIVE, result, error, sizeof(error)))
            return aero_problem(r, error);
        aero_line(r, t == AE_DERIVATIVE ? "Derivative with respect to x:" : "Collected polynomial:");
        /* Complete expression is split into readable, exportable chunks. */
        for (size_t i = 0; i < strlen(result); i += 62)
            aero_line(r, "%.62s", result + i);
        if (t == AE_DERIVATIVE) {
            aero_line(r, "The rule respects DEG/RAD/GRAD");
            aero_line(r, "Valid in the original formula's domain,");
            aero_line(r, "where the function is differentiable.");
        }
        r->ok = 1;
        return 1;
    }
    case AE_ANALYZE:
        return aero_analyze(expanded, ctx, r);
    default:
        return aero_problem(r, "Unknown tool");
    }
}
