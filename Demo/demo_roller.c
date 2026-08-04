#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Roller.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_roller.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 滚轮/居中高亮平滑滚动列表 demo。左侧交互滚轮(拖动滚动+抬起吸附),右侧程序驱动
  *	              滚轮(每 40 帧自动 SetSelected 到下一项,缓动平滑滚动)。每帧 Tick 两个滚轮推进
  *	              动画。演示"一个通用居中高亮滚动控件"可交互也可纯程序驱动(歌词/时间/日历同款)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 360
#define SCR_H 260
#define BAND_H 64

static const char* const fruits[10] = {
	"Apple", "Banana", "Cherry", "Durian", "Elderberry",
	"Fig", "Grape", "Honeydew", "Kiwi", "Lemon"
};

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
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x14, 0x14, 0x1C));

	GYOBJ t1 = YMGUI_Creat_Label_Creat(ctx->root, 20, 6, 150, 16);
	YMGUI_Label_SetText(t1, "Drag me");
	YMGUI_Label_SetTextColor(t1, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));
	GYOBJ t2 = YMGUI_Creat_Label_Creat(ctx->root, 200, 6, 150, 16);
	YMGUI_Label_SetText(t2, "Auto");
	YMGUI_Label_SetTextColor(t2, GY_ARGB(0xFF, 0x60, 0xC0, 0xF0));

	//左:交互滚轮
	GYOBJ ri = YMGUI_Creat_Roller_Creat(ctx->root, 20, 30, 150, 210);
	YMGUI_Roller_SetLines(ri, fruits, 10);
	YMGUI_Roller_SetRowHeight(ri, 34);
	YMGUI_Roller_SetColors(ri, GY_ARGB(0xFF, 0x20, 0x20, 0x2A),
	                       GY_ARGB(0xFF, 0x70, 0x70, 0x78), GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF),
	                       GY_ARGB(0xFF, 0x30, 0x50, 0x80));
	YMGUI_Roller_SetInteractive(ri, 1);
	YMGUI_Roller_SetSelected(ri, 2, 0);

	//右:程序驱动滚轮
	GYOBJ rp = YMGUI_Creat_Roller_Creat(ctx->root, 200, 30, 150, 210);
	YMGUI_Roller_SetLines(rp, fruits, 10);
	YMGUI_Roller_SetRowHeight(rp, 34);
	YMGUI_Roller_SetColors(rp, GY_ARGB(0xFF, 0x20, 0x20, 0x2A),
	                       GY_ARGB(0xFF, 0x70, 0x70, 0x78), GY_ARGB(0xFF, 0x80, 0xF0, 0x80),
	                       GY_ARGB(0xFF, 0x20, 0x50, 0x30));
	YMGUI_Roller_SetEaseDiv(rp, 6);

	YMGUI_Inject_SetCtx(ctx);

	int auto_idx = 0;
	while (SDL_LCD_PumpEvents())
	{
		//程序驱动:每 40 帧切下一项
		if (frame % 40 == 0)
		{
			auto_idx = (auto_idx + 1) % 10;
			YMGUI_Roller_SetSelected(rp, auto_idx, 1);
		}
		//每帧推进两个滚轮动画
		YMGUI_Roller_Tick(ri);
		YMGUI_Roller_Tick(rp);

		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
		if (max_frames <= 0) frame++;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_roller exit ok\n");
	return 0;
}
