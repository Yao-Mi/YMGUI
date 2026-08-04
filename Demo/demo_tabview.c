#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_Tabview.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_tabview.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-31
  *	@Description: 标签页 demo:3 页(各放不同控件),点顶部 tab 切页;切页时页内控件随页显隐
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;

	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;

	SDL_LCD_Init(&disp, 2);
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));

	GYOBJ tv = YMGUI_Creat_Tabview_Creat(ctx->root, 10, 10, 300, 220);
	YMGUI_Tabview_SetBarHeight(tv, 28);

	//页 0:一个标签 + 一个按钮
	GYOBJ p0 = YMGUI_Tabview_AddTab(tv, "Home");
	GYOBJ l0 = YMGUI_Creat_Label_Creat(p0, 20, 20, 260, 16);
	YMGUI_Label_SetText(l0, "Welcome to the Home tab");
	GYOBJ b0 = YMGUI_Creat_Button_Creat(p0, 20, 60, 120, 32);
	YMGUI_Button_SetText(b0, "OK");

	//页 1:两个复选框
	GYOBJ p1 = YMGUI_Tabview_AddTab(tv, "Settings");
	GYOBJ cb1 = YMGUI_Creat_Checkbox_Creat(p1, 20, 20, 200, 20);
	YMGUI_Checkbox_SetText(cb1, "Enable sound");
	GYOBJ cb2 = YMGUI_Creat_Checkbox_Creat(p1, 20, 50, 200, 20);
	YMGUI_Checkbox_SetText(cb2, "Dark mode");

	//页 2:一段文字
	GYOBJ p2 = YMGUI_Tabview_AddTab(tv, "About");
	GYOBJ l2 = YMGUI_Creat_Label_Creat(p2, 20, 30, 260, 16);
	YMGUI_Label_SetText(l2, "YMGUI - bare-metal GUI");

	YMGUI_Inject_SetCtx(ctx);

	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_tabview exit ok\n");
	return 0;
}
