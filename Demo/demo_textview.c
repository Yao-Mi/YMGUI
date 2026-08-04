#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_TextView.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_textview.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 多行文本视图 demo:一段含 '\n' 硬换行 + 超宽长句的文本,拖动纵向滚动,
  *	              按钮切换 Wrap(折行/裁切)看长句在两态下的表现。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60

static const char* DOC =
	"YMGUI TextView\n"
	"Drag up/down to scroll.\n"
	"\n"
	"This is a very long paragraph that does not contain any hard line "
	"breaks, so it demonstrates how word-less greedy wrapping folds a "
	"single logical line across the widget width when Wrap is enabled.\n"
	"\n"
	"Line A\n"
	"Line B\n"
	"Line C\n"
	"Line D\n"
	"Line E\n"
	"-- end --";

static GYOBJ g_tv;
static GYOBJ g_btn;
static uint8 g_wrap = 1;

static void onToggle(GYOBJ btn)
{
	g_wrap = g_wrap ? 0 : 1;
	YMGUI_TextView_SetWrap(g_tv, g_wrap);
	YMGUI_Button_SetText(btn, g_wrap ? "Wrap: ON" : "Wrap: OFF");
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
	YMGUI_Label_SetText(title, "Scrollable multi-line text");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	g_tv = YMGUI_Creat_TextView_Creat(ctx->root, 20, 28, 280, 170);
	YMGUI_TextView_SetText(g_tv, DOC);
	YMGUI_TextView_SetWrap(g_tv, g_wrap);

	g_btn = YMGUI_Creat_Button_Creat(ctx->root, 210, 206, 90, 26);
	YMGUI_Button_SetText(g_btn, "Wrap: ON");
	YMGUI_Button_SetClicked(g_btn, onToggle);

	GYOBJ hint = YMGUI_Creat_Label_Creat(ctx->root, 20, 212, 180, 16);
	YMGUI_Label_SetText(hint, "Drag text to scroll");
	YMGUI_Label_SetTextColor(hint, GY_ARGB(0xFF, 0xA0, 0xE0, 0xA0));

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
	gy_log_print("demo_textview exit ok\n");
	return 0;
}
