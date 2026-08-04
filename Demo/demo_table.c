#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Table.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_table.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-31
  *	@Description: 表格 demo:3 列 x 30 行,拖动滚动(sticky 表头固定),单击某行选中并把行号同步到状态标签
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60

static GYOBJ g_status;

static void onRow(GYOBJ t, int32 row)
{
	(void)t;
	char buf[48];
	snprintf(buf, sizeof(buf), "Selected row %d", (int)row);
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
	YMGUI_Label_SetText(title, "Drag to scroll, click a row");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	GYOBJ tbl = YMGUI_Creat_Table_Creat(ctx->root, 20, 28, 280, 170);
	YMGUI_Table_AddColumn(tbl, "ID",    50);
	YMGUI_Table_AddColumn(tbl, "Name",  140);
	YMGUI_Table_AddColumn(tbl, "Score", 88);
	for (int i = 0; i < 30; i++)
	{
		int r = YMGUI_Table_AddRow(tbl);
		char a[24], b[24], c[24];
		snprintf(a, sizeof(a), "%d", i);
		snprintf(b, sizeof(b), "item-%d", i);
		snprintf(c, sizeof(c), "%d", (i * 37) % 100);
		YMGUI_Table_SetCell(tbl, r, 0, a);
		YMGUI_Table_SetCell(tbl, r, 1, b);
		YMGUI_Table_SetCell(tbl, r, 2, c);
	}
	YMGUI_Table_SetRowCb(tbl, onRow);

	g_status = YMGUI_Creat_Label_Creat(ctx->root, 0, 210, SCR_W, 16);
	YMGUI_Label_SetText(g_status, "Selected row -1");
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
	gy_log_print("demo_table exit ok\n");
	return 0;
}
