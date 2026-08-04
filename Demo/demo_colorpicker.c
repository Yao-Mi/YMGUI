#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_ColorPicker.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_colorpicker.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: HSV 取色器 demo:拖动 SV 方块选饱和度/明度,拖动右侧色相条选色相,选中色实时刷到
  *	              右侧预览色块(用 SetBgColor)并把 RGB 打印到状态标签。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60

static GYOBJ g_swatch;
static GYOBJ g_status;

static void onColor(GYOBJ picker, GYcolor color)
{
	(void)picker;
	YMGUI_Obj_SetBgColor(g_swatch, color);
	char buf[48];
	snprintf(buf, sizeof(buf), "R%d G%d B%d",
		(int)GY_COLOR_R(color), (int)GY_COLOR_G(color), (int)GY_COLOR_B(color));
	YMGUI_Label_SetText(g_status, buf);
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
	YMGUI_Label_SetText(title, "Drag SV square / hue bar");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	GYOBJ picker = YMGUI_Creat_ColorPicker_Creat(ctx->root, 16, 30, 200, 180);
	YMGUI_ColorPicker_SetChangedCb(picker, onColor);
	YMGUI_ColorPicker_SetColor(picker, GY_ARGB(0xFF, 0xE0, 0x40, 0x40));

	//预览色块
	g_swatch = YMGUI_Creat_Obj_Creat(ctx->root, 236, 30, 68, 68);
	YMGUI_Obj_SetBgColor(g_swatch, GY_ARGB(0xFF, 0xE0, 0x40, 0x40));

	g_status = YMGUI_Creat_Label_Creat(ctx->root, 236, 108, 80, 16);
	YMGUI_Label_SetText(g_status, "R224 G64 B64");
	YMGUI_Label_SetTextColor(g_status, GY_ARGB(0xFF, 0xA0, 0xE0, 0xA0));

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
	gy_log_print("demo_colorpicker exit ok\n");
	return 0;
}
