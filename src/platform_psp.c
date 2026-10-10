#include "platform.h"
#include <pspuser.h>
#include <psputility.h>
#include <string.h>
#include "l10n.h"
static SceUtilityOskData data;
static SceUtilityOskParams params;
static unsigned short input[512], output[512], description[80];
static int active, seen, shutting;
int cl_keyboard_active(void) { return active; }
int cl_keyboard_start(const char *text) {
    if (active)
        return 0;
    memset(&data, 0, sizeof(data));
    memset(&params, 0, sizeof(params));
    memset(output, 0, sizeof(output));
    int n = 0;
    while (text[n] && n < 511) {
        input[n] = (unsigned char)text[n];
        ++n;
    }
    input[n] = 0;
    const char *title = "Expression / arguments ; / intervals |";
    int i = 0;
    while (title[i]) {
        description[i] = (unsigned char)title[i];
        ++i;
    }
    description[i] = 0;
    data.language = PSP_UTILITY_OSK_LANGUAGE_ENGLISH;
    data.inputtype =
        PSP_UTILITY_OSK_INPUTTYPE_LATIN_DIGIT | PSP_UTILITY_OSK_INPUTTYPE_LATIN_SYMBOL |
        PSP_UTILITY_OSK_INPUTTYPE_LATIN_LOWERCASE | PSP_UTILITY_OSK_INPUTTYPE_LATIN_UPPERCASE;
    data.lines = 1;
    data.desc = description;
    data.intext = input;
    data.outtext = output;
    data.outtextlength = 512;
    data.outtextlimit = 511;
    params.base.size = sizeof(params);
    params.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP, &params.base.buttonSwap);
    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;
    if (sceUtilityOskInitStart(&params) < 0)
        return 0;
    active = 1;
    seen = shutting = 0;
    return 1;
}
void cl_platform_overlay(void) {
    if (!active)
        return;
    int status = sceUtilityOskGetStatus();
    if (status)
        seen = 1;
    if (status == PSP_UTILITY_DIALOG_VISIBLE)
        sceUtilityOskUpdate(1);
    else if (status == PSP_UTILITY_DIALOG_QUIT && !shutting) {
        sceUtilityOskShutdownStart();
        shutting = 1;
    }
}
int cl_keyboard_poll(char out[512]) {
    if (!active)
        return 0;
    int status = sceUtilityOskGetStatus();
    if (status)
        seen = 1;
    if (status != PSP_UTILITY_DIALOG_NONE || !seen)
        return 0;
    active = 0;
    if (data.result != PSP_UTILITY_OSK_RESULT_CHANGED)
        return -1;
    int n = 0;
    for (int i = 0; i < 511 && output[i]; ++i) {
        unsigned short c = output[i];
        if (c < 32 || c > 126)
            return -2;
        out[n++] = (char)c;
    }
    out[n] = 0;
    return 1;
}
