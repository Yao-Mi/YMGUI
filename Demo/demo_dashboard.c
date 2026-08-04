#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_ArcWidget.h"
#include "YMGUI_Spinner.h"
#include "YMGUI_Meter.h"
#include "YMGUI_Image.h"
#include "SDL_LCD.h"
#include <stdlib.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_dashboard.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 显示控件组合 demo:环形进度 + 仪表盘 + 加载转圈 + 图片,值随帧动画
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

	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL;
	disp.user_data = NULL;

	SDL_LCD_Init(&disp, 2);
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x12, 0x14, 0x1C));

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 6, SCR_W, 16);
	YMGUI_Label_SetText(title, "YMGUI Dashboard");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	//环形进度
	GYOBJ arc = YMGUI_Creat_Arc_Creat(ctx->root, 20, 40, 90, 90);
	YMGUI_Arc_SetRange(arc, 0, 100);
	YMGUI_Arc_SetWidth(arc, 8);

	//仪表盘
	GYOBJ meter = YMGUI_Creat_Meter_Creat(ctx->root, 200, 40, 100, 100);
	YMGUI_Meter_SetRange(meter, 0, 100);
	YMGUI_Meter_SetTicks(meter, 7);

	//加载转圈
	GYOBJ sp = YMGUI_Creat_Spinner_Creat(ctx->root, 130, 150, 60, 60);
	YMGUI_Spinner_SetSpan(sp, 100, 24);

	YMGUI_Inject_SetCtx(ctx);

	int32 v = 0, dir = 1;
	while (SDL_LCD_PumpEvents())
	{
		//值来回摆动
		v += dir * 2;
		if (v >= 100) { v = 100; dir = -1; }
		if (v <= 0)   { v = 0;   dir = 1; }
		YMGUI_Arc_SetValue(arc, v);
		YMGUI_Meter_SetValue(meter, v);
		YMGUI_Spinner_Tick(sp);//每帧转一步

		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(33);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_dashboard exit ok\n");
	return 0;
}
