#ifndef CALCAERO_EXTENSIONS_H
#define CALCAERO_EXTENSIONS_H
#include "calc.h"
#include "tools.h"
#define AERO_VERSION "1.3"
#define AERO_FUNCTIONS 3
#define AERO_CURVES 3
#define AERO_FORMULAS 12
#define AERO_STUDY_TOPICS 6
enum {
    AE_STEPS, AE_LINEAR, AE_QUADRATIC, AE_VECTOR, AE_REGRESSION,
    AE_DIMENSIONS, AE_FORMULA, AE_FUNCTION, AE_BINOMIAL, AE_NORMAL,
    AE_IEEE, AE_EXPAND, AE_DERIVATIVE, AE_ANALYZE, AERO_LABS
};
enum { AM_ADD, AM_SUB, AM_MUL, AM_TRANSPOSE, AM_RANK, AM_DET, AM_INVERSE, AERO_MATRIX_OPS };
typedef struct {
    char function[AERO_FUNCTIONS][CL_EXPR];
    char graph[2][CL_EXPR];
    int pretty, matrix_n;
    double matrix[2][16];
    uint32_t study_seed, study_topic, study_total, study_correct;
} AeroState;
void aero_defaults(AeroState *s);
int aero_state_load(const char *dir, AeroState *s);
int aero_state_save(const char *dir, const AeroState *s);
int aero_expand(const char *text, const AeroState *s, char out[CL_EXPR], CLError *e);
int aero_compile(const char *text, const AeroState *s, CLProgram *p, CLError *e);
int aero_calculate(const char *text, const AeroState *s, const CLContext *ctx,
                   CLValue *out, CLError *e);
int aero_define(AeroState *s, int slot, const char *body, CLError *e);
const char *aero_lab_name(int tool);
const char *aero_lab_hint(int tool);
const char *aero_lab_example(int tool);
int aero_lab_run(int tool, const char *input, const CLContext *ctx,
                 AeroState *s, CLToolResult *r);
int aero_matrix_run(const AeroState *s, int op, CLToolResult *r);
const char *aero_matrix_name(int op);
const char *aero_formula_name(int id);
const char *aero_formula_example(int id);
const char *aero_formula_variables(int id);
int aero_dimensions(const char *input, double *value, int dimensions[8],
                     char *unit, size_t cap, char *error, size_t error_cap);
int aero_symbolic(const char *input, const CLContext *ctx, int derivative,
                  char out[CL_EXPR], char *error, size_t cap);
int aero_node_text(const CLProgram *p, int node, char out[CL_EXPR]);
int aero_steps(const char *input, const CLContext *ctx, CLToolResult *r);
void aero_line(CLToolResult *r, const char *format, ...);
int aero_problem(CLToolResult *r, const char *message);
double aero_binomial_pmf(int n, int k, double p);
double aero_binomial_cdf(int n, int k, double p);
double aero_normal_interval(double mu, double sigma, double lo, double hi);
void aero_study_question(AeroState *s, char question[CL_EXPR], CLValue *answer,
                         char explanation[CL_EXPR]);
#endif
