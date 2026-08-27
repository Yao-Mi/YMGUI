#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_List.h"
#include "YMGUI_Font.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_list.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 可滚动列表 demo:20 条目,鼠标拖动上下滚动,超出视口的条目被裁剪
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

	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;

	SDL_LCD_Init(&disp, 1);
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 8, SCR_W, 16);
	YMGUI_Label_SetText(title, "Drag to scroll");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	//故意少一个默认字高:比上一版再少半个字,用于直观看列表视口裁剪是否可靠。
	GYOBJ list = YMGUI_Creat_List_Creat(ctx->root, 40, 32, 240,
		190 - YMGUI_Font_Default.cell_h);
	for (int i = 0; i < 20; i++)
	{
		char buf[24];
		snprintf(buf, sizeof(buf), "List item #%d", i);
		YMGUI_List_AddItem(list, buf, 32);
	}

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
	gy_log_print("demo_list exit ok\n");
	return 0;
}
