#ifndef CALCLAB_APP_H
#define CALCLAB_APP_H
#include "aero.h"
#include "calc.h"
#include "tools.h"
#define CL_HISTORY 32
#define CL_BOOKMARKS 8
#define CL_GRAPH_POINTS 241
enum {
    PAGE_CALC,
    PAGE_GRAPH,
    PAGE_TOOLS,
    PAGE_UNITS,
    PAGE_BASE,
    PAGE_HISTORY,
    PAGE_SETTINGS,
    PAGE_HELP,
    PAGE_LAB,
    PAGE_MATRIX,
    PAGE_STUDY,
    PAGE_FORMULA,
    PAGE_LANGUAGE,
    CL_PAGE_COUNT
};
#define CL_MENU_COUNT 22
#define CL_HELP_PAGES 8
enum {
    KEY_UP = 1,
    KEY_DOWN = 2,
    KEY_LEFT = 4,
    KEY_RIGHT = 8,
    KEY_CROSS = 16,
    KEY_CIRCLE = 32,
    KEY_SQUARE = 64,
    KEY_TRIANGLE = 128,
    KEY_L = 256,
    KEY_R = 512,
    KEY_START = 1024,
    KEY_SELECT = 2048
};
typedef struct {
    char expr[CL_EXPR];
    CLValue value;
    int angle, complex_mode;
    CLContext context;
} CLHistory;
typedef struct {
    CLContext ctx;
    int digits, style, fraction, theme;
    char expr[CL_EXPR], bookmark[CL_BOOKMARKS][CL_EXPR];
    CLHistory history[CL_HISTORY];
    int history_count;
} CLPersistent;
typedef struct {
    CLPersistent save;
    int page, menu, menu_row, keypage, keysel, editing, cursor, view, dirty;
    int has_result, history_row, history_bookmarks, settings_row, help_page;
    CLValue result;
    CLError error;
    char notice[160], undo[CL_EXPR];
    int undo_cursor, undo_ready;
    char input[6][CL_EXPR];
    int tool, tool_scroll, unit_group, unit_from, unit_to, unit_row, base_bits, base_op, base_row;
    int request_keyboard, request_export, request_exit, save_error;
    CLToolResult tool_result;
    double unit_result;
    int unit_ok;
    uint64_t base_result;
    int base_ok;
    CLProgram graph_program;
    int graph_compiled, graph_index, graph_valid[CL_GRAPH_POINTS], graph_trace;
    double graph_y[CL_GRAPH_POINTS], xmin, xmax, ymin, ymax;
    int graph_focus;
    AeroState extra;
    CLProgram other_graph[2];
    int other_compiled[2], other_valid[2][CL_GRAPH_POINTS], graph_series;
    double other_y[2][CL_GRAPH_POINTS];
    int lab_tool, lab_mode, lab_scroll, lab_variant;
    CLToolResult lab_result, matrix_result;
    int matrix_slot, matrix_row, matrix_col, matrix_editing, matrix_op, matrix_show, matrix_scroll;
    char study_question[CL_EXPR], study_explanation[CL_EXPR];
    CLValue study_answer;
    int study_graded, study_counted, study_right;
    int request_csv, request_svg, request_capture, request_report;
    int language_row, request_language;
} CLApp;
typedef struct {
    const char *label, *insert;
    int action;
} CLKey;
void cl_app_init(CLApp *a);
void cl_app_key(CLApp *a, unsigned keys);
void cl_app_input(CLApp *a, const char *s);
void cl_app_evaluate(CLApp *a);
void cl_app_tick(CLApp *a);
char *cl_app_editor(CLApp *a);
const CLKey *cl_key(int page, int index);
const char *cl_keypage_name(int page);
void cl_app_graph_reset(CLApp *a);
void cl_app_page(CLApp *a, int target);
int cl_menu_index(int page);
const char *cl_menu_name(int row);
const char *cl_graph_expression(const CLApp *a, int series);
const char *cl_lab_example(const CLApp *a);
int aero_export_csv(const char *dir, const CLApp *a, char *name, size_t cap);
int aero_export_svg(const char *dir, const CLApp *a, char *name, size_t cap);
int aero_export_report(const char *dir, const CLApp *a, char *name, size_t cap);
int aero_capture_bmp(const char *dir, char *name, size_t cap);
void cl_ui(const CLApp *a);
#endif
