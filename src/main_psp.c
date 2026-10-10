#include "app.h"
#include "gfx.h"
#include "l10n.h"
#include "platform.h"
#include "selftest.h"
#include "store.h"
#include "unicode_font.h"
#include <pspctrl.h>
#include <psppower.h>
#include <pspuser.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
PSP_MODULE_INFO("CalcAero", 0, 1, 3);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_MAIN_THREAD_STACK_SIZE_KB(256);
PSP_HEAP_SIZE_KB(4096);
static CLApp app;
static volatile int exiting, power_event;
static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    exiting = 1;
    return 0;
}
static int power_callback(int unknown, int flags, void *common) {
    (void)unknown;
    (void)common;
    if (flags & (PSP_POWER_CB_SUSPENDING | PSP_POWER_CB_RESUME_COMPLETE))
        power_event = 1;
    return 0;
}
static int callbacks(SceSize size, void *arg) {
    (void)size;
    (void)arg;
    int exitid = sceKernelCreateCallback("CalcAeroExit", exit_callback, 0);
    if (exitid >= 0)
        sceKernelRegisterExitCallback(exitid);
    int powerid = sceKernelCreateCallback("CalcAeroPower", power_callback, 0);
    if (powerid >= 0)
        scePowerRegisterCallback(-1, powerid);
    sceKernelSleepThreadCB();
    return 0;
}
static unsigned map(unsigned p) {
    unsigned k = 0;
    if (p & PSP_CTRL_UP)
        k |= KEY_UP;
    if (p & PSP_CTRL_DOWN)
        k |= KEY_DOWN;
    if (p & PSP_CTRL_LEFT)
        k |= KEY_LEFT;
    if (p & PSP_CTRL_RIGHT)
        k |= KEY_RIGHT;
    if (p & PSP_CTRL_CROSS)
        k |= KEY_CROSS;
    if (p & PSP_CTRL_CIRCLE)
        k |= KEY_CIRCLE;
    if (p & PSP_CTRL_SQUARE)
        k |= KEY_SQUARE;
    if (p & PSP_CTRL_TRIANGLE)
        k |= KEY_TRIANGLE;
    if (p & PSP_CTRL_LTRIGGER)
        k |= KEY_L;
    if (p & PSP_CTRL_RTRIGGER)
        k |= KEY_R;
    if (p & PSP_CTRL_START)
        k |= KEY_START;
    if (p & PSP_CTRL_SELECT)
        k |= KEY_SELECT;
    return k;
}
static void save(void) {
    if (!app.dirty)
        return;
    int base_ok = cl_store_save(".", &app.save), aero_ok = aero_state_save(".", &app.extra);
    app.save_error = !base_ok || !aero_ok;
    if (!app.save_error)
        app.dirty = 0;
}
int main(int argc, char **argv) {
    if (argc > 0 && argv[0]) {
        char folder[700];
        snprintf(folder, sizeof(folder), "%s", argv[0]);
        char *slash = strrchr(folder, '/');
        if (slash) {
            *slash = 0;
            chdir(folder);
        }
    }
    int language = LANG_EN;
    int configured = ca_config_load(".", &language);
    if (!ca_font_select(".", language) || !ca_locale_load(".", language)) {
        ca_locale_load(".", LANG_EN);
        ca_font_select(".", LANG_EN);
        configured = 0;
        language = LANG_EN;
    }
    cl_app_init(&app);
    app.language_row = language;
    if (!configured)
        cl_app_page(&app, PAGE_LANGUAGE);
    int total = 0, passed = cl_selftest(&total);
    snprintf(app.notice, sizeof(app.notice), "Engine check: %d/%d; /\\: help and menu", passed,
             total);
    int loaded = cl_store_load(".", &app.save);
    int extra_loaded = aero_state_load(".", &app.extra);
    if (!loaded && access("state.dat", F_OK) != 0 && access("state.dat.bak", F_OK) != 0) {
        loaded = cl_store_load("../CalcLab", &app.save);
        if (loaded) {
            app.dirty = 1;
            snprintf(app.notice, sizeof(app.notice), "History and settings imported from CalcLab");
        }
    }
    app.cursor = (int)strlen(app.save.expr);
    if (loaded == 2)
        snprintf(app.notice, sizeof(app.notice), "State restored from backup");
    if (extra_loaded == 2)
        snprintf(app.notice, sizeof(app.notice), "Functions and matrices restored from backup");
    int thread = sceKernelCreateThread("CalcAeroCallbacks", callbacks, 0x11, 0x2000,
                                       PSP_THREAD_ATTR_USER, 0);
    if (thread >= 0)
        sceKernelStartThread(thread, 0, 0);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    gfx_init();
    unsigned previous = 0;
    uint64_t repeat_at = 0, changed_at = 0, last_save = 0;
    int suppress = 1, redraw = 1;
    while (!exiting && !app.request_exit) {
        uint64_t now = sceKernelGetSystemTimeWide();
        SceCtrlData pad;
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned held = map(pad.Buttons), pressed = held & ~previous;
        if (power_event) {
            power_event = 0;
            save();
            suppress = 1;
            previous = held;
            redraw = 1;
        }
        if (suppress) {
            pressed = 0;
            if (!held)
                suppress = 0;
        }
        unsigned repeatable = KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_CIRCLE;
        if (held != previous)
            repeat_at = now + 420000;
        else if (held && now >= repeat_at) {
            pressed |= held & repeatable;
            repeat_at = now + 85000;
        }
        previous = held;
        if (!cl_keyboard_active() && !app.request_keyboard && pressed) {
            cl_app_key(&app, pressed);
            redraw = 1;
            changed_at = now;
            if (app.request_exit) {
                save();
                if (app.save_error) {
                    app.request_exit = 0;
                    app.menu = 0;
                }
            }
        }
        if (app.request_language >= 0) {
            int chosen = app.request_language, old = ca_language();
            app.request_language = -1;
            if (ca_font_select(".", chosen) && ca_locale_load(".", chosen) &&
                ca_config_save(".", chosen)) {
                app.notice[0] = 0;
                aero_study_question(&app.extra, app.study_question, &app.study_answer,
                                    app.study_explanation);
                cl_app_page(&app, PAGE_CALC);
            } else {
                ca_font_select(".", old);
                ca_locale_load(".", old);
                snprintf(app.notice, sizeof(app.notice),
                         "Language save failed: check assets and memory card");
            }
            redraw = 1;
        }
        /* Let the opening button go before handing input to the system OSK. */
        if (app.request_keyboard && !held) {
            app.request_keyboard = 0;
            if (!cl_keyboard_start(cl_app_editor(&app)))
                snprintf(app.notice, sizeof(app.notice), "Could not open the keyboard");
            redraw = 1;
        }
        char text[CL_EXPR];
        int status = cl_keyboard_poll(text);
        if (status) {
            redraw = 1;
            if (status == 1) {
                cl_app_input(&app, text);
                changed_at = now;
            } else if (status == -2)
                snprintf(app.notice, sizeof(app.notice), "Use Latin characters");
            suppress = 1;
        }
        if (app.request_export) {
            redraw = 1;
            char name[80];
            app.request_export = 0;
            if (cl_export_history(".", &app.save, name, sizeof(name)))
                snprintf(app.notice, sizeof(app.notice), "Saved: reports/%s", name);
            else
                snprintf(app.notice, sizeof(app.notice), "Could not save the report");
        }
        if (app.request_csv || app.request_svg || app.request_report || app.request_capture) {
            while (app.page == PAGE_GRAPH && app.graph_index < CL_GRAPH_POINTS)
                cl_app_tick(&app);
            char name[80];
            int ok = 0;
            if (app.request_csv)
                ok = aero_export_csv(".", &app, name, sizeof(name));
            else if (app.request_svg)
                ok = aero_export_svg(".", &app, name, sizeof(name));
            else if (app.request_report)
                ok = aero_export_report(".", &app, name, sizeof(name));
            else if (app.request_capture) {
                cl_ui(&app);
                gfx_end();
                ok = aero_capture_bmp(".", name, sizeof(name));
            }
            app.request_csv = app.request_svg = app.request_report = app.request_capture = 0;
            snprintf(app.notice, sizeof(app.notice),
                     ok ? "Saved: reports/%s" : "Save failed: calculate a result first",
                     ok ? name : "");
            redraw = 1;
        }
        if (app.dirty && now - changed_at >= 1500000 && now - last_save >= 3000000) {
            int old_dirty = app.dirty, old_error = app.save_error;
            save();
            last_save = now;
            if (old_dirty != app.dirty || old_error != app.save_error)
                redraw = 1;
        }
        int old_index = app.graph_index;
        cl_app_tick(&app);
        if (old_index != app.graph_index)
            redraw = 1;
        if (redraw || cl_keyboard_active()) {
            if (cl_keyboard_active()) {
                gfx_begin();
                gfx_rect(0, 0, PP_WIDTH, PP_HEIGHT, PP_RGB(18, 22, 31));
            } else
                cl_ui(&app);
            gfx_end();
            redraw = 0;
        }
        sceKernelDelayThreadCB(16000);
    }
    save();
    gfx_term();
    ca_font_close();
    ca_locale_close();
    sceKernelExitGame();
    return 0;
}
