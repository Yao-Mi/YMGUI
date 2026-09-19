/* 无 SDL 示例：渲染控件、注入点击并将 framebuffer 写入 PPM。 */
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "YMGUI_Bind.h"
#include <stdio.h>

#define WIDTH 320
#define HEIGHT 240
static GYpx pixels[WIDTH * HEIGHT];
static GYpx band[WIDTH * 16];
static GY_SUBJECT_INT(count, 0);
static int flushes;

static void flush(GYdisp* d, const GYrect* area, const GYpx* buf)
{
    for (int y = 0; y < area->h; ++y)
        for (int x = 0; x < area->w; ++x)
            pixels[(area->y + y) * WIDTH + area->x + x] = buf[y * area->w + x];
    ++flushes;
    YMGUI_Disp_FlushReady(d);
}
static void clicked(GYOBJ button)
{
    (void)button;
    YMGUI_State_SetInt(&count, YMGUI_State_GetInt(&count) + 1);
}
int main(void)
{
    GYdisp display = {0};
    display.hor_res = WIDTH;
    display.ver_res = HEIGHT;
    display.buf1 = band;
    display.buf_px_cnt = WIDTH * 16;
    display.flush_cb = flush;
    GYCTX ctx = YMGUI_Creat_Ctx_Creat(&display, WIDTH, HEIGHT);
    if (!ctx) return 1;
    GYOBJ label = YMGUI_Creat_Label_Creat(ctx->root, 20, 30, 280, 30);
    GYOBJ button = YMGUI_Creat_Button_Creat(ctx->root, 100, 100, 120, 40);
    if (!label || !button) { YMGUI_Free_CtxFree(ctx); return 1; }
    YMGUI_Label_Bind(label, &count);
    YMGUI_Button_SetText(button, "Click me");
    YMGUI_Button_SetClicked(button, clicked);
    YMGUI_Inject_SetCtx(ctx);
    YMGUI_Refresh(ctx);
    YMGUI_Inject_Pointer(150, 120, 1);
    YMGUI_Inject_Pointer(150, 120, 0);
    YMGUI_Refresh(ctx);
    int valid = YMGUI_State_GetInt(&count) == 1 && flushes > 0;
    int previous = flushes;
    YMGUI_Refresh(ctx);
    valid = valid && flushes == previous; /* 无脏区不再 flush。 */
    YMGUI_Inject_SetCtx(NULL);
    YMGUI_Free_CtxFree(ctx);
    if (!valid) return 1;
    FILE* out = fopen("ymgui.ppm", "wb");
    if (!out) return 1;
    if (fprintf(out, "P6\n%d %d\n255\n", WIDTH, HEIGHT) < 0) valid = 0;
    for (int i = 0; valid && i < WIDTH * HEIGHT; ++i)
    {
        GYcolor color = GY_PxToColor(pixels[i]);
        unsigned char rgb[3] = {GY_COLOR_R(color), GY_COLOR_G(color), GY_COLOR_B(color)};
        if (fwrite(rgb, 1, 3, out) != 3) valid = 0;
    }
    if (fclose(out)) valid = 0;
    if (!valid) return 1;
    puts("PASS: render, input, binding, idle refresh; wrote ymgui.ppm");
    return 0;
}
