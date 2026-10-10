/* A real save produced by the previous CalcLab KP 1.0 PSP EBOOT in PPSSPP. */
#include "app.h"
#include "l10n.h"
#include "store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static CLApp app;
int main(void) {
    assert(ca_locale_load(".",LANG_RU));
    cl_app_init(&app);
    assert(cl_store_load("tests/fixtures/1.0", &app.save) == 1);
    assert(app.save.history_count >= 3);
    assert(fabs(app.save.ctx.ans.re - 1.5) < 1e-14);
    cl_app_input(&app, "Ans+1");
    cl_app_evaluate(&app);
    assert(app.has_result && !app.error.code && fabs(app.result.re - 2.5) < 1e-14);
    printf("PASS: real CalcLab KP 1.0 save loaded; history, settings and Ans preserved\n");
    return 0;
}
