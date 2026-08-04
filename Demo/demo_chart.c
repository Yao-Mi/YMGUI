#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Chart.h"
#include "YMGUI_Trig.h"
#include "SDL_LCD.h"
#include <stdlib.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_chart.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-03
  *	@Description: 折线图 demo:两条流式序列(正弦/锯齿),每帧 SetNext 左移滚动
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60
#define PTS    40

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
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x10, 0x12, 0x18));

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 6, SCR_W, 16);
	YMGUI_Label_SetText(title, "YMGUI Chart");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0x40, 0xE0, 0xC0));

	GYOBJ ch = YMGUI_Creat_Chart_Creat(ctx->root, 10, 30, 300, 200);
	YMGUI_Chart_SetRange(ch, 0, 100);
	YMGUI_Chart_SetPointCount(ch, PTS);
	YMGUI_Chart_SetGrid(ch, 4, 8);
	int32 s_sin = YMGUI_Chart_AddSeries(ch, GY_ARGB(0xFF, 0x40, 0xC0, 0xF0));
	int32 s_saw = YMGUI_Chart_AddSeries(ch, GY_ARGB(0xFF, 0xF0, 0x90, 0x30));

	YMGUI_Inject_SetCtx(ctx);

	int32 ang = 0;
	int32 saw = 0;
	while (SDL_LCD_PumpEvents())
	{
		//正弦:50 + 45*sin(ang) (Q15 定点三角还原到 0..100 附近)
		int32 sv = 50 + (45 * GY_Sin(ang)) / (1 << GY_TRIG_SHIFT);
		YMGUI_Chart_SetNext(ch, s_sin, sv);
		//锯齿:0..100 循环
		YMGUI_Chart_SetNext(ch, s_saw, saw);
		ang = (ang + 18) % 360;
		saw = (saw + 8) % 100;

		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(50);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_chart exit ok\n");
	return 0;
}
