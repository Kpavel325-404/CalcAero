#ifndef CALCLAB_CALC_H
#define CALCLAB_CALC_H
#include <stddef.h>
#include <stdint.h>
#define CL_EXPR 512
#define CL_NODES 256
#define CL_PI 3.141592653589793238462643383279502884
/* Public syntax tree tags, shared by the equation renderer and symbolic tools. */
enum { N_VALUE, N_VAR, N_NEG, N_ADD, N_SUB, N_MUL, N_DIV, N_POW, N_FACT, N_PERCENT, N_FUNC };
enum {
    F_SIN, F_COS, F_TAN, F_ASIN, F_ACOS, F_ATAN, F_ATAN2,
    F_SINH, F_COSH, F_TANH, F_ASINH, F_ACOSH, F_ATANH,
    F_SQRT, F_CBRT, F_ROOT, F_LN, F_LOG, F_LOG2, F_EXP,
    F_ABS, F_FLOOR, F_CEIL, F_ROUND, F_TRUNC, F_SIGN, F_FRAC,
    F_INV, F_FACT, F_NCR, F_NPR, F_GCD, F_LCM, F_MOD, F_MIN,
    F_MAX, F_CLAMP, F_RE, F_IM, F_CONJ, F_ARG, F_POLAR,
    F_D2R, F_R2D, F_D2G, F_G2D, F_DMS, F_POW, F_HYPOT
};
typedef struct {
    double re, im;
    int64_t num, den;
    int exact;
} CLValue;
typedef enum { CL_DEG, CL_RAD, CL_GRAD } CLAngle;
typedef enum {
    CL_OK,
    CL_SYNTAX,
    CL_UNKNOWN,
    CL_ARGUMENTS,
    CL_DOMAIN,
    CL_ZERO,
    CL_OVERFLOW,
    CL_LIMIT,
    CL_PRECISION,
    CL_COMPLEX
} CLErrorCode;
typedef struct {
    int code, pos;
} CLError;
typedef struct {
    CLAngle angle;
    int complex_mode;
    CLValue ans, mem[6], x;
} CLContext;
typedef struct {
    int kind, op, a, b, c, pos;
    CLValue value;
} CLNode;
typedef struct {
    CLNode nodes[CL_NODES];
    int count, root;
} CLProgram;
CLValue cl_real(double x);
CLValue cl_integer(int64_t x);
CLValue cl_fraction(int64_t n, int64_t d);
void cl_context_init(CLContext *ctx);
int cl_compile(const char *s, CLProgram *p, CLError *error);
int cl_eval_budget(const CLProgram *p, const CLContext *ctx, CLValue *out, CLError *error,
                   int *budget);
int cl_eval(const CLProgram *p, const CLContext *ctx, CLValue *out, CLError *error);
int cl_calculate(const char *s, const CLContext *ctx, CLValue *out, CLError *error);
void cl_format(CLValue v, int digits, int style, int fraction, char *s, size_t cap);
const char *cl_error_text(int code);
int cl_value_valid(CLValue v);
const char *cl_function_name(int code);
double cl_to_rad(double x, CLAngle angle);
double cl_from_rad(double x, CLAngle angle);
#endif
