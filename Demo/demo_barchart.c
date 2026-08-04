#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_BarChart.h"
#include "YMGUI_Trig.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_barchart.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 通用柱状图 demo。定点三角表(无 libm)按帧生成跳动值流式喂进 BarChart,每帧 Tick
  *	              让柱身与顶标衰减回落。每 ~3 秒轮换一次"配色模式 × 顶部回落模式"四种组合,直观演示
  *	              同一控件既能当频谱(BY_HEIGHT+BAR),也能当类目统计柱图(PER_BAR+NONE)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 400
#define SCR_H 240
#define BAND_H 64
#define BARS   24

//四种组合(配色模式, 顶部回落模式, 说明)
static const struct { uint8 color; uint8 top; const char* name; } MODES[] = {
	{ GY_BARCHART_COLOR_BY_HEIGHT, GY_BARCHART_TOP_BAR,  "BY_HEIGHT + TOP_BAR  (spectrum)" },
	{ GY_BARCHART_COLOR_BY_HEIGHT, GY_BARCHART_TOP_NONE, "BY_HEIGHT + TOP_NONE (plain bars)" },
	{ GY_BARCHART_COLOR_PER_BAR,   GY_BARCHART_TOP_BAR,  "PER_BAR + TOP_BAR    (palette+peak)" },
	{ GY_BARCHART_COLOR_PER_BAR,   GY_BARCHART_TOP_NONE, "PER_BAR + TOP_NONE   (category bars)" },
};
#define N_MODES ((int)(sizeof(MODES) / sizeof(MODES[0])))

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
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x10, 0x10, 0x16));

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 8, SCR_W, 16);
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	GYOBJ bc = YMGUI_Creat_BarChart_Creat(ctx->root, 20, 40, 360, 180);
	YMGUI_BarChart_SetBarCount(bc, BARS);
	YMGUI_BarChart_SetRange(bc, 100);
	YMGUI_BarChart_SetGap(bc, 3);
	YMGUI_BarChart_SetDecay(bc, 5, 1);
	YMGUI_BarChart_SetGradient(bc, GY_ARGB(0xFF, 0x20, 0x80, 0xF0), GY_ARGB(0xFF, 0xF0, 0x40, 0x40));
	YMGUI_BarChart_SetTopColor(bc, GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	YMGUI_BarChart_SetBgColor(bc, GY_ARGB(0xFF, 0x10, 0x10, 0x16));

	//PER_BAR 用的一版彩虹调色板(HSV 感的整数近似:绕一圈)
	GYcolor pal[BARS];
	for (int i = 0; i < BARS; i++)
	{
		int32 r = (GY_Sin((i * 360 / BARS) % 360) + 32768) >> 8;         //0..255
		int32 g = (GY_Sin((i * 360 / BARS + 120) % 360) + 32768) >> 8;
		int32 b = (GY_Sin((i * 360 / BARS + 240) % 360) + 32768) >> 8;
		pal[i] = GY_ARGB(0xFF, (uint8)r, (uint8)g, (uint8)b);
	}
	YMGUI_BarChart_SetBarColors(bc, pal, BARS);

	YMGUI_Inject_SetCtx(ctx);

	int t = 0;
	int cur_mode = -1;
	while (SDL_LCD_PumpEvents())
	{
		//每 ~180 帧(≈3s)轮换一次模式组合
		int m = (frame / 180) % N_MODES;
		if (m != cur_mode)
		{
			cur_mode = m;
			YMGUI_BarChart_SetColorMode(bc, MODES[m].color);
			YMGUI_BarChart_SetTopMode(bc, MODES[m].top);
			YMGUI_Label_SetText(title, MODES[m].name);
		}

		//每 3 帧喂一批新值:每柱一个不同相位/频率的定点正弦,叠加成"跳动的数据"
		if (frame % 3 == 0)
		{
			for (int i = 0; i < BARS; i++)
			{
				int32 s1 = GY_Sin((t * 7 + i * 30) % 360);   //Q15
				int32 s2 = GY_Sin((t * 13 + i * 50) % 360);
				int32 mag = (s1 * s1 + s2 * s2) >> 20;         //~0..64
				int32 v = 20 + mag + (i % 4) * 5;
				if (v > 100) v = 100;
				YMGUI_BarChart_SetValue(bc, i, v);
			}
			t += 9;
		}
		YMGUI_BarChart_Tick(bc);

		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
		if (max_frames <= 0) frame++;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_barchart exit ok\n");
	return 0;
}
