#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "YMGUI_Bind.h"
#include "SDL_LCD.h"
#include <stdlib.h>

static GY_SUBJECT_INT(clicks, 0);

static void on_click(GYOBJ button)
{
    (void)button;
    YMGUI_State_SetInt(&clicks, YMGUI_State_GetInt(&clicks) + 1);
}

int main(int argc, char** argv)
{
    int result = 1;
    int frames = argc > 1 ? atoi(argv[1]) : -1;
    GYCTX ctx = NULL;
    GYdisp disp = {0};
    disp.hor_res = 320;
    disp.ver_res = 240;
    disp.buf_px_cnt = 320 * 32;
    disp.buf1 = GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
    if (!disp.buf1 || SDL_LCD_Init(&disp, 1) != 0)
        goto done;
    ctx = YMGUI_Creat_Ctx_Creat(&disp, 320, 240);
    if (!ctx)
        goto done;
    GYOBJ label = YMGUI_Creat_Label_Creat(ctx->root, 20, 30, 280, 30);
    GYOBJ button = YMGUI_Creat_Button_Creat(ctx->root, 100, 100, 120, 40);
    if (!label || !button)
        goto done;
    YMGUI_Label_Bind(label, &clicks);
    YMGUI_Button_SetText(button, "Click me");
    YMGUI_Button_SetClicked(button, on_click);
    YMGUI_Inject_SetCtx(ctx);
    while (frames != 0 && SDL_LCD_PumpEvents())
    {
        YMGUI_Refresh(ctx);
        SDL_LCD_Delay(16);
        if (frames > 0)
            --frames;
    }
    result = 0;
done:
    YMGUI_Inject_SetCtx(NULL);
    if (ctx)
        YMGUI_Free_CtxFree(ctx);
    SDL_LCD_Destroy();
    GY_free1(disp.buf1);
    return result;
}
