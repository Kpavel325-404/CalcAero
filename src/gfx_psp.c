#include "gfx.h"
#include "platform.h"
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspuser.h>
#include "l10n.h"
static unsigned int list[8192] __attribute__((aligned(64)));
static uint32_t palette[256] __attribute__((aligned(64)));
void gfx_init(void) {
    sceGuInit();
    for (int i = 0; i < 256; ++i)
        palette[i] = ((uint32_t)i << 24) | UINT32_C(0x00ffffff);
    sceKernelDcacheWritebackRange(palette, sizeof(palette));
    sceKernelDcacheWritebackRange((void *)pp_atlas, PP_ATLAS_SIZE * PP_ATLAS_SIZE);
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
    sceGuDispBuffer(PP_WIDTH, PP_HEIGHT, (void *)0x88000, 512);
    sceGuOffset(2048 - PP_WIDTH / 2, 2048 - PP_HEIGHT / 2);
    sceGuViewport(2048, 2048, PP_WIDTH, PP_HEIGHT);
    sceGuScissor(0, 0, PP_WIDTH, PP_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDepthMask(GU_TRUE);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}
void gfx_end(void) {
    sceKernelDcacheWritebackRange(pp_atlas, PP_ATLAS_SIZE * PP_ATLAS_SIZE);
    sceKernelDcacheWritebackRange(pp_vertices, pp_vertex_count * sizeof(PPVertex));
    sceGuStart(GU_DIRECT, list);
    /* Utility dialogs may use different GU state. Restore the 2D draw state. */
    sceGuOffset(2048 - PP_WIDTH / 2, 2048 - PP_HEIGHT / 2);
    sceGuViewport(2048, 2048, PP_WIDTH, PP_HEIGHT);
    sceGuScissor(0, 0, PP_WIDTH, PP_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDepthMask(GU_TRUE);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_COLOR_LOGIC_OP);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuShadeModel(GU_SMOOTH);
    sceGuClutMode(GU_PSM_8888, 0, 255, 0);
    sceGuClutLoad(32, palette);
    sceGuTexMode(GU_PSM_T8, 0, 0, 0);
    sceGuTexImage(0, PP_ATLAS_SIZE, PP_ATLAS_SIZE, PP_ATLAS_SIZE, pp_atlas);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuClearColor(PP_RGB(74, 159, 199));
    sceGuClear(GU_COLOR_BUFFER_BIT);
    /* Keep drawing order, but only sample the atlas for letters. Glass panels
       are colored vertices and need no texture lookup for each screen pixel. */
    for (int start = 0; start < pp_vertex_count;) {
        int textured = pp_vertices[start].u != 2 || pp_vertices[start].v != 2;
        int end = start + 3;
        while (end < pp_vertex_count &&
               ((pp_vertices[end].u != 2 || pp_vertices[end].v != 2) == textured))
            end += 3;
        if (textured) sceGuEnable(GU_TEXTURE_2D); else sceGuDisable(GU_TEXTURE_2D);
        sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                       end - start, 0, pp_vertices + start);
        start = end;
    }
    sceGuFinish();
    sceGuSync(0, 0);
    cl_platform_overlay();
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}
void gfx_term(void) { sceGuTerm(); }
