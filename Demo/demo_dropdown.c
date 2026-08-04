#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Dropdown.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_dropdown.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-31
  *	@Description: 下拉框 demo:两个 Dropdown,点击展开浮动菜单(挂 top_layer,盖过下方内容),
  *	              选项点击后合起并把选择同步到状态标签;底部 Dropdown 菜单向上翻。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60

static GYOBJ g_status;
static const char* g_colors[] = {"Red", "Green", "Blue", "Amber"};
static const char* g_speeds[] = {"Slow", "Normal", "Fast"};

static void onColor(GYOBJ dd, uint16 sel)
{
	(void)dd;
	char buf[48];
	snprintf(buf, sizeof(buf), "Color = %s", g_colors[sel]);
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

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 8, SCR_W, 16);
	YMGUI_Label_SetText(title, "Click a dropdown");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	//顶部下拉:菜单朝下
	GYOBJ ddColor = YMGUI_Creat_Dropdown_Creat(ctx->root, 40, 40, 120, 26);
	for (int i = 0; i < 4; i++) YMGUI_Dropdown_AddOption(ddColor, g_colors[i]);
	YMGUI_Dropdown_SetSelectedCb(ddColor, onColor);

	//右侧下拉(与上者部分重叠区域下方无遮挡,演示浮动菜单盖过 root 内容)
	GYOBJ ddSpeed = YMGUI_Creat_Dropdown_Creat(ctx->root, 180, 40, 100, 26);
	for (int i = 0; i < 3; i++) YMGUI_Dropdown_AddOption(ddSpeed, g_speeds[i]);

	//底部下拉:下方放不下 → 菜单向上翻
	GYOBJ ddBottom = YMGUI_Creat_Dropdown_Creat(ctx->root, 40, SCR_H - 34, 120, 26);
	for (int i = 0; i < 4; i++) YMGUI_Dropdown_AddOption(ddBottom, g_colors[i]);

	g_status = YMGUI_Creat_Label_Creat(ctx->root, 0, 120, SCR_W, 16);
	YMGUI_Label_SetText(g_status, "Color = (none)");
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
	gy_log_print("demo_dropdown exit ok\n");
	return 0;
}
