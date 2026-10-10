#include "selftest.h"
#include "calc.h"
#include "tools.h"
#include "aero.h"
#include <math.h>
#include <stdio.h>
#include "l10n.h"
int cl_selftest(int *total) {
    struct Test {
        const char *expr;
        double re, im;
        int angle, complex_mode, error;
    };
    static const struct Test tests[] = {{"-2^2", -4, 0, 0, 0, 0},
                                        {"(-2)^2", 4, 0, 0, 0, 0},
                                        {"2^3^2", 512, 0, 0, 0, 0},
                                        {"2^-3", .125, 0, 0, 0, 0},
                                        {"1/3+1/6", .5, 0, 0, 0, 0},
                                        {"0.1+0.2", .3, 0, 0, 0, 0},
                                        {"sin(30)", .5, 0, 0, 0, 0},
                                        {"cos(60)", .5, 0, 0, 0, 0},
                                        {"tan(45)", 1, 0, 0, 0, 0},
                                        {"sin(pi/2)", 1, 0, 1, 0, 0},
                                        {"sin(100)", 1, 0, 2, 0, 0},
                                        {"asin(.5)", 30, 0, 0, 0, 0},
                                        {"ln(e)", 1, 0, 0, 0, 0},
                                        {"log(1000)", 3, 0, 0, 0, 0},
                                        {"log2(1024)", 10, 0, 0, 0, 0},
                                        {"sqrt(4/9)", 2.0 / 3, 0, 0, 0, 0},
                                        {"root(-8;3)", -2, 0, 0, 0, 0},
                                        {"sinh(1)", 1.1752011936438014, 0, 0, 0, 0},
                                        {"atanh(.5)", .54930614433405485, 0, 0, 0, 0},
                                        {"sqrt(-1)", 0, 1, 0, 1, 0},
                                        {"ln(-1)", 0, CL_PI, 0, 1, 0},
                                        {"(1+i)/(1-i)", 0, 1, 0, 1, 0},
                                        {"sin(i)", 0, 1.1752011936438014, 1, 1, 0},
                                        {"conj(2+3i)", 2, -3, 0, 1, 0},
                                        {"20!", 2432902008176640000.0, 0, 0, 0, 0},
                                        {"ncr(52;5)", 2598960, 0, 0, 0, 0},
                                        {"dms(-30;15;0)", -30.25, 0, 0, 0, 0},
                                        {"E+e", 7 + 2.718281828459045, 0, 0, 0, 0},
                                        {"1/0", 0, 0, 0, 0, CL_ZERO},
                                        {"tan(90)", 0, 0, 0, 0, CL_DOMAIN},
                                        {"0^0", 0, 0, 0, 0, CL_DOMAIN},
                                        {"sqrt(-1)", 0, 0, 0, 0, CL_DOMAIN},
                                        {"exp(-1000)", 0, 0, 0, 0, CL_PRECISION},
                                        {"sin()", 0, 0, 0, 0, CL_ARGUMENTS}};
    int passed = 0;
    CLContext ctx;
    cl_context_init(&ctx);
    ctx.mem[4] = cl_integer(7);
    *total = (int)(sizeof(tests) / sizeof(tests[0])) + 4;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        const struct Test *t = &tests[i];
        ctx.angle = (CLAngle)t->angle;
        ctx.complex_mode = t->complex_mode;
        CLValue v;
        CLError e;
        int ok = cl_calculate(t->expr, &ctx, &v, &e), match;
        if (t->error)
            match = !ok && e.code == t->error;
        else
            match = ok && fabs(v.re - t->re) <= 1e-11 * fmax(1, fabs(t->re)) &&
                    fabs(v.im - t->im) <= 1e-11 * fmax(1, fabs(t->im));
        if (match)
            ++passed;
        else
            printf("CALCLAB SELFTEST FAIL %s code=%d\n", t->expr, e.code);
    }
    ctx.angle = CL_DEG;
    ctx.complex_mode = 0;
    CLToolResult r;
    if (cl_tool_run(3, "2;1;2;3;4", &ctx, &r) && fabs(r.value + 2) < 1e-10)
        ++passed;
    if (cl_tool_run(7, "x^3|2", &ctx, &r) && fabs(r.value - 12) < 1e-8)
        ++passed;
    if (cl_tool_run(8, "x^2|0|1", &ctx, &r) && fabs(r.value - 1.0 / 3) < 1e-10)
        ++passed;
    uint64_t n;
    char err[160];
    if (cl_programmer("255;1", 8, 7, &n, err, sizeof(err)) && n == 0)
        ++passed;
    AeroState state;
    aero_defaults(&state);
    *total += AERO_LABS + AERO_MATRIX_OPS + 3;
    for (int i = 0; i < AERO_LABS; ++i)
        if (aero_lab_run(i, aero_lab_example(i), &ctx, &state, &r) && r.ok)
            ++passed;
    for (int i = 0; i < AERO_MATRIX_OPS; ++i)
        if (aero_matrix_run(&state, i, &r) && r.ok)
            ++passed;
    CLValue v;
    CLError e;
    if (aero_calculate("f(2)", &state, &ctx, &v, &e) && v.exact && v.num == 9 && v.den == 1)
        ++passed;
    if (fabs(aero_binomial_pmf(10, 3, .5) - .1171875) < 1e-13)
        ++passed;
    if (fabs(aero_normal_interval(0, 1, -1, 1) - .6826894921370859) < 1e-13)
        ++passed;
    printf("CALCAERO SELFTEST %d/%d\n", passed, *total);
    return passed;
}
