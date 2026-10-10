#define CA_NO_PRINTF_WRAP
#include "l10n.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void ws(const char **p) {
    while (isspace((unsigned char)**p))
        ++*p;
}
static int string(const char **p, char *out, size_t cap) {
    ws(p);
    if (*(*p)++ != '"')
        return 0;
    size_t n = 0;
    while (**p && **p != '"') {
        unsigned char c = (unsigned char)*(*p)++;
        if (c < 32 || c == '\\' || n + 1 >= cap)
            return 0;
        out[n++] = (char)c;
    }
    if (**p != '"')
        return 0;
    ++*p;
    out[n] = 0;
    return 1;
}
static int parse(const char *p, int *lang) {
    ws(&p);
    if (*p++ != '{')
        return 0;
    int seen = 0, chosen = LANG_EN;
    ws(&p);
    if (*p == '}')
        return 0;
    for (int fields = 0; fields < 16; fields++) {
        char key[40], value[80];
        if (!string(&p, key, sizeof(key)))
            return 0;
        ws(&p);
        if (*p++ != ':')
            return 0;
        ws(&p);
        if (!strcmp(key, "language")) {
            if (seen++ || !string(&p, value, sizeof(value)))
                return 0;
            chosen = ca_language_from_code(value);
            if (chosen < 0)
                return 0;
        } else if (*p == '"') {
            if (!string(&p, value, sizeof(value)))
                return 0;
        } else {
            const char *start = p;
            if (*p == '-')
                p++;
            while (isdigit((unsigned char)*p))
                p++;
            if (p == start || p[-1] == '-')
                return 0;
        }
        ws(&p);
        if (*p == '}') {
            p++;
            ws(&p);
            if (*p || !seen)
                return 0;
            *lang = chosen;
            return 1;
        }
        if (*p++ != ',')
            return 0;
    }
    return 0;
}
int ca_config_load(const char *dir, int *lang) {
    *lang = LANG_EN;
    char path[700], buf[1025];
    int n = snprintf(path, sizeof(path), "%s/config.json", dir);
    if (n < 0 || (size_t)n >= sizeof(path))
        return 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    size_t len = fread(buf, 1, sizeof(buf), f);
    int err = ferror(f);
    fclose(f);
    if (err || len >= sizeof(buf) || memchr(buf, 0, len))
        return 0;
    buf[len] = 0;
    return parse(buf, lang);
}
int ca_config_save(const char *dir, int lang) {
    if (lang < 0 || lang >= LANG_COUNT)
        return 0;
    char path[700], temp[700], backup[700];
    if (snprintf(path, sizeof(path), "%s/config.json", dir) >= (int)sizeof(path) ||
        snprintf(temp, sizeof(temp), "%s/config.json.tmp", dir) >= (int)sizeof(temp) ||
        snprintf(backup, sizeof(backup), "%s/config.json.bak", dir) >= (int)sizeof(backup))
        return 0;
    FILE *f = fopen(temp, "wb");
    if (!f)
        return 0;
    int ok =
        fprintf(f, "{\n  \"version\": 1,\n  \"language\": \"%s\"\n}\n", ca_language_code(lang)) > 0;
    if (fflush(f))
        ok = 0;
    if (ok && fsync(fileno(f)))
        ok = 0;
    if (fclose(f))
        ok = 0;
    if (!ok) {
        remove(temp);
        return 0;
    }
    int old = access(path, F_OK) == 0;
    if (old) {
        if (remove(backup) && errno != ENOENT) {
            remove(temp);
            return 0;
        }
        if (rename(path, backup)) {
            remove(temp);
            return 0;
        }
    }
    if (rename(temp, path)) {
        if (old)
            rename(backup, path);
        remove(temp);
        return 0;
    }
    return 1;
}
