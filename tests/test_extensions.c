#include "app.h"
#include "l10n.h"
#include "store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static CLApp a, b;
static unsigned rng = 20261009;
static unsigned random_u(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static void menu(int row) {
    cl_app_key(&a, KEY_TRIANGLE);
    a.menu_row = row;
    cl_app_key(&a, KEY_CROSS);
    assert(!a.menu);
}
static void app_features(void) {
    cl_app_init(&a);
    CLError e;
    assert(aero_define(&a.extra, 0, "x^2+1", &e));
    cl_app_input(&a, "f(3)"); cl_app_evaluate(&a);
    assert(a.has_result && a.result.exact && a.result.num == 10);
    assert(!strstr(a.save.history[0].expr, "f("));
    assert(aero_define(&a.extra, 0, "x+1", &e));
    menu(8); cl_app_key(&a, KEY_CROSS); cl_app_key(&a, KEY_START);
    assert(a.result.num == 10); /* history freezes expanded user functions */
    menu(1); cl_app_input(&a, "x^2");
    cl_app_key(&a, KEY_R); cl_app_input(&a, "x");
    cl_app_key(&a, KEY_R); cl_app_input(&a, "f(x)");
    for (int i=0;i<30;++i) cl_app_tick(&a);
    assert(a.graph_index==CL_GRAPH_POINTS);
    assert(a.graph_valid[120] && a.other_valid[0][120] && a.other_valid[1][120]);
    assert(a.graph_y[120]==0 && a.other_y[0][120]==0 && a.other_y[1][120]==1);
    cl_app_key(&a, KEY_CROSS);
    assert(a.page==PAGE_LAB && a.lab_tool==AE_ANALYZE && a.lab_mode==2 && a.lab_result.ok);
    menu(2);
    for (int tool=0;tool<AERO_LABS;++tool) {
        a.lab_tool=tool; a.lab_mode=0;
        cl_app_key(&a, KEY_CROSS); assert(a.lab_mode==1);
        cl_app_key(&a, KEY_START); assert(a.lab_mode==2 && a.lab_result.ok);
        cl_app_key(&a, KEY_CROSS); assert(a.lab_mode==1);
        cl_app_key(&a, KEY_SELECT); assert(a.lab_mode==0);
    }
    menu(3); cl_app_key(&a, KEY_CROSS);
    assert(a.matrix_editing);
    cl_app_input(&a,"1/3"); cl_app_key(&a,KEY_START);
    assert(!a.matrix_editing && a.extra.matrix[0][0]==1./3);
    for (int i=0;i<AERO_MATRIX_OPS;++i) {
        a.matrix_op=i; cl_app_key(&a,KEY_START);
        assert(a.matrix_show && a.matrix_result.ok);
        cl_app_key(&a,KEY_CIRCLE); assert(!a.matrix_show);
    }
    menu(4);
    for (int i=0;i<AERO_STUDY_TOPICS;++i) {
        assert(!a.study_graded);
        char text[100];
        snprintf(text,sizeof(text),"%lld/%lld",(long long)a.study_answer.num,(long long)a.study_answer.den);
        unsigned before=a.extra.study_total;
        cl_app_input(&a,text); cl_app_key(&a,KEY_START);
        assert(a.study_graded && a.study_right && a.extra.study_total==before+1);
        cl_app_key(&a,KEY_CIRCLE); cl_app_key(&a,KEY_START);
        assert(a.extra.study_total==before+1); /* retries cannot inflate the score */
        cl_app_key(&a,KEY_SELECT);
    }
    cl_app_input(&a,"1/0"); unsigned before=a.extra.study_total;
    cl_app_key(&a,KEY_START); assert(!a.study_graded && a.extra.study_total==before);
    cl_app_input(&a,"-99999"); cl_app_key(&a,KEY_START);
    assert(a.study_graded && !a.study_right && a.extra.study_total==before+1);
    menu(0); cl_app_input(&a,"sqrt(1+x^2)/(1+x)"); menu(20);
    assert(a.page==PAGE_FORMULA); cl_app_key(&a,KEY_START); assert(a.page==PAGE_CALC);
    puts("PASS: 15 features, function history snapshots, three curves, lab, matrix and study controller flows");
}
static void write_file(const char *path,const void *data,size_t size) {
    FILE *f=fopen(path,"wb"); assert(f); assert(fwrite(data,1,size,f)==size); assert(fclose(f)==0);
}
static void state_tests(void) {
    char dir[]="build/aero-store-XXXXXX"; assert(mkdtemp(dir));
    char primary[256],backup[256];
    snprintf(primary,sizeof(primary),"%s/aero.dat",dir);
    snprintf(backup,sizeof(backup),"%s/aero.dat.bak",dir);
    cl_app_init(&a); CLError e;
    assert(aero_define(&a.extra,0,"x^2+1",&e));
    assert(aero_define(&a.extra,1,"f(x+1)",&e));
    strcpy(a.extra.graph[0],"g(x)"); strcpy(a.extra.graph[1],"1/x");
    a.extra.matrix[0][1]=1./7; a.extra.study_total=33; a.extra.study_correct=20;
    assert(aero_state_save(dir,&a.extra)); cl_app_init(&b);
    assert(aero_state_load(dir,&b.extra)==1 && !memcmp(&a.extra,&b.extra,sizeof(AeroState)));
    a.extra.pretty=0; assert(aero_state_save(dir,&a.extra));
    unsigned char good[2852],bad[2852];
    FILE *f=fopen(primary,"rb"); assert(f); assert(fread(good,1,sizeof(good),f)==sizeof(good)); assert(fgetc(f)==EOF); fclose(f);
    for (int i=0;i<256;++i) {
        memcpy(bad,good,sizeof(bad)); bad[random_u()%sizeof(bad)]^=(unsigned char)(1+random_u()%255);
        write_file(primary,bad,sizeof(bad));
        assert(aero_state_load(dir,&b.extra)==2 && b.extra.pretty==1 && !strcmp(b.extra.function[1],"f(x+1)"));
    }
    write_file(primary,good,100); assert(aero_state_load(dir,&b.extra)==2);
    write_file(primary,good,sizeof(good)); a.extra.matrix[0][0]=NAN;
    assert(!aero_state_save(dir,&a.extra)); assert(aero_state_load(dir,&b.extra)==1 && b.extra.pretty==0);
    a.extra=b.extra; assert(remove(backup)==0); assert(mkdir(backup,0700)==0);
    assert(!aero_state_save(dir,&a.extra)); assert(aero_state_load(dir,&b.extra)==1);
    assert(rmdir(backup)==0); assert(aero_state_save(dir,&a.extra));
    assert(!aero_state_save("build/missing-parent/child",&a.extra));
    puts("PASS: portable Aero state, 256 CRC corruptions, truncated file, nonfinite rejection and I/O rollback");
}
static void exports(void) {
    char dir[]="build/aero-export-XXXXXX"; assert(mkdtemp(dir));
    char name1[100],name2[100];
    cl_app_init(&a); cl_app_input(&a,"1/3+1/6"); cl_app_evaluate(&a);
    assert(aero_export_csv(dir,&a,name1,sizeof(name1)));
    assert(aero_export_csv(dir,&a,name2,sizeof(name2)) && strcmp(name1,name2));
    menu(1); cl_app_input(&a,"1/x");
    cl_app_key(&a,KEY_R); cl_app_input(&a,"x^2");
    for (int i=0;i<30;++i) cl_app_tick(&a);
    assert(aero_export_csv(dir,&a,name1,sizeof(name1)));
    assert(aero_export_svg(dir,&a,name2,sizeof(name2)));
    FILE *f=fopen("build/export-directory.txt","w"); assert(f); fprintf(f,"%s\n",dir); fclose(f);
    menu(3); assert(aero_export_csv(dir,&a,name1,sizeof(name1)));
    cl_app_key(&a,KEY_START); assert(aero_export_report(dir,&a,name1,sizeof(name1)));
    assert(!aero_export_svg(dir,&a,name2,sizeof(name2)));
    assert(!aero_export_csv("build/missing-parent/child",&a,name1,sizeof(name1)));
    puts("PASS: unique CSV, SVG, TXT, graph gaps, matrix export and I/O failures");
}
static void fuzz(void) {
    cl_app_init(&a); CLError e; assert(aero_define(&a.extra,0,"x^2+1",&e));
    const char *alphabet="0123456789+-*/^()!;|abcdefghijklmnopqrstuvwxyz .?";
    CLContext c; cl_context_init(&c); CLToolResult r;
    for (int i=0;i<4000;++i) {
        char text[512]; int n=random_u()%511;
        for (int j=0;j<n;++j) text[j]=alphabet[random_u()%strlen(alphabet)];
        text[n]=0;
        int ok=aero_lab_run(random_u()%AERO_LABS,text,&c,&a.extra,&r);
        assert(r.count>0 && r.count<=CL_TOOL_LINES && !!ok==!!r.ok);
    }
    puts("PASS: 4000 malformed extension expressions under ASan/UBSan");
}
int main(void) {
    assert(ca_locale_load(".",LANG_RU));app_features();state_tests();exports();fuzz();return 0;}
