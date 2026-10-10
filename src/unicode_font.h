#ifndef CALCAERO_UNICODE_FONT_H
#define CALCAERO_UNICODE_FONT_H
#include "font.h"
#include <stdint.h>
#define CA_TEXT_GLYPHS 256
typedef struct {
    unsigned gid;
    float x, y;
    unsigned char font;
} CAGlyph;
typedef struct {
    int count;
    float width;
    CAGlyph glyph[CA_TEXT_GLYPHS];
} CATextLayout;
int ca_font_select(const char *dir, int lang);
void ca_font_close(void);
void ca_font_frame(void);
const CATextLayout *ca_font_layout(const char *utf8);
const PPGlyph *ca_font_glyph(unsigned id, int face);
void ca_font_bearing(unsigned id, int face, float *x, float *y);
int ca_font_faults(void);
#endif
