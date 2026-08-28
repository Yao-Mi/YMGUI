#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_DrawArc.h"
#include "YMGUI_DrawImg.h"
#include "YMGUI_Trig.h"
#include "SDL_LCD.h"
#include <stdlib.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_draw.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 图元展示 demo:自定义 draw_cb 的对象里画线/圆/弧/图片
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

//一张 8x8 小图(棋盘格,青/洋红)
static GYpx s_icon[8 * 8];
static void makeIcon(void)
{
	GYpx a = GY_ColorToPx(GY_ARGB(0xFF, 0x30, 0xE0, 0xE0));
	GYpx b = GY_ColorToPx(GY_ARGB(0xFF, 0xE0, 0x30, 0xE0));
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
			s_icon[y * 8 + x] = ((x + y) & 1) ? a : b;
}

//画布对象:在自己区域里画各种图元
static void canvasDraw(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	(void)obj;
	//星形放射线
	GYcoord cx = abs->x + 70, cy = abs->y + 70;
	for (int a = 0; a < 360; a += 30)
		YMGUI_Draw_Line(s, cx, cy,
			cx + (GYcoord)(((int32)55 * GY_Cos(a)) >> 15),
			cy + (GYcoord)(((int32)55 * GY_Sin(a)) >> 15),
			GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));
	//同心圆
	YMGUI_Draw_Circle(s, abs->x + 210, cy, 40, GY_ARGB(0xFF, 0x40, 0xC0, 0x50));
	YMGUI_Draw_Circle(s, abs->x + 210, cy, 25, GY_ARGB(0xFF, 0x40, 0x80, 0xE0));
	YMGUI_Draw_CircleFill(s, abs->x + 210, cy, 8, GY_ARGB(0xFF, 0xE0, 0x40, 0x40));
	//下方进度弧(0..270 度)
	YMGUI_Draw_Arc(s, abs->x + 140, abs->y + 175, 45, 135, 45, GY_ARGB(0xFF, 0xE0, 0x80, 0x30));
	//图片 blit(原尺寸,直接贴几个)
	GYimg img = {s_icon, 8, 8, 0, GY_PX_ZERO};
	for (int i = 0; i < 4; i++)
		YMGUI_Draw_Img(s, &img, abs->x + 10 + i * 12, abs->y + 150);
	//缩放 blit:同一张 8x8 图逐级放大(2x/4x/6x),展示 Draw_ImgScaled
	YMGUI_Draw_ImgScaled(s, &img, (GYrect){abs->x + 10,  abs->y + 165, 16, 16});
	YMGUI_Draw_ImgScaled(s, &img, (GYrect){abs->x + 32,  abs->y + 165, 32, 32});
	YMGUI_Draw_ImgScaled(s, &img, (GYrect){abs->x + 70,  abs->y + 165, 48, 48});
}

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;

	makeIcon();
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL;
	disp.user_data = NULL;

	SDL_LCD_Init(&disp, 2);
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x10, 0x10, 0x18));

	//全屏画布对象,挂自定义 draw_cb
	GYOBJ canvas = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, SCR_W, SCR_H);
	canvas->draw_cb = canvasDraw;
	canvas->bg_color = GY_ARGB(0xFF, 0x10, 0x10, 0x18);
	YMGUI_Obj_Invalidate(canvas);

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
	gy_log_print("demo_draw exit ok\n");
	return 0;
}
