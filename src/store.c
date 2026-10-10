#include "store.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "l10n.h"
#define STATE_SIZE 33000
typedef struct {
    unsigned char *p;
    size_t at;
    int ok;
} Stream;
uint32_t cl_crc32(const unsigned char *p, size_t n) {
    uint32_t c = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int b = 0; b < 8; ++b)
            c = (c >> 1) ^ ((c & 1) ? UINT32_C(0xedb88320) : 0);
    }
    return ~c;
}
static void put32(Stream *s, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        s->p[s->at++] = (unsigned char)(v >> (8 * i));
}
static uint32_t get32(Stream *s) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v |= (uint32_t)s->p[s->at++] << (8 * i);
    return v;
}
static void put64(Stream *s, uint64_t v) {
    for (int i = 0; i < 8; ++i)
        s->p[s->at++] = (unsigned char)(v >> (8 * i));
}
static uint64_t get64(Stream *s) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= (uint64_t)s->p[s->at++] << (8 * i);
    return v;
}
static void putdouble(Stream *s, double d) {
    uint64_t v;
    memcpy(&v, &d, 8);
    put64(s, v);
}
static double getdouble(Stream *s) {
    uint64_t v = get64(s);
    double d;
    memcpy(&d, &v, 8);
    return d;
}
static void putvalue(Stream *s, CLValue v) {
    putdouble(s, v.re);
    putdouble(s, v.im);
    put64(s, (uint64_t)v.num);
    put64(s, (uint64_t)v.den);
    put32(s, (uint32_t)v.exact);
}
static CLValue getvalue(Stream *s) {
    CLValue v;
    v.re = getdouble(s);
    v.im = getdouble(s);
    uint64_t n = get64(s), d = get64(s);
    memcpy(&v.num, &n, 8);
    memcpy(&v.den, &d, 8);
    v.exact = (int)get32(s);
    if (!cl_value_valid(v))
        s->ok = 0;
    return v;
}
static void putctx(Stream *s, const CLContext *c) {
    put32(s, (uint32_t)c->angle);
    put32(s, (uint32_t)c->complex_mode);
    putvalue(s, c->ans);
    putvalue(s, c->x);
    for (int i = 0; i < 6; ++i)
        putvalue(s, c->mem[i]);
}
static void getctx(Stream *s, CLContext *c) {
    c->angle = (CLAngle)get32(s);
    c->complex_mode = (int)get32(s);
    c->ans = getvalue(s);
    c->x = getvalue(s);
    for (int i = 0; i < 6; ++i)
        c->mem[i] = getvalue(s);
    if (c->angle > CL_GRAD || c->complex_mode < 0 || c->complex_mode > 1)
        s->ok = 0;
}
static void putstring(Stream *s, const char *text) {
    memset(s->p + s->at, 0, CL_EXPR);
    size_t n = strnlen(text, CL_EXPR);
    if (n >= CL_EXPR) {
        s->ok = 0;
        n = CL_EXPR - 1;
    }
    memcpy(s->p + s->at, text, n);
    s->at += CL_EXPR;
}
static void getstring(Stream *s, char *text) {
    memcpy(text, s->p + s->at, CL_EXPR);
    s->at += CL_EXPR;
    if (!memchr(text, 0, CL_EXPR)) {
        s->ok = 0;
        text[CL_EXPR - 1] = 0;
    }
    for (size_t i = 0; text[i]; ++i)
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126)
            s->ok = 0;
}
static int encode(const CLPersistent *c, unsigned char *buffer) {
    memset(buffer, 0, STATE_SIZE);
    memcpy(buffer, "CLSAVE01", 8);
    Stream s = {buffer, 8, 1};
    put32(&s, STATE_SIZE);
    put32(&s, 1);
    putctx(&s, &c->ctx);
    put32(&s, (uint32_t)c->digits);
    put32(&s, (uint32_t)c->style);
    put32(&s, (uint32_t)c->fraction);
    put32(&s, (uint32_t)c->theme);
    put32(&s, (uint32_t)c->history_count);
    putstring(&s, c->expr);
    for (int i = 0; i < CL_BOOKMARKS; ++i)
        putstring(&s, c->bookmark[i]);
    for (int i = 0; i < CL_HISTORY; ++i) {
        putstring(&s, c->history[i].expr);
        putvalue(&s, c->history[i].value);
        put32(&s, (uint32_t)c->history[i].angle);
        put32(&s, (uint32_t)c->history[i].complex_mode);
        putctx(&s, &c->history[i].context);
    }
    if (s.at > STATE_SIZE - 4)
        return 0;
    s.at = STATE_SIZE - 4;
    put32(&s, cl_crc32(buffer, STATE_SIZE - 4));
    return s.ok;
}
static int decode(unsigned char *buffer, CLPersistent *c) {
    if (memcmp(buffer, "CLSAVE01", 8))
        return 0;
    Stream s = {buffer, 8, 1};
    if (get32(&s) != STATE_SIZE || get32(&s) != 1)
        return 0;
    Stream tail = {buffer, STATE_SIZE - 4, 1};
    if (get32(&tail) != cl_crc32(buffer, STATE_SIZE - 4))
        return 0;
    memset(c, 0, sizeof(*c));
    getctx(&s, &c->ctx);
    c->digits = (int)get32(&s);
    c->style = (int)get32(&s);
    c->fraction = (int)get32(&s);
    c->theme = (int)get32(&s);
    c->history_count = (int)get32(&s);
    getstring(&s, c->expr);
    if (c->digits < 6 || c->digits > 15 || c->style < 0 || c->style > 2 || c->fraction < 0 ||
        c->fraction > 1 || c->theme < 0 || c->theme > 7 || c->history_count < 0 ||
        c->history_count > CL_HISTORY)
        return 0;
    for (int i = 0; i < CL_BOOKMARKS; ++i)
        getstring(&s, c->bookmark[i]);
    for (int i = 0; i < CL_HISTORY; ++i) {
        CLHistory *h = &c->history[i];
        getstring(&s, h->expr);
        h->value = getvalue(&s);
        h->angle = (int)get32(&s);
        h->complex_mode = (int)get32(&s);
        getctx(&s, &h->context);
        if (h->angle < 0 || h->angle > 2 || h->complex_mode < 0 || h->complex_mode > 1)
            s.ok = 0;
    }
    return s.ok;
}
static int path(char *out, size_t cap, const char *dir, const char *name) {
    int n = snprintf(out, cap, "%s/%s", dir, name);
    return n >= 0 && (size_t)n < cap;
}
static int read_state(const char *p, unsigned char *bytes, CLPersistent *s) {
    FILE *f = fopen(p, "rb");
    if (!f)
        return 0;
    int ok = fread(bytes, 1, STATE_SIZE, f) == STATE_SIZE && fgetc(f) == EOF && !ferror(f);
    if (fclose(f) != 0)
        ok = 0;
    return ok && decode(bytes, s);
}
int cl_store_load(const char *dir, CLPersistent *s) {
    char p[700];
    unsigned char *bytes = malloc(STATE_SIZE);
    CLPersistent *tmp = malloc(sizeof(*tmp));
    if (!bytes || !tmp) {
        free(bytes);
        free(tmp);
        return 0;
    }
    int result = 0;
    if (path(p, sizeof(p), dir, "state.dat") && read_state(p, bytes, tmp))
        result = 1;
    else if (path(p, sizeof(p), dir, "state.dat.bak") && read_state(p, bytes, tmp))
        result = 2;
    if (result)
        *s = *tmp;
    free(bytes);
    free(tmp);
    return result;
}
int cl_store_save(const char *dir, const CLPersistent *s) {
    char main[700], backup[700], temp[700];
    if (!path(main, sizeof(main), dir, "state.dat") ||
        !path(backup, sizeof(backup), dir, "state.dat.bak") ||
        !path(temp, sizeof(temp), dir, "state.tmp"))
        return 0;
    unsigned char *bytes = malloc(STATE_SIZE);
    CLPersistent *verify = malloc(sizeof(*verify));
    if (!bytes || !verify) {
        free(bytes);
        free(verify);
        return 0;
    }
    int ok = encode(s, bytes) && decode(bytes, verify);
    if (!ok)
        goto done;
    FILE *f = fopen(temp, "wb");
    if (!f) {
        ok = 0;
        goto done;
    }
    ok = fwrite(bytes, 1, STATE_SIZE, f) == STATE_SIZE && fflush(f) == 0;
    if (fclose(f) != 0)
        ok = 0;
    if (!ok || !read_state(temp, bytes, verify)) {
        ok = 0;
        goto done;
    }
    if (read_state(main, bytes, verify)) {
        if (unlink(backup) != 0 && errno != ENOENT) {
            ok = 0;
            goto done;
        }
        if (rename(main, backup) != 0) {
            ok = 0;
            goto done;
        }
    } else if (unlink(main) != 0 && errno != ENOENT) {
        ok = 0;
        goto done;
    }
    ok = rename(temp, main) == 0;
done:
    free(bytes);
    free(verify);
    return ok;
}
int cl_export_history(const char *dir, const CLPersistent *s, char *name, size_t cap) {
    char folder[700], p[800];
    if (!path(folder, sizeof(folder), dir, "reports"))
        return 0;
    struct stat info;
    if (stat(folder, &info) == 0) {
        if (!S_ISDIR(info.st_mode))
            return 0;
    } else if (mkdir(folder, 0777) != 0) {
        if (stat(folder, &info) != 0 || !S_ISDIR(info.st_mode))
            return 0;
    }
    int fd = -1;
    for (int i = 1; i <= 9999; ++i) {
        snprintf(name, cap, "CalcAero-%04d.txt", i);
        if (!path(p, sizeof(p), folder, name))
            return 0;
        if (stat(p, &info) == 0)
            continue;
        fd = open(p, O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (fd >= 0)
            break;
        if (errno != EEXIST && stat(p, &info) != 0)
            return 0;
    }
    if (fd < 0)
        return 0;
    FILE *f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        remove(p);
        return 0;
    }
    fprintf(f, "CalcAero 1.3.1\nCalculation history (newest first)\n~= means an approximate "
               "result; angle units are saved for each item.\n\n");
    for (int i = 0; i < s->history_count; ++i) {
        const CLHistory *h = &s->history[i];
        char v[160];
        cl_format(h->value, 15, 0, 1, v, sizeof(v));
        fprintf(f, "%02d. [%s / %s] %s\n    %s %s\n", i + 1,
                h->angle == 0   ? "DEG"
                : h->angle == 1 ? "RAD"
                                : "GRAD",
                h->complex_mode ? "complex" : "real", h->expr, h->value.exact ? "=" : "~=", v);
    }
    int ok = !ferror(f) && fflush(f) == 0;
    if (fclose(f) != 0)
        ok = 0;
    if (!ok)
        remove(p);
    return ok;
}
