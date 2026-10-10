#ifndef PP_GFX_H
#define PP_GFX_H
#include <stdint.h>
#define PP_WIDTH 480
#define PP_HEIGHT 272
#define PP_MAX_VERTICES 32768
#define PP_RGB(r,g,b) (UINT32_C(0xff000000) | ((uint32_t)(b)<<16) | ((uint32_t)(g)<<8) | (uint32_t)(r))
#define PP_RGBA(r,g,b,a) (((uint32_t)(a)<<24) | ((uint32_t)(b)<<16) | ((uint32_t)(g)<<8) | (uint32_t)(r))
/* PSP GE vertex order is texture, color, position. */
typedef struct { float u, v; uint32_t color; float x, y, z; } PPVertex;
extern PPVertex pp_vertices[PP_MAX_VERTICES];
extern int pp_vertex_count, pp_vertex_overflow;
extern unsigned char pp_atlas[];
#define PP_ATLAS_SIZE 512
void gfx_init(void);
void gfx_begin(void);
void gfx_end(void);
void gfx_term(void);
void gfx_triangle(float x1,float y1,float x2,float y2,float x3,float y3,uint32_t c);
void gfx_rect(float x,float y,float w,float h,uint32_t c);
void gfx_line(float x1,float y1,float x2,float y2,float width,uint32_t c);
void gfx_line_smooth(float x1,float y1,float x2,float y2,float width,uint32_t c);
void gfx_circle(float x,float y,float r,uint32_t c);
void gfx_round(float x,float y,float w,float h,float r,uint32_t c);
void gfx_gradient(float x,float y,float w,float h,uint32_t top,uint32_t bottom);
void gfx_round_gradient(float x,float y,float w,float h,float r,uint32_t top,uint32_t bottom);
uint32_t gfx_mix(uint32_t a,uint32_t b,float t);
void gfx_text_clipped(float x,float y,const char *s,int scale,uint32_t color,float width);
void gfx_text(float x,float y,const char *s,int scale,uint32_t c);
int gfx_text_width(const char *s,int scale);
void gfx_text_mono(float x,float y,const char *s,uint32_t c);
int gfx_mono_width(void);
void gfx_key_mask(float x,float y,int tile,uint32_t top,uint32_t bottom);
void gfx_button(int button,float x,float y,float r,uint32_t c);
#endif
