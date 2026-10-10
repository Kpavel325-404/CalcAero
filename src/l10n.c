#define CA_NO_PRINTF_WRAP
#include "l10n.h"
#include "store.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static int language = LANG_EN;
static unsigned char *catalog;
static size_t catalog_size;
static uint32_t count;
static const char *codes[LANG_COUNT] = {"en", "de", "it", "pl",    "es",    "nl", "et", "fi", "sv",
                                        "ro", "el", "ar", "kk",    "ru",    "be", "uk", "sr", "bs",
                                        "hr", "cs", "sk", "zh-TW", "zh-CN", "hi", "ko", "ja", "pt", "fr"};
static const char *names[LANG_COUNT] = {"English",
                                        "Deutsch",
                                        "Italiano",
                                        "Polski",
                                        "Espanol",
                                        "Nederlands",
                                        "Eesti",
                                        "Suomi",
                                        "Svenska",
                                        "Romana",
                                        "Greek",
                                        "Arabic",
                                        "Kazakh",
                                        "Russian",
                                        "Belarusian",
                                        "Ukrainian",
                                        "Serbian",
                                        "Bosanski",
                                        "Hrvatski",
                                        "Cesky",
                                        "Slovencina",
                                        "Chinese (Traditional)",
                                        "Chinese (Simplified)",
                                        "Hindi",
                                        "Korean",
                                        "Japanese",
                                        "Portugu\303\252s",
                                        "Fran\303\247ais"};
int ca_language(void) { return language; }
const char *ca_language_code(int id) { return id >= 0 && id < LANG_COUNT ? codes[id] : "en"; }
const char *ca_language_name(int id) { return id >= 0 && id < LANG_COUNT ? names[id] : "?"; }
int ca_language_from_code(const char *code) {
    for (int i = 0; i < LANG_COUNT; i++)
        if (!strcmp(code, codes[i]))
            return i;
    return -1;
}
unsigned ca_hash(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h;
}
static uint32_t u32(const unsigned char *s) {
    return (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24;
}
void ca_locale_close(void) {
    free(catalog);
    catalog = 0;
    count = 0;
    catalog_size = 0;
    language = LANG_EN;
}
int ca_locale_load(const char *dir, int lang) {
    if (lang < 0 || lang >= LANG_COUNT)
        return 0;
    if (lang == LANG_EN) {
        ca_locale_close();
        return 1;
    }
    char path[700];
    int n = snprintf(path, sizeof(path), "%s/assets/locale/%s.bin", dir, codes[lang]);
    if (n < 0 || (size_t)n >= sizeof(path))
        return 0;
    struct stat st;
    if (stat(path, &st) || st.st_size < 20 || st.st_size > 262144)
        return 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    size_t size = (size_t)st.st_size;
    unsigned char *p = malloc(size);
    if (!p) {
        fclose(f);
        return 0;
    }
    int ok = fread(p, 1, size, f) == size;
    if (fclose(f))
        ok = 0;
    if (!ok || memcmp(p, "CALNG013", 8) || u32(p + size - 4) != cl_crc32(p, size - 4)) {
        free(p);
        return 0;
    }
    uint32_t cnt = u32(p + 8);
    if (cnt > 2000 || 12 + (size_t)cnt * 8 > size - 4) {
        free(p);
        return 0;
    }
    uint32_t prev = 0;
    for (uint32_t i = 0; i < cnt; i++) {
        uint32_t h = u32(p + 12 + i * 8), offset = u32(p + 16 + i * 8);
        if ((i && h <= prev) || offset < 12 + cnt * 8 || offset >= size - 4 ||
            !memchr(p + offset, 0, size - 4 - offset)) {
            free(p);
            return 0;
        }
        prev = h;
    }
    free(catalog);
    catalog = p;
    count = cnt;
    catalog_size = size;
    language = lang;
    return 1;
}
const char *ca_tr(const char *s) {
    if (!s || !catalog || !*s)
        return s ? s : "";
    uint32_t h = ca_hash(s), lo = 0, hi = count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2, v = u32(catalog + 12 + mid * 8);
        if (v < h)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < count && u32(catalog + 12 + lo * 8) == h) {
        uint32_t at = u32(catalog + 16 + lo * 8);
        if (at < catalog_size - 4)
            return (char *)catalog + at;
    }
    return s;
}
/* Compare the complete ordered printf conversions, including widths/lengths.
   A corrupt or edited translation cannot change the types consumed from va_list. */
static const char *conversion(const char *s, size_t *len) {
    while (*s) {
        if (*s++ != '%')
            continue;
        const char *begin = s - 1;
        if (*s == '%') {
            s++;
            continue;
        }
        while (*s && strchr("-+ #0", *s))
            s++;
        while (isdigit((unsigned char)*s))
            s++;
        if (*s == '*')
            s++;
        if (*s == '.') {
            s++;
            while (isdigit((unsigned char)*s))
                s++;
            if (*s == '*')
                s++;
        }
        while (*s && strchr("hlLjzt", *s))
            s++;
        if (*s && strchr("diuoxXfFeEgGaAcspn", *s)) {
            *len = (size_t)(s - begin + 1);
            return begin;
        }
        if (!*s)
            break;
    }
    *len = 0;
    return s;
}
int ca_format_compatible(const char *a, const char *b) {
    for (;;) {
        size_t an, bn;
        const char *ap = conversion(a, &an), *bp = conversion(b, &bn);
        if (an != bn || (an && memcmp(ap, bp, an)))
            return 0;
        if (!an)
            return 1;
        a = ap + an;
        b = bp + bn;
    }
}
int ca_vsnprintf(char *out, size_t size, const char *fmt, va_list args) {
    const char *translated = ca_tr(fmt);
    if (!ca_format_compatible(fmt, translated))
        translated = fmt;
    return vsnprintf(out, size, translated, args);
}
int ca_snprintf(char *out, size_t size, const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    int n = ca_vsnprintf(out, size, fmt, a);
    va_end(a);
    return n;
}

int ca_fprintf(FILE *file, const char *format, ...) {
    const char *translated = ca_tr(format);
    if (!ca_format_compatible(format, translated))
        translated = format;
    va_list args;
    va_start(args, format);
    int result = vfprintf(file, translated, args);
    va_end(args);
    return result;
}
