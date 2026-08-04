#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Canvas.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_canvas.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 画布控件 demo:一块 96x72 可绘制位图,放大 3x 显示。按下/拖动在画布缓冲上画 3x3 笔刷,
  *	              沿上次点到当前点做线性插值补点(防止快速拖动断线)。演示 GetBuffer 直写 + 绘制回调 +
  *	              IsDrawing 逐帧(指针按住不动也持续画一点,示意喷枪式驱动)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60
#define CW 96
#define CH 72

static GYOBJ g_canvas;
static GYpx   g_ink;
static int32  g_prev_x = -1, g_prev_y = -1;

//在画布缓冲画一个 3x3 点
static void dab(int32 cx, int32 cy)
{
	GYpx* buf = YMGUI_Canvas_GetBuffer(g_canvas);
	for (int32 dy = -1; dy <= 1; dy++)
		for (int32 dx = -1; dx <= 1; dx++)
		{
			int32 x = cx + dx, y = cy + dy;
			if (x >= 0 && x < CW && y >= 0 && y < CH)
				buf[y * CW + x] = g_ink;
		}
}

//从 (x0,y0) 到 (x1,y1) 线性补点
static void stroke(int32 x0, int32 y0, int32 x1, int32 y1)
{
	int32 dx = x1 - x0, dy = y1 - y0;
	int32 adx = dx < 0 ? -dx : dx;
	int32 ady = dy < 0 ? -dy : dy;
	int32 steps = adx > ady ? adx : ady;
	if (steps == 0) { dab(x1, y1); return; }
	for (int32 i = 0; i <= steps; i++)
		dab(x0 + dx * i / steps, y0 + dy * i / steps);
}

static void onPaint(GYOBJ c, GYcanvas_phase phase, int32 cx, int32 cy, uint8 in_bounds)
{
	(void)in_bounds;//越界也画,dab 内部按画布边界裁
	if (phase == GY_CANVAS_DOWN)
	{
		dab(cx, cy);
		g_prev_x = cx; g_prev_y = cy;
	}
	else if (phase == GY_CANVAS_MOVE)
	{
		stroke(g_prev_x, g_prev_y, cx, cy);
		g_prev_x = cx; g_prev_y = cy;
	}
	else //UP
	{
		g_prev_x = g_prev_y = -1;
	}
	YMGUI_Canvas_Invalidate(c);
}

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

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 6, SCR_W, 16);
	YMGUI_Label_SetText(title, "Drag on canvas to draw (3x zoom)");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	g_ink = GY_ColorToPx(GY_ARGB(0xFF, 0x20, 0x80, 0xF0));

	//画布 96x72,放大 3x → 288x216 显示;视口给 296x224
	g_canvas = YMGUI_Creat_Canvas_Creat(ctx->root, 12, 26, 296, 200, CW, CH);
	YMGUI_Canvas_SetZoom(g_canvas, 3);
	YMGUI_Canvas_SetPaintCb(g_canvas, onPaint);

	YMGUI_Inject_SetCtx(ctx);

	while (SDL_LCD_PumpEvents())
	{
		//喷枪式:指针按住不动时(MOVE 不触发)也持续补一点
		int32 cx, cy;
		if (YMGUI_Canvas_IsDrawing(g_canvas, &cx, &cy))
		{
			dab(cx, cy);
			YMGUI_Canvas_Invalidate(g_canvas);
		}
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_canvas exit ok\n");
	return 0;
}
