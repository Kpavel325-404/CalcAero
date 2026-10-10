#ifndef CALCLAB_TOOLS_H
#define CALCLAB_TOOLS_H
#include "calc.h"
#define CL_TOOL_COUNT 9
#define CL_TOOL_LINES 48
typedef struct {
    char line[CL_TOOL_LINES][160];
    int count, ok, approx;
    double value, error;
} CLToolResult;
const char *cl_tool_name(int tool);
const char *cl_tool_hint(int tool);
const char *cl_tool_example(int tool);
int cl_tool_run(int tool, const char *input, const CLContext *ctx, CLToolResult *r);
int cl_split_values(const char *s, char sep, char out[][CL_EXPR], int max);
int cl_unit_groups(void);
const char *cl_unit_group_name(int group);
int cl_unit_count(int group);
const char *cl_unit_name(int group, int unit);
int cl_convert_unit(int group, int from, int to, double x, double *out);
int cl_programmer(const char *input, int bits, int op, uint64_t *out, char *error, size_t cap);
void cl_base_format(uint64_t v, int bits, int base, int is_signed, char *out, size_t cap);
#endif
