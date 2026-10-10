#include "app.h"
#include "gfx.h"
#include "font.h"
#include "l10n.h"
#include "unicode_font.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char pixels[PP_WIDTH * PP_HEIGHT * 3];
static char filename[500];
static int peak, frames;
static float sample(float u, float v) {
    u = fminf(511, fmaxf(0, u - .5f));
    v = fminf(511, fmaxf(0, v - .5f));
    int x = (int)u, y = (int)v, nx = x == 511 ? x : x + 1, ny = y == 511 ? y : y + 1;
    float dx = u - x, dy = v - y;
    return (pp_atlas[y*512+x]*(1-dx)+pp_atlas[y*512+nx]*dx)*(1-dy) +
           (pp_atlas[ny*512+x]*(1-dx)+pp_atlas[ny*512+nx]*dx)*dy;
}
static int top_left(PPVertex a, PPVertex b) {
    return b.y > a.y || (b.y == a.y && b.x < a.x);
}
void gfx_init(void) {}
void gfx_term(void) {}
static float edge(PPVertex a, PPVertex b, float x, float y) {
    return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}
void gfx_end(void) {
    memset(pixels, 255, sizeof(pixels));
    assert(!pp_vertex_overflow && pp_vertex_count <= PP_MAX_VERTICES);
    for (int i = 0; i < pp_vertex_count; i += 3) {
        PPVertex a = pp_vertices[i], b = pp_vertices[i + 1], c = pp_vertices[i + 2];
        PPVertex points[] = {a, b, c};
        for (int j = 0; j < 3; ++j) {
            PPVertex v = points[j];
            assert(isfinite(v.x) && isfinite(v.y) && v.x >= -1 && v.x <= 481 && v.y >= -1 &&
                   v.y <= 273);
            assert(isfinite(v.u) && isfinite(v.v) && v.u >= 0 && v.u <= 512 && v.v >= 0 && v.v <= 512);
        }
        float sign = edge(a, b, c.x, c.y);
        if (sign == 0)
            continue;
        if (sign < 0) { PPVertex swap = b; b = c; c = swap; sign = -sign; }
        int x0 = (int)floorf(fminf(a.x, fminf(b.x, c.x))),
            x1 = (int)ceilf(fmaxf(a.x, fmaxf(b.x, c.x))),
            y0 = (int)floorf(fminf(a.y, fminf(b.y, c.y))),
            y1 = (int)ceilf(fmaxf(a.y, fmaxf(b.y, c.y)));
        if (x0 < 0)
            x0 = 0;
        if (y0 < 0)
            y0 = 0;
        if (x1 >= PP_WIDTH)
            x1 = PP_WIDTH - 1;
        if (y1 >= PP_HEIGHT)
            y1 = PP_HEIGHT - 1;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                float e1 = edge(a, b, x + .5f, y + .5f), e2 = edge(b, c, x + .5f, y + .5f),
                      e3 = edge(c, a, x + .5f, y + .5f);
                if ((e1 > 0 || (e1 == 0 && top_left(a,b))) &&
                    (e2 > 0 || (e2 == 0 && top_left(b,c))) &&
                    (e3 > 0 || (e3 == 0 && top_left(c,a)))) {
                    int j = (y * PP_WIDTH + x) * 3;
                    float wa = e2/sign, wb = e3/sign, wc = e1/sign;
                    float alpha = (((a.color>>24)&255)*wa + ((b.color>>24)&255)*wb +
                                   ((c.color>>24)&255)*wc) / 255;
                    if (a.u != 2 || a.v != 2)
                        alpha *= sample(a.u*wa+b.u*wb+c.u*wc, a.v*wa+b.v*wb+c.v*wc)/255;
                    for (int ch=0; ch<3; ++ch) {
                        float color = ((a.color>>(ch*8))&255)*wa + ((b.color>>(ch*8))&255)*wb +
                                      ((c.color>>(ch*8))&255)*wc;
                        pixels[j+ch] = (unsigned char)fminf(255,fmaxf(0, color*alpha + pixels[j+ch]*(1-alpha) + .5f));
                    }
                }
            }
    }
    FILE *f = fopen(filename, "wb");
    assert(f);
    fprintf(f, "P6\n480 272\n255\n");
    assert(fwrite(pixels, 1, sizeof(pixels), f) == sizeof(pixels));
    fclose(f);
}
static CLApp a;
static void render(const char *name) {
    cl_ui(&a);
    snprintf(filename, sizeof(filename), "build/%s.ppm", name);
    gfx_end();
    if (pp_vertex_count > peak)
        peak = pp_vertex_count;
    ++frames;
}
int main(void) {
    assert(ca_locale_load(".",LANG_RU));
    assert(ca_font_select(".",LANG_RU));
    /* A fractional-coverage atlas is required; a one-bit font cannot pass. */
    int coverage = 0;
    for (int i=0; i<512*512; ++i) if (pp_atlas[i] > 0 && pp_atlas[i] < 255) ++coverage;
    assert(coverage > 10000);
    assert(gfx_mono_width() == 7);
    assert(gfx_text_width("iiii",1) < gfx_text_width("WWWW",1));
    assert(gfx_text_width("Ёё",1) > 0);
    cl_app_init(&a);
    render("startup");
    cl_app_input(&a, "sin(30)+1/3");
    cl_app_evaluate(&a);
    render("calculator");
    for (int page = 0; page < 6; ++page) {
        a.keypage = page;
        a.keysel = page * 4;
        char name[50];
        snprintf(name, sizeof(name), "keys-%d", page);
        render(name);
    }
    a.save.ctx.complex_mode = 1;
    cl_app_input(&a, "sqrt(-1)+(2+3i)^2");
    cl_app_evaluate(&a);
    render("complex");
    a.save.ctx.complex_mode = 0;
    cl_app_input(&a, "1/0");
    cl_app_evaluate(&a);
    render("error");
    a.save.ctx.angle = CL_RAD;
    a.page = PAGE_GRAPH;
    cl_app_input(&a, "sin(x)");
    for (int i = 0; i < 30; ++i)
        cl_app_tick(&a);
    render("graph");
    a.graph_focus = 2;
    render("table");
    a.graph_focus = 0;
    cl_app_input(&a, "1/x");
    for (int i = 0; i < 30; ++i)
        cl_app_tick(&a);
    render("graph-gap");
    a.page = PAGE_TOOLS;
    a.keypage = 0;
    for (int t = 0; t < CL_TOOL_COUNT; ++t) {
        a.tool = t;
        cl_app_input(&a, cl_tool_example(t));
        cl_app_evaluate(&a);
        char name[50];
        snprintf(name, sizeof(name), "tool-%d", t);
        render(name);
    }
    a.page = PAGE_UNITS;
    a.unit_group = 3;
    a.unit_from = 0;
    a.unit_to = 1;
    cl_app_input(&a, "100");
    cl_app_evaluate(&a);
    render("units");
    a.unit_ok = 0;
    render("units-input");
    a.page = PAGE_BASE;
    a.base_bits = 64;
    a.base_op = 0;
    cl_app_input(&a, "0xFFFFFFFFFFFFFFFF");
    cl_app_evaluate(&a);
    render("programmer");
    a.base_ok = 0;
    a.keypage = 4;
    render("programmer-input");
    a.page = PAGE_HISTORY;
    render("history");
    a.history_bookmarks = 1;
    strcpy(a.save.bookmark[0], "sqrt(2)");
    render("bookmarks");
    a.history_bookmarks = 0;
    a.page = PAGE_SETTINGS;
    for (int i = 0; i < 7; ++i) {
        a.settings_row = i;
        char name[40];
        snprintf(name, sizeof(name), "settings-%d", i);
        render(name);
    }
    a.page = PAGE_HELP;
    for (int i = 0; i < CL_HELP_PAGES; ++i) {
        a.help_page = i;
        char name[40];
        snprintf(name, sizeof(name), "help-%d", i);
        render(name);
    }
    for (int i = 0; i < CL_MENU_COUNT; ++i) {
        a.menu = 1;
        a.menu_row = i;
        char name[40];
        snprintf(name, sizeof(name), "menu-%d", i);
        render(name);
    }
    a.menu = 0;
    a.page = PAGE_CALC;
    a.save.ctx.angle = CL_DEG;
    a.keypage = 0;
    cl_app_input(&a, "sin(30)");
    cl_app_evaluate(&a);
    for (int t = 1; t < 8; ++t) {
        a.save.theme = t;
        char name[40];
        snprintf(name, sizeof(name), "theme-%d", t);
        render(name);
    }
    a.save.theme = 0;
    memset(a.save.expr, '1', 511);
    a.save.expr[511] = 0;
    a.cursor = 511;
    a.has_result = 0;
    a.error.code = 0;
    render("long-input");
    a.save_error = 1;
    render("save-error");
    a.save_error = 0;
    a.menu = 1;
    /* Menus must also fit over the most expensive keypad and longest editor. */
    for (int page = 0; page < 6; ++page) {
        a.keypage = page;
        for (int row = 0; row < CL_MENU_COUNT; ++row) {
            a.menu_row = row;
            char name[60];
            snprintf(name, sizeof(name), "overlay-%d-%d", page, row);
            render(name);
        }
    }
    a.menu = 0;
    a.save_error = 0;
    a.page = PAGE_HISTORY;
    a.save.history_count = 7;
    a.history_row = 6;
    for (int i=0;i<7;++i) {
        strcpy(a.save.history[i].expr,"sin(30)+1/3");
        a.save.history[i].value=cl_real(.83333333333333);
    }
    a.notice[0] = 0;
    render("history-full");
    a.page = PAGE_HELP;
    a.help_page = 0;
    a.save.theme = 2;
    render("night-help");
    cl_app_init(&a);
    char name[100];
    for (int theme=0;theme<8;++theme) {
        a.save.theme=theme;a.page=PAGE_LAB;
        for (int tool=0;tool<AERO_LABS;++tool) {
            a.lab_tool=tool;a.lab_mode=0;a.lab_variant=0;
            snprintf(name,sizeof(name),"lab-%d-%d-catalog",theme,tool);render(name);
            a.lab_mode=1;cl_app_input(&a,cl_lab_example(&a));
            snprintf(name,sizeof(name),"lab-%d-%d-input",theme,tool);render(name);
            cl_app_evaluate(&a);assert(a.lab_result.ok);
            snprintf(name,sizeof(name),"lab-%d-%d-result",theme,tool);render(name);
            if(a.lab_result.count>10){a.lab_scroll=a.lab_result.count-10;snprintf(name,sizeof(name),"lab-%d-%d-end",theme,tool);render(name);}
        }
        a.page=PAGE_MATRIX;
        for(int n=1;n<=4;++n){
            a.extra.matrix_n=n;a.matrix_show=a.matrix_editing=0;a.matrix_row=a.matrix_col=n-1;
            a.extra.matrix[0][(n-1)*4+n-1]=1.23456789012345e100;
            snprintf(name,sizeof(name),"matrix-%d-%d",theme,n);render(name);
        }
        a.matrix_editing=1;cl_app_input(&a,"1/3+sqrt(2)");snprintf(name,sizeof(name),"matrix-%d-edit",theme);render(name);
        a.matrix_editing=0;a.matrix_row=a.matrix_col=0;
        for(int op=0;op<AERO_MATRIX_OPS;++op){a.matrix_op=op;cl_app_evaluate(&a);snprintf(name,sizeof(name),"matrix-%d-op-%d",theme,op);render(name);}
        a.page=PAGE_STUDY;
        for(int topic=0;topic<AERO_STUDY_TOPICS;++topic){
            a.extra.study_topic=topic;aero_study_question(&a.extra,a.study_question,&a.study_answer,a.study_explanation);
            a.study_graded=0;a.study_counted=0;snprintf(name,sizeof(name),"study-%d-%d-input",theme,topic);render(name);
            char answer[80];snprintf(answer,sizeof(answer),"%lld/%lld",(long long)a.study_answer.num,(long long)a.study_answer.den);
            cl_app_input(&a,answer);cl_app_evaluate(&a);assert(a.study_right);
            snprintf(name,sizeof(name),"study-%d-%d-correct",theme,topic);render(name);
            a.study_graded=0;cl_app_input(&a,"-99999");cl_app_evaluate(&a);assert(!a.study_right);
            snprintf(name,sizeof(name),"study-%d-%d-wrong",theme,topic);render(name);
        }
        a.page=PAGE_CALC;a.save.ctx.angle=CL_RAD;cl_app_input(&a,"(1/3+x^2)/(sqrt(1+x^2))");a.page=PAGE_FORMULA;a.view=0;
        snprintf(name,sizeof(name),"formula-%d",theme);render(name);
        strcpy(a.save.expr,"(1+x+x^2+x^3+x^4+x^5+x^6+x^7+x^8)/(1+x+x^2+x^3+x^4+x^5+x^6+x^7+x^8)");
        snprintf(name,sizeof(name),"formula-%d-fallback",theme);render(name);
        a.page=PAGE_GRAPH;a.graph_focus=0;a.graph_series=0;cl_app_input(&a,"sin(x)");strcpy(a.extra.graph[0],"cos(x)");strcpy(a.extra.graph[1],"sin(2*x)");
        cl_app_graph_reset(&a);for(int i=0;i<30;++i)cl_app_tick(&a);
        snprintf(name,sizeof(name),"three-graphs-%d",theme);render(name);
        a.graph_focus=2;a.graph_series=2;snprintf(name,sizeof(name),"three-table-%d",theme);render(name);
        a.page=PAGE_LAB;a.lab_mode=1;a.lab_tool=AE_IEEE;a.keypage=2;memset(a.input[3],'1',511);a.input[3][511]=0;a.cursor=511;a.menu=1;
        for(int row=0;row<CL_MENU_COUNT;++row){a.menu_row=row;snprintf(name,sizeof(name),"new-overlay-%d-%d",theme,row);render(name);}
        a.menu=0;
    }
    // Render every language, page and help page; catch missing glyphs/cache reuse.
    for(int lang=0;lang<LANG_COUNT;lang++){
        assert(ca_locale_load(".",lang));assert(ca_font_select(".",lang));
        cl_app_init(&a);a.notice[0]=0;a.language_row=lang;
        for(int page=0;page<CL_PAGE_COUNT;page++){
            cl_app_page(&a,page);
            char name[100];snprintf(name,sizeof(name),"locale-%s-page-%d",ca_language_code(lang),page);render(name);
        }
        a.page=PAGE_HELP;
        for(int help=0;help<CL_HELP_PAGES;help++){
            a.help_page=help;char name[100];snprintf(name,sizeof(name),"locale-%s-help-%d",ca_language_code(lang),help);render(name);
        }
        for(int topic=0;topic<AERO_STUDY_TOPICS;topic++){
            a.page=PAGE_STUDY;a.extra.study_topic=topic;
            aero_study_question(&a.extra,a.study_question,&a.study_answer,a.study_explanation);
            a.study_graded=1;a.study_right=0;char name[100];snprintf(name,sizeof(name),"locale-%s-study-%d",ca_language_code(lang),topic);render(name);
        }
        assert(ca_font_faults()==0);
    }
    ca_font_close();ca_locale_close();
    FILE *f = fopen("build/ui-qa.json", "w");
    assert(f);
    fprintf(f,
            "{\"states\":%d,\"max_vertices\":%d,\"capacity\":%d,\"resolution\":[480,272],"
            "\"geometry_in_bounds\":true,\"antialias_font\":true,\"atlas_bytes\":262144}\n",
            frames, peak, PP_MAX_VERTICES);
    fclose(f);
    printf("PASS: UI %d states, in-bounds geometry, max %d / %d vertices\n", frames, peak,
           PP_MAX_VERTICES);
    /* XMB icon made with the same smooth renderer, without extra runtime assets. */
    gfx_begin();
    gfx_gradient(0,0,144,80,PP_RGB(162,221,240),PP_RGB(46,145,185));
    gfx_round(12,10,120,61,10,PP_RGBA(15,65,83,70));
    gfx_round_gradient(12,8,120,61,10,PP_RGB(247,255,255),PP_RGB(206,237,247));
    gfx_round_gradient(15,11,114,24,8,PP_RGBA(255,255,255,230),PP_RGBA(255,255,255,20));
    gfx_text(29,15,"f(x)",2,PP_RGB(0,105,150));
    gfx_text(69,43,"CalcAero",1,PP_RGB(20,78,101));
    gfx_round_gradient(22,45,36,14,5,PP_RGB(184,236,172),PP_RGB(100,186,120));
    gfx_text(27,45,"1.3",1,PP_RGB(26,89,57));
    snprintf(filename,sizeof(filename),"build/app-icon.ppm");
    gfx_end();
    return 0;
}
