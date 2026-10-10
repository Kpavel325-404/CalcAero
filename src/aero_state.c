#include "aero.h"
#include "store.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "l10n.h"
#define AERO_BYTES 2852
static void put32(unsigned char *b, size_t *at, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        b[(*at)++] = (unsigned char)(v >> (8 * i));
}
static uint32_t get32(const unsigned char *b, size_t *at) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v |= (uint32_t)b[(*at)++] << (8 * i);
    return v;
}
static int valid(const AeroState *s) {
    if (s->pretty < 0 || s->pretty > 1 || s->matrix_n < 1 || s->matrix_n > 4 ||
        s->study_topic >= AERO_STUDY_TOPICS || s->study_correct > s->study_total ||
        s->study_total > 1000000000)
        return 0;
    for (int i = 0; i < 2; ++i)
        if (!memchr(s->graph[i], 0, CL_EXPR))
            return 0;
    for (int i = 0; i < 3; ++i)
        if (!memchr(s->function[i], 0, CL_EXPR))
            return 0;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < 16; ++i)
            if (!isfinite(s->matrix[k][i]))
                return 0;
    for (int i = 0; i < 3; ++i)
        if (s->function[i][0]) {
            CLProgram p;
            CLError e;
            char call[8];
            snprintf(call, sizeof(call), "%c(x)", 'f' + i);
            if (!aero_compile(call, s, &p, &e))
                return 0;
        }
    return 1;
}
static int encode(const AeroState *s, unsigned char b[AERO_BYTES]) {
    if (!valid(s))
        return 0;
    memset(b, 0, AERO_BYTES);
    memcpy(b, "CAERO012", 8);
    size_t at = 8;
    put32(b, &at, (uint32_t)s->pretty);
    put32(b, &at, (uint32_t)s->matrix_n);
    put32(b, &at, s->study_seed);
    put32(b, &at, s->study_topic);
    put32(b, &at, s->study_total);
    put32(b, &at, s->study_correct);
    for (int i = 0; i < 3; ++i) {
        memcpy(b + at, s->function[i], strlen(s->function[i]));
        at += CL_EXPR;
    }
    for (int i = 0; i < 2; ++i) {
        memcpy(b + at, s->graph[i], strlen(s->graph[i]));
        at += CL_EXPR;
    }
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < 16; ++i) {
            uint64_t v;
            memcpy(&v, &s->matrix[k][i], 8);
            for (int j = 0; j < 8; ++j)
                b[at++] = (unsigned char)(v >> (8 * j));
        }
    if (at != AERO_BYTES - 4)
        return 0;
    put32(b, &at, cl_crc32(b, AERO_BYTES - 4));
    return 1;
}
static int decode(const unsigned char b[AERO_BYTES], AeroState *s) {
    size_t checksum_at = AERO_BYTES - 4;
    if (memcmp(b, "CAERO012", 8) || get32(b, &checksum_at) != cl_crc32(b, AERO_BYTES - 4))
        return 0;
    size_t at = 8;
    memset(s, 0, sizeof(*s));
    s->pretty = (int)get32(b, &at);
    s->matrix_n = (int)get32(b, &at);
    s->study_seed = get32(b, &at);
    s->study_topic = get32(b, &at);
    s->study_total = get32(b, &at);
    s->study_correct = get32(b, &at);
    for (int i = 0; i < 3; ++i) {
        memcpy(s->function[i], b + at, CL_EXPR);
        at += CL_EXPR;
    }
    for (int i = 0; i < 2; ++i) {
        memcpy(s->graph[i], b + at, CL_EXPR);
        at += CL_EXPR;
    }
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < 16; ++i) {
            uint64_t v = 0;
            for (int j = 0; j < 8; ++j)
                v |= (uint64_t)b[at++] << (8 * j);
            memcpy(&s->matrix[k][i], &v, 8);
        }
    return at == AERO_BYTES - 4 && valid(s);
}
static int read_state(const char *file, AeroState *s) {
    unsigned char b[AERO_BYTES];
    FILE *f = fopen(file, "rb");
    if (!f)
        return 0;
    int ok = fread(b, 1, sizeof(b), f) == sizeof(b) && fgetc(f) == EOF && !ferror(f);
    if (fclose(f))
        ok = 0;
    return ok && decode(b, s);
}
static int filename(char out[700], const char *dir, const char *name) {
    int n = snprintf(out, 700, "%s/%s", dir, name);
    return n >= 0 && n < 700;
}
int aero_state_load(const char *dir, AeroState *s) {
    char file[700];
    AeroState result;
    if (!filename(file, dir, "aero.dat"))
        return 0;
    if (read_state(file, &result)) {
        *s = result;
        return 1;
    }
    if (!filename(file, dir, "aero.dat.bak"))
        return 0;
    if (read_state(file, &result)) {
        *s = result;
        return 2;
    }
    return 0;
}
int aero_state_save(const char *dir, const AeroState *s) {
    unsigned char b[AERO_BYTES];
    AeroState verification;
    char main[700], backup[700], temp[700];
    if (!filename(main, dir, "aero.dat") || !filename(backup, dir, "aero.dat.bak") ||
        !filename(temp, dir, "aero.tmp") || !encode(s, b))
        return 0;
    FILE *f = fopen(temp, "wb");
    if (!f)
        return 0;
    int ok = fwrite(b, 1, sizeof(b), f) == sizeof(b) && fflush(f) == 0;
    if (fclose(f))
        ok = 0;
    if (!ok || !read_state(temp, &verification)) {
        unlink(temp);
        return 0;
    }
    if (read_state(main, &verification)) {
        if (unlink(backup) != 0 && errno != ENOENT)
            return 0;
        if (rename(main, backup) != 0)
            return 0;
    } else if (unlink(main) != 0 && errno != ENOENT)
        return 0;
    if (rename(temp, main) == 0)
        return 1;
    /* Keep a readable primary if a final rename fails after rotation. */
    if (access(main, F_OK) != 0)
        rename(backup, main);
    return 0;
}
