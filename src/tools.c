#include "tools.h"
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "l10n.h"
static void line(CLToolResult *r, const char *format, ...) {
    if (r->count >= CL_TOOL_LINES)
        return;
    va_list ap;
    va_start(ap, format);
    vsnprintf(r->line[r->count++], 160, format, ap);
    va_end(ap);
}
static int problem(CLToolResult *r, const char *s) {
    r->ok = 0;
    r->count = 0;
    line(r, "%s", s);
    return 0;
}
const char *cl_tool_name(int t) {
    static const char *n[] = {
        "Quadratic equation", "2 x 2 system",       "3 x 3 system",
        "Matrix determinant", "Inverse matrix",    "Statistics",
        "Root on an interval",  "Derivative at a point", "Definite integral"};
    return t >= 0 && t < CL_TOOL_COUNT ? L(n[t]) : L("Tool");
}
const char *cl_tool_hint(int t) {
    static const char *h[] = {"a;b;c for a*x^2+b*x+c=0",        "a11;a12;b1;a21;a22;b2",
                              "a11;a12;a13;b1; ... (12 numbers)", "n;row-major elements (n=1..4)",
                              "n;row-major elements (n=1..4)", "Numbers separated by ; (up to 64)",
                              "f(x)|left|right; sign change", "f(x)|point x",
                              "f(x)|lower|upper bound"};
    return t >= 0 && t < CL_TOOL_COUNT ? L(h[t]) : "";
}
const char *cl_tool_example(int t) {
    static const char *e[] = {"1;-3;2",    "2;1;5;1;-1;1", "1;1;1;6;2;-1;1;3;1;2;-1;2",
                              "2;1;2;3;4", "2;1;2;3;4",    "1;2;3;4;5",
                              "x^2-2|0|2", "sin(x)|30",    "x^2|0|1"};
    return t >= 0 && t < CL_TOOL_COUNT ? e[t] : "";
}
int cl_split_values(const char *s, char sep, char out[][CL_EXPR], int max) {
    int depth = 0, n = 0, start = 0;
    if (strlen(s) >= CL_EXPR)
        return -1;
    for (int i = 0;; ++i) {
        char c = s[i];
        if (c == '(')
            ++depth;
        else if (c == ')')
            --depth;
        if (depth < 0)
            return -1;
        if ((c == sep && depth == 0) || !c) {
            if (n >= max || i == start)
                return -1;
            int len = i - start;
            memcpy(out[n], s + start, (size_t)len);
            out[n++][len] = 0;
            start = i + 1;
            if (!c)
                break;
        }
    }
    return depth ? -1 : n;
}
static int values(const char *s, const CLContext *ctx, double *v, int expected, int max,
                  CLToolResult *r) {
    /* Keep this bounded workspace off the PSP thread stack. */
    char (*parts)[CL_EXPR] = malloc((size_t)max * CL_EXPR);
    if (!parts)
        return problem(r, "Out of memory");
    int n = cl_split_values(s, ';', parts, max);
    if (n < 0 || (expected && n != expected)) {
        free(parts);
        return problem(r, "Check value count and ; separators");
    }
    for (int i = 0; i < n; ++i) {
        CLValue x;
        CLError e;
        if (!cl_calculate(parts[i], ctx, &x, &e) || x.im) {
            free(parts);
            return problem(r, "Each field needs a real number");
        }
        v[i] = x.re;
    }
    free(parts);
    return n;
}
static int quad(const char *input, const CLContext *ctx, CLToolResult *r) {
    double v[3];
    if (!values(input, ctx, v, 3, 3, r))
        return 0;
    double scale = fmax(fabs(v[0]), fmax(fabs(v[1]), fabs(v[2])));
    if (scale == 0)
        return problem(r, "Any x: all coefficients are zero");
    double a = v[0] / scale, b = v[1] / scale, c = v[2] / scale;
    if (a == 0) {
        if (b == 0)
            return problem(r, "No solutions: nonzero constant term");
        r->value = -c / b;
        line(r, "Linear: x ~= %.12g", r->value);
    } else {
        double d = b * b - 4 * a * c;
        if (d < 0) {
            double re = -b / (2 * a), im = sqrt(-d) / fabs(2 * a);
            line(r, "x1 ~= %.12g + %.12gi", re, im);
            line(r, "x2 ~= %.12g - %.12gi", re, im);
        } else {
            double q = -0.5 * (b + copysign(sqrt(d), b));
            double x1 = q / a, x2 = q == 0 ? -b / (2 * a) : c / q;
            if (!isfinite(x1) || !isfinite(x2))
                return problem(r, "Roots outside numerical range");
            r->value = x1;
            line(r, "x1 ~= %.14g", x1);
            line(r, "x2 ~= %.14g", x2);
            double residual = fmax(fabs((a * x1 + b) * x1 + c), fabs((a * x2 + b) * x2 + c));
            r->error = residual;
            line(r, "Residual (normalized) ~= %.3g", residual);
        }
    }
    r->ok = r->approx = 1;
    return 1;
}
static int matrix(const char *input, const CLContext *ctx, CLToolResult *r, int t) {
    double v[17], a[4][8] = {{0}}, original[4][4] = {{0}};
    int n, count;
    if (t == 1 || t == 2) {
        n = t + 1;
        count = values(input, ctx, v, n * (n + 1), 17, r);
    } else {
        count = values(input, ctx, v, 0, 17, r);
        if (!count)
            return 0;
        if (v[0] != trunc(v[0]) || v[0] < 1 || v[0] > 4)
            return problem(r, "Matrix size: integer 1..4");
        n = (int)v[0];
        if (count != 1 + n * n)
            return problem(r, "Exactly n*n elements are required after n");
    }
    if (!count)
        return 0;
    int inverse = t == 4, system = t == 1 || t == 2;
    double norm = 0, det = 1;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            a[i][j] = system ? v[i * (n + 1) + j] : v[1 + i * n + j];
            original[i][j] = a[i][j];
            norm = fmax(norm, fabs(a[i][j]));
        }
    if (norm == 0)
        return problem(r, "Singular matrix / no unique solution");
    for (int i = 0; i < n; ++i) {
        if (system)
            a[i][n] = v[i * (n + 1) + n] / norm;
        else if (inverse)
            a[i][n + i] = 1 / norm;
        for (int j = 0; j < n; ++j)
            a[i][j] /= norm;
    }
    int cols = system ? n + 1 : inverse ? 2 * n : n;
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        for (int i = col + 1; i < n; ++i)
            if (fabs(a[i][col]) > fabs(a[pivot][col]))
                pivot = i;
        if (a[pivot][col] == 0 || (t != 3 && fabs(a[pivot][col]) < 1e-14)) {
            if (t == 3) {
                r->value = 0;
                line(r, "det = 0 (within precision)");
                r->ok = r->approx = 1;
                return 1;
            }
            return problem(r, "Matrix is singular or ill-conditioned");
        }
        if (pivot != col) {
            for (int j = 0; j < cols; ++j) {
                double tmp = a[col][j];
                a[col][j] = a[pivot][j];
                a[pivot][j] = tmp;
            }
            det = -det;
        }
        double p = a[col][col];
        det *= p;
        for (int j = 0; j < cols; ++j)
            a[col][j] /= p;
        for (int i = 0; i < n; ++i)
            if (i != col) {
                double factor = a[i][col];
                for (int j = 0; j < cols; ++j)
                    a[i][j] -= factor * a[col][j];
            }
    }
    if (t == 3) {
        for (int i = 0; i < n; ++i)
            det *= norm;
        if (!isfinite(det))
            return problem(r, "Determinant outside numerical range");
        r->value = det;
        line(r, "det ~= %.14g", det);
    } else if (system) {
        double residual = 0;
        for (int i = 0; i < n; ++i) {
            if (!isfinite(a[i][n]))
                return problem(r, "Solution outside numerical range");
            line(r, "x%d ~= %.14g", i + 1, a[i][n]);
            double sum = 0;
            for (int j = 0; j < n; ++j)
                sum += original[i][j] / norm * a[j][n];
            residual = fmax(residual, fabs(sum - v[i * (n + 1) + n] / norm));
        }
        r->value = a[0][n];
        r->error = residual;
        line(r, "Residual (normalized) ~= %.3g", residual);
    } else {
        double residual = 0;
        for (int i = 0; i < n; ++i) {
            char s[150] = {0};
            size_t used = 0;
            for (int j = 0; j < n; ++j) {
                if (!isfinite(a[i][n + j]))
                    return problem(r, "Element outside numerical range");
                int written =
                    snprintf(s + used, sizeof(s) - used, "%s%.9g", j ? "  " : "", a[i][n + j]);
                if (written > 0)
                    used += (size_t)written;
            }
            line(r, "%s", s);
        }
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                double sum = 0;
                for (int k = 0; k < n; ++k)
                    sum += original[i][k] * a[k][n + j];
                residual = fmax(residual, fabs(sum - (i == j)));
            }
        r->value = a[0][n];
        r->error = residual;
        line(r, "Residual A*A^-1 ~= %.3g", residual);
    }
    r->ok = r->approx = 1;
    return 1;
}
static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static int statistics(const char *input, const CLContext *ctx, CLToolResult *r) {
    double v[64];
    int n = values(input, ctx, v, 0, 64, r);
    if (!n)
        return 0;
    double scale = 0;
    for (int i = 0; i < n; ++i)
        scale = fmax(scale, fabs(v[i]));
    double mean = 0, m2 = 0;
    for (int i = 0; i < n; ++i) {
        double x = scale ? v[i] / scale : 0, d = x - mean;
        mean += d / (i + 1);
        m2 += d * (x - mean);
    }
    qsort(v, (size_t)n, sizeof(*v), cmp_double);
    double median = n & 1 ? v[n / 2] : v[n / 2 - 1] / 2 + v[n / 2] / 2;
    mean *= scale;
    double pop = sqrt(fmax(0, m2 / n)) * scale,
           sample = n > 1 ? sqrt(fmax(0, m2 / (n - 1))) * scale : 0;
    if (!isfinite(mean) || !isfinite(pop) || !isfinite(sample))
        return problem(r, "Statistics outside numerical range");
    line(r, "n = %d", n);
    line(r, "Mean ~= %.14g", mean);
    line(r, "Median ~= %.14g", median);
    line(r, "min = %.12g; max = %.12g", v[0], v[n - 1]);
    line(r, "sigma (divisor n) ~= %.12g", pop);
    if (n > 1)
        line(r, "s (divisor n-1) ~= %.12g", sample);
    else
        line(r, "s: at least 2 values required");
    r->value = mean;
    r->ok = r->approx = 1;
    return 1;
}
typedef struct {
    CLProgram p;
    CLContext ctx;
    int calls, budget, failed, opbudget;
} Numeric;
static double fx(Numeric *n, double x) {
    if (++n->calls > n->budget || !isfinite(x)) {
        n->failed = 1;
        return 0;
    }
    n->ctx.x = cl_real(x);
    CLValue v;
    CLError e;
    if (!cl_eval_budget(&n->p, &n->ctx, &v, &e, &n->opbudget) || v.im) {
        n->failed = 1;
        return 0;
    }
    return v.re;
}
/* Gauss-Kronrod 7/15 coefficients: mathematical quadrature constants,
   independently implemented; reference https://www.netlib.org/quadpack/dqk15.f. */
typedef struct {
    double a, b, value, error;
} Interval;
static Interval gk15(Numeric *n, double a, double b) {
    static const double x[] = {.9914553711208126392, .9491079123427585245, .8648644233597690728,
                               .7415311855993944399, .5860872354676911303, .4058451513773971669,
                               .2077849550078984676};
    static const double wk[] = {.02293532201052922496, .06309209262997855329, .1047900103222501838,
                                .1406532597155259187,  .1690047266392679028,  .1903505780647854099,
                                .2044329400752988924,  .2094821410847278280};
    static const double wg[] = {.1294849661688696933, .2797053914892766679, .3818300505051189449,
                                .4179591836734693878};
    double mid = a / 2 + b / 2, half = (b - a) / 2, fc = fx(n, mid), kron = wk[7] * fc,
           gauss = wg[3] * fc, absum = wk[7] * fabs(fc);
    double low[7], high[7];
    for (int j = 0; j < 7; ++j) {
        double dx = half * x[j];
        low[j] = fx(n, mid - dx);
        high[j] = fx(n, mid + dx);
        double pair = low[j] + high[j];
        kron += wk[j] * pair;
        absum += wk[j] * (fabs(low[j]) + fabs(high[j]));
        if (j & 1)
            gauss += wg[j / 2] * pair;
    }
    double mean = kron / 2, asc = wk[7] * fabs(fc - mean);
    for (int j = 0; j < 7; ++j)
        asc += wk[j] * (fabs(low[j] - mean) + fabs(high[j] - mean));
    asc *= fabs(half);
    absum *= fabs(half);
    double err = fabs((kron - gauss) * half);
    if (asc > 0 && err > 0)
        err = asc * fmin(1, pow(200 * err / asc, 1.5));
    err = fmax(err, 50 * DBL_EPSILON * absum);
    return (Interval){a, b, kron * half, err};
}
static int integrate(Numeric *n, double a, double b, double *result, double *error) {
    if (a == b) {
        *result = *error = 0;
        return 1;
    }
    double width = b - a;
    if (!isfinite(width))
        return 0;
    Interval cells[128];
    int count = 8;
    for (int i = 0; i < 8; ++i)
        cells[i] = gk15(n, a + width * (i / 8.0), a + width * ((i + 1) / 8.0));
    while (!n->failed) {
        double sum = 0, err = 0;
        int worst = 0;
        for (int i = 0; i < count; ++i) {
            sum += cells[i].value;
            err += cells[i].error;
            if (cells[i].error > cells[worst].error)
                worst = i;
        }
        if (!isfinite(sum) || !isfinite(err))
            return 0;
        *result = sum;
        *error = err;
        if (err <= 1e-10 + 1e-9 * fabs(sum))
            return 1;
        if (count >= 128)
            return 0;
        Interval old = cells[worst];
        double mid = old.a / 2 + old.b / 2;
        if (mid == old.a || mid == old.b)
            return 0;
        cells[worst] = gk15(n, old.a, mid);
        cells[count++] = gk15(n, mid, old.b);
    }
    return 0;
}
static int numeric(int t, const char *input, const CLContext *ctx, CLToolResult *r) {
    char part[3][CL_EXPR];
    int count = cl_split_values(input, '|', part, 3), need = t == 7 ? 2 : 3;
    if (count != need)
        return problem(r, "Separate function and bounds with |");
    Numeric *n = calloc(1, sizeof(*n));
    if (!n)
        return problem(r, "Out of memory");
    n->ctx = *ctx;
    CLError error;
    if (!cl_compile(part[0], &n->p, &error)) {
        free(n);
        return problem(r, cl_error_text(error.code));
    }
    n->budget = 4096;
    n->opbudget = 300000;
    if (n->p.count > 120)
        n->budget = 2000;
    CLValue av, bv = cl_real(0);
    if (!cl_calculate(part[1], ctx, &av, &error) || av.im ||
        (count == 3 && (!cl_calculate(part[2], ctx, &bv, &error) || bv.im))) {
        free(n);
        return problem(r, "Bounds must be real numbers");
    }
    double a = av.re, b = bv.re, result = 0, err = 0;
    int converged = 0;
    if (t == 6) {
        if (a >= b) {
            free(n);
            return problem(r, "Left bound must be smaller than right bound");
        }
        double fa = fx(n, a), fb = fx(n, b);
        if (fa == 0 && !n->failed) {
            result = a;
            converged = 1;
        } else if (fb == 0 && !n->failed) {
            result = b;
            converged = 1;
        } else if (signbit(fa) == signbit(fb)) {
            free(n);
            return problem(r, "f(x) must change sign between the bounds");
        }
        double scale = fmax(fabs(fa), fabs(fb));
        for (int i = 0; i < 160 && !converged && !n->failed; ++i) {
            double mid = a / 2 + b / 2, fm = fx(n, mid);
            result = mid;
            err = fabs(b - a) / 2;
            if (fm == 0 || (err <= 1e-11 * (1 + fabs(mid)) && fabs(fm) <= 1e-8 * fmax(1, scale))) {
                converged = 1;
                break;
            }
            if (mid == a || mid == b)
                break;
            if (signbit(fm) == signbit(fa)) {
                a = mid;
                fa = fm;
            } else {
                b = mid;
                fb = fm;
            }
        }
        if (converged)
            line(r, "x ~= %.14g; |f(x)| ~= %.3g", result, fabs(fx(n, result)));
    } else if (t == 7) {
        double h = pow(DBL_EPSILON, 0.2) * fmax(1, fabs(a)), previous = 0;
        for (int i = 0; i < 10 && !n->failed; ++i) {
            double fm2 = fx(n, a - 2 * h), fm = fx(n, a - h), fp = fx(n, a + h),
                   fp2 = fx(n, a + 2 * h);
            result = ((fm2 - fp2) + 8 * (fp - fm)) / (12 * h);
            if (i) {
                err = fabs(result - previous) / 15;
                if (err <= 1e-9 * (1 + fabs(result))) {
                    double f0 = fx(n, a), left = (f0 - fm) / h, right = (fp - f0) / h;
                    if (fabs(left - right) <= 1e-3 * (1 + fabs(result))) {
                        converged = 1;
                        break;
                    }
                }
            }
            previous = result;
            h /= 2;
        }
        if (converged)
            line(r, "f'(%.10g) ~= %.14g", a, result);
    } else {
        converged = integrate(n, a, b, &result, &err);
        if (converged)
            line(r, "Integral ~= %.14g", result);
    }
    int failed = n->failed, calls = n->calls;
    free(n);
    if (failed)
        return problem(r, "Function error or evaluation limit");
    if (!converged || !isfinite(result) || !isfinite(err))
        return problem(r, "Calculation did not converge to required precision");
    line(r, "Error estimate ~= %.3g", err);
    line(r, "Function evaluations: %d", calls);
    if (t == 8)
        line(r, "For smooth functions without discontinuities");
    r->value = result;
    r->error = err;
    r->ok = r->approx = 1;
    return 1;
}
int cl_tool_run(int t, const char *input, const CLContext *ctx, CLToolResult *r) {
    memset(r, 0, sizeof(*r));
    if (t == 0)
        return quad(input, ctx, r);
    if (t >= 1 && t <= 4)
        return matrix(input, ctx, r, t);
    if (t == 5)
        return statistics(input, ctx, r);
    if (t >= 6 && t <= 8)
        return numeric(t, input, ctx, r);
    return problem(r, "Unknown tool");
}
typedef struct {
    const char *name;
    double factor, offset;
} Unit;
typedef struct {
    const char *name;
    const Unit *u;
    int count;
} Group;
static const Unit length[] = {{"m", 1, 0},          {"km", 1000, 0},    {"cm", .01, 0},
                              {"mm", .001, 0},      {"inch", .0254, 0}, {"ft", .3048, 0},
                              {"mile", 1609.344, 0}};
static const Unit mass[] = {{"kg", 1, 0},       {"g", .001, 0},       {"mg", .000001, 0},
                            {"tonne", 1000, 0}, {"lb", .45359237, 0}, {"oz", .028349523125, 0}};
static const Unit time_u[] = {
    {"s", 1, 0}, {"ms", .001, 0}, {"min", 60, 0}, {"hour", 3600, 0}, {"day", 86400, 0}};
static const Unit temperature[] = {
    {"C", 1, 273.15}, {"F", 5.0 / 9, 255.37222222222222}, {"K", 1, 0}};
static const Unit angle_u[] = {
    {"deg", CL_PI / 180, 0}, {"rad", 1, 0}, {"grad", CL_PI / 200, 0}, {"turn", 2 * CL_PI, 0}};
static const Unit speed[] = {
    {"m/s", 1, 0}, {"km/h", 1.0 / 3.6, 0}, {"mph", .44704, 0}, {"knot", 1852.0 / 3600, 0}};
static const Unit area[] = {{"m2", 1, 0},
                            {"cm2", .0001, 0},
                            {"km2", 1000000, 0},
                            {"hectare", 10000, 0},
                            {"acre", 4046.8564224, 0}};
static const Unit volume[] = {{"m3", 1, 0},
                              {"litre", .001, 0},
                              {"ml", .000001, 0},
                              {"gal US", .003785411784, 0},
                              {"gal UK", .00454609, 0}};
static const Unit energy[] = {
    {"J", 1, 0},         {"kJ", 1000, 0},         {"Wh", 3600, 0},
    {"kWh", 3600000, 0}, {"cal (IT)", 4.1868, 0}, {"eV", 1.602176634e-19, 0}};
static const Unit power[] = {
    {"W", 1, 0}, {"kW", 1000, 0}, {"MW", 1000000, 0}, {"hp mech", 745.6998715822702, 0}};
static const Unit pressure[] = {{"Pa", 1, 0},
                                {"kPa", 1000, 0},
                                {"bar", 100000, 0},
                                {"atm", 101325, 0},
                                {"psi", 6894.757293168361, 0}};
static const Unit data_u[] = {{"byte", 1, 0},      {"bit", .125, 0},      {"kB", 1000, 0},
                              {"MB", 1000000, 0},  {"GB", 1e9, 0},        {"KiB", 1024, 0},
                              {"MiB", 1048576, 0}, {"GiB", 1073741824, 0}};
#define GROUP(name, array) {name, array, (int)(sizeof(array) / sizeof(array[0]))}
static const Group groups[] = {GROUP("Length", length),      GROUP("Mass", mass),
                               GROUP("Time", time_u),      GROUP("Temperature", temperature),
                               GROUP("Angles", angle_u),      GROUP("Speed", speed),
                               GROUP("Area", area),      GROUP("Volume", volume),
                               GROUP("Energy", energy),    GROUP("Power", power),
                               GROUP("Pressure", pressure), GROUP("Data", data_u)};
int cl_unit_groups(void) { return (int)(sizeof(groups) / sizeof(groups[0])); }
const char *cl_unit_group_name(int g) {
    return g >= 0 && g < cl_unit_groups() ? L(groups[g].name) : "?";
}
int cl_unit_count(int g) { return g >= 0 && g < cl_unit_groups() ? groups[g].count : 0; }
const char *cl_unit_name(int g, int u) {
    return u >= 0 && u < cl_unit_count(g) ? groups[g].u[u].name : "?";
}
int cl_convert_unit(int g, int from, int to, double x, double *out) {
    if (!isfinite(x) || from < 0 || to < 0 || from >= cl_unit_count(g) || to >= cl_unit_count(g))
        return 0;
    const Unit *a = &groups[g].u[from], *b = &groups[g].u[to];
    double result = (x * a->factor + a->offset - b->offset) / b->factor;
    if (!isfinite(result))
        return 0;
    *out = result;
    return 1;
}
static uint64_t mask_for(int bits) { return bits == 64 ? UINT64_MAX : (UINT64_C(1) << bits) - 1; }
static int parse_u(const char *s, int bits, uint64_t *out) {
    while (isspace((unsigned char)*s))
        ++s;
    int negative = 0;
    if (*s == '-' || *s == '+') {
        negative = *s == '-';
        ++s;
    }
    int base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    } else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        base = 2;
        s += 2;
    } else if (s[0] == '0' && (s[1] == 'o' || s[1] == 'O')) {
        base = 8;
        s += 2;
    }
    if (!isxdigit((unsigned char)*s))
        return 0;
    errno = 0;
    char *end;
    uint64_t v = strtoull(s, &end, base);
    while (isspace((unsigned char)*end))
        ++end;
    uint64_t max = mask_for(bits);
    if (*end || errno == ERANGE || v > max || (negative && v > (UINT64_C(1) << (bits - 1))))
        return 0;
    *out = negative ? (-v) & max : v;
    return 1;
}
int cl_programmer(const char *input, int bits, int op, uint64_t *out, char *error, size_t cap) {
    if (bits != 8 && bits != 16 && bits != 32 && bits != 64) {
        snprintf(error, cap, "Bit width: 8/16/32/64");
        return 0;
    }
    char parts[2][CL_EXPR];
    int n = cl_split_values(input, ';', parts, 2);
    uint64_t a, b = 0;
    if (n < 1 || !parse_u(parts[0], bits, &a) ||
        (op != 0 && op != 4 && (n != 2 || !parse_u(parts[1], bits, &b))) ||
        ((op == 0 || op == 4) && n != 1)) {
        snprintf(error, cap, "Integers: DEC, 0xHEX, 0bBIN, 0oOCT");
        return 0;
    }
    uint64_t r = 0, mask = mask_for(bits);
    switch (op) {
    case 0:
        r = a;
        break;
    case 1:
        r = a & b;
        break;
    case 2:
        r = a | b;
        break;
    case 3:
        r = a ^ b;
        break;
    case 4:
        r = ~a;
        break;
    case 5:
    case 6:
        if (b >= (uint64_t)bits) {
            snprintf(error, cap, "Shift: from 0 to %d", bits - 1);
            return 0;
        }
        r = op == 5 ? a << b : a >> b;
        break;
    case 7:
        r = a + b;
        break;
    case 8:
        r = a - b;
        break;
    case 9:
        r = a * b;
        break;
    case 10:
    case 11:
        if (!b) {
            snprintf(error, cap, "Division by zero");
            return 0;
        }
        r = op == 10 ? a / b : a % b;
        break;
    default:
        snprintf(error, cap, "Unknown operation");
        return 0;
    }
    *out = r & mask;
    error[0] = 0;
    return 1;
}
void cl_base_format(uint64_t v, int bits, int base, int is_signed, char *out, size_t cap) {
    if (!cap)
        return;
    if (bits != 8 && bits != 16 && bits != 32 && bits != 64) {
        out[0] = 0;
        return;
    }
    v &= mask_for(bits);
    if (base == 10) {
        if (is_signed && (v & (UINT64_C(1) << (bits - 1)))) {
            uint64_t mag = (-v) & mask_for(bits);
            snprintf(out, cap, "-%llu", (unsigned long long)mag);
        } else
            snprintf(out, cap, "%llu", (unsigned long long)v);
        return;
    }
    if (base != 2 && base != 8 && base != 16) {
        out[0] = 0;
        return;
    }
    char tmp[65];
    int n = 0;
    do {
        tmp[n++] = "0123456789ABCDEF"[v % (uint64_t)base];
        v /= (uint64_t)base;
    } while (v && n < 64);
    int limit = (int)cap - 1;
    if (n > limit)
        n = limit;
    for (int i = 0; i < n; ++i)
        out[i] = tmp[n - i - 1];
    out[n] = 0;
}
