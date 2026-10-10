#ifndef CALCAERO_L10N_H
#define CALCAERO_L10N_H
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
enum {
    LANG_EN,
    LANG_DE,
    LANG_IT,
    LANG_PL,
    LANG_ES,
    LANG_NL,
    LANG_ET,
    LANG_FI,
    LANG_SV,
    LANG_RO,
    LANG_EL,
    LANG_AR,
    LANG_KK,
    LANG_RU,
    LANG_BE,
    LANG_UK,
    LANG_SR,
    LANG_BS,
    LANG_HR,
    LANG_CS,
    LANG_SK,
    LANG_ZH_TW,
    LANG_ZH_CN,
    LANG_HI,
    LANG_KO,
    LANG_JA,
    LANG_COUNT
};
int ca_language(void);
const char *ca_language_code(int lang);
const char *ca_language_name(int lang);
int ca_language_from_code(const char *code);
int ca_locale_load(const char *dir, int lang);
void ca_locale_close(void);
const char *ca_tr(const char *text);
unsigned ca_hash(const char *text);
int ca_format_compatible(const char *a, const char *b);
int ca_fprintf(FILE *file, const char *format, ...);
int ca_snprintf(char *out, size_t size, const char *format, ...);
int ca_vsnprintf(char *out, size_t size, const char *format, va_list args);
int ca_config_load(const char *dir, int *language);
int ca_config_save(const char *dir, int language);
#define L(s) ca_tr(s)
#ifndef CA_NO_PRINTF_WRAP
#define fprintf ca_fprintf
#define snprintf ca_snprintf
#define vsnprintf ca_vsnprintf
#endif
#endif
