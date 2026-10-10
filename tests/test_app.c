#include "app.h"
#include "l10n.h"
#include "selftest.h"
#include "store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static CLApp app, other;
static unsigned rng = 20261004;
static unsigned random_u(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static void check_app(void) {
    assert(app.page >= 0 && app.page < CL_PAGE_COUNT);
    assert(app.keypage >= 0 && app.keypage < 6);
    assert(app.keysel >= 0 && app.keysel < 30);
    assert(app.cursor >= 0 && app.cursor < (int)CL_EXPR);
    assert(app.save.history_count >= 0 && app.save.history_count <= CL_HISTORY);
    assert(app.save.ctx.angle >= CL_DEG && app.save.ctx.angle <= CL_GRAD);
    assert(strlen(cl_app_editor(&app)) < CL_EXPR);
    assert(app.graph_series >= 0 && app.graph_series < AERO_CURVES);
    assert(app.lab_tool >= 0 && app.lab_tool < AERO_LABS);
    assert(app.extra.matrix_n >= 1 && app.extra.matrix_n <= 4);
    assert(app.matrix_row >= 0 && app.matrix_row < app.extra.matrix_n);
    assert(app.matrix_col >= 0 && app.matrix_col < app.extra.matrix_n);
}
static void app_tests(void) {
    cl_app_init(&app);
    cl_app_input(&app, "7");
    cl_app_evaluate(&app);
    app.save.ctx.mem[0] = cl_integer(9);
    app.save.ctx.complex_mode = 1;
    cl_app_evaluate(&app);
    assert(app.save.history_count == 1 && app.save.history[0].context.mem[0].num == 9 &&
           app.save.history[0].complex_mode == 1);
    cl_app_init(&app);
    cl_app_input(&app, "1/3+1/6");
    cl_app_key(&app, KEY_START);
    assert(app.has_result && app.result.exact && app.result.num == 1 && app.result.den == 2);
    cl_app_input(&app, "Ans+1");
    cl_app_key(&app, KEY_START);
    assert(app.result.num == 3 && app.result.den == 2);
    app.page = PAGE_HISTORY;
    app.history_row = 0;
    cl_app_key(&app, KEY_CROSS);
    cl_app_key(&app, KEY_START);
    assert(app.result.num == 3 &&
           app.result.den == 2); /* restores the earlier Ans, not the newest result */
    cl_app_input(&app, "123");
    cl_app_key(&app, KEY_CIRCLE);
    assert(!strcmp(app.save.expr, "12"));
    app.keypage = 0;
    app.keysel = 25;
    cl_app_key(&app, KEY_CROSS);
    assert(!strcmp(app.save.expr, "123"));
    cl_app_key(&app, KEY_CROSS);
    assert(!strcmp(app.save.expr, "12"));
    cl_app_key(&app, KEY_SQUARE);
    cl_app_key(&app, KEY_UP);
    assert(app.cursor == 0);
    cl_app_key(&app, KEY_RIGHT);
    assert(app.cursor == 1);
    cl_app_key(&app, KEY_CIRCLE);
    assert(!strcmp(app.save.expr, "2"));
    for (int i = 0; i < 60; ++i) {
        char s[30];
        snprintf(s, sizeof(s), "%d+1", i);
        cl_app_input(&app, s);
        cl_app_key(&app, KEY_START);
    }
    assert(app.save.history_count == 32 && !strcmp(app.save.history[0].expr, "59+1"));
    app.page = PAGE_GRAPH;
    cl_app_input(&app, "1/x");
    for (int i = 0; i < 30; ++i)
        cl_app_tick(&app);
    assert(app.graph_index == CL_GRAPH_POINTS && !app.graph_valid[120]);
    app.page = PAGE_CALC;
    app.save.ctx.complex_mode = 1;
    cl_app_input(&app, "sqrt(-1)");
    cl_app_key(&app, KEY_START);
    assert(app.result.im == 1);
    app.keypage = 3;
    app.keysel = 10;
    cl_app_key(&app, KEY_CROSS);
    assert(app.save.ctx.mem[4].im == 1);
    app.page = PAGE_SETTINGS;
    app.settings_row = 1;
    cl_app_key(&app, KEY_CROSS);
    assert(!app.save.ctx.complex_mode);
    app.page = PAGE_CALC;
    cl_app_input(&app, "E");
    cl_app_key(&app, KEY_START);
    assert(!app.has_result && app.error.code == CL_COMPLEX);
    cl_app_init(&app);
    for (int i = 0; i < 30000; ++i) {
        unsigned k = 1u << (random_u() % 12);
        cl_app_key(&app, k);
        if (app.request_keyboard) {
            app.request_keyboard = 0;
            cl_app_input(&app, "sin(30)+1/3");
        }
        app.request_export = app.request_exit = 0;
        app.request_csv = app.request_svg = app.request_capture = app.request_report = 0;
        cl_app_tick(&app);
        check_app();
    }
    puts("PASS: editing, history context, memory, graph gaps, 30000 controller actions");
}
static unsigned char bytes[33000], good[33000];
static void write_bytes(const char *path, const unsigned char *p, size_t n) {
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(p, 1, n, f) == n);
    assert(fclose(f) == 0);
}
static void store_tests(void) {
    char folder[] = "build/store-test-XXXXXX";
    assert(mkdtemp(folder));
    char main[300], backup[300], report[300];
    snprintf(main, sizeof(main), "%s/state.dat", folder);
    snprintf(backup, sizeof(backup), "%s/state.dat.bak", folder);
    cl_app_init(&app);
    cl_app_input(&app, "sin(30)+1/3");
    cl_app_evaluate(&app);
    app.save.ctx.mem[4] = cl_fraction(1, 7);
    strcpy(app.save.bookmark[0], "sqrt(2)");
    assert(cl_store_save(folder, &app.save));
    cl_app_init(&other);
    assert(cl_store_load(folder, &other.save) == 1);
    assert(!strcmp(other.save.expr, app.save.expr) && other.save.ctx.mem[4].exact &&
           other.save.ctx.mem[4].den == 7);
    assert(other.save.history[0].value.re == app.save.history[0].value.re);
    app.save.digits = 15;
    assert(cl_store_save(folder, &app.save));
    assert(access(backup, F_OK) == 0);
    FILE *f = fopen(main, "rb");
    assert(f);
    assert(fread(good, 1, sizeof(good), f) == sizeof(good));
    fclose(f);
    for (int i = 0; i < 256; ++i) {
        memcpy(bytes, good, sizeof(bytes));
        size_t pos = random_u() % sizeof(bytes);
        bytes[pos] ^= (unsigned char)(1 + (random_u() % 255));
        write_bytes(main, bytes, sizeof(bytes));
        assert(cl_store_load(folder, &other.save) == 2 && other.save.digits == 12);
    }
    write_bytes(main, good, 100);
    assert(cl_store_load(folder, &other.save) == 2);
    write_bytes(main, good, sizeof(good));
    assert(cl_store_load(folder, &other.save) == 1 && other.save.digits == 15);
    memset(app.save.expr, 'A', CL_EXPR);
    assert(!cl_store_save(folder, &app.save));
    assert(cl_store_load(folder, &other.save) == 1 && other.save.digits == 15);
    strcpy(app.save.expr, "2+2");
    assert(!cl_store_save("build/does-not-exist/child", &app.save));
    /* A backup path collision must not replace the valid primary state. */
    assert(remove(backup) == 0);
    assert(mkdir(backup, 0700) == 0);
    assert(!cl_store_save(folder, &app.save));
    assert(cl_store_load(folder, &other.save) == 1);
    assert(rmdir(backup) == 0);
    char name1[80], name2[80];
    assert(cl_export_history(folder, &other.save, name1, sizeof(name1)));
    assert(cl_export_history(folder, &other.save, name2, sizeof(name2)));
    assert(strcmp(name1, name2));
    snprintf(report, sizeof(report), "%s/reports/%s", folder, name1);
    f = fopen(report, "rb");
    assert(f);
    assert(fread(bytes, 1, 500, f) > 0);
    fclose(f);
    assert(cl_store_save(folder, &app.save));
    assert(cl_store_load(folder, &other.save) == 1 && !strcmp(other.save.expr, "2+2"));
    puts("PASS: saved context, 256 CRC corruptions, backup recovery, invalid-state and I/O "
         "rollback, unique exports");
}
static void parser_fuzz(void) {
    CLContext c;
    cl_context_init(&c);
    CLValue v;
    CLError e;
    char s[CL_EXPR + 8];
    const char *alphabet = "0123456789+-*/^()!.;,|abcdefghijklmnopqrstuvwxyz ABCDEFi";
    for (int i = 0; i < 30000; ++i) {
        int len = (int)(random_u() % sizeof(s));
        for (int j = 0; j < len; ++j)
            s[j] = alphabet[random_u() % strlen(alphabet)];
        s[len] = 0;
        c.angle = (CLAngle)(random_u() % 3);
        c.complex_mode = random_u() % 2;
        if (cl_calculate(s, &c, &v, &e)) {
            assert(cl_value_valid(v));
            assert(!e.code);
        } else
            assert(e.code > 0 && e.code <= CL_COMPLEX);
    }
    assert(cl_calculate("(-9223372036854775807-1)/(-9223372036854775807-1)", &c, &v, &e) &&
           v.re == 1);
    assert(cl_calculate("sqrt(9223372036854775807)", &c, &v, &e));
    assert(cl_calculate("ncr(1000;500)", &c, &v, &e) && isfinite(v.re));
    assert(!cl_calculate("1e-200*1e-200", &c, &v, &e) && e.code == CL_PRECISION);
    int total;
    int passed = cl_selftest(&total);
    assert(passed == total);
    puts("PASS: 30000 malformed expressions, int64 limits, factorial/combinations and native "
         "self-checks");
}
int main(void) {
    assert(ca_locale_load(".",LANG_RU));
    app_tests();
    store_tests();
    parser_fuzz();
    return 0;
}
