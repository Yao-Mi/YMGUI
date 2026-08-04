#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_font_cjk.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 中文字体 demo。所有控件走默认 ASCII 字体,遇到 CJK 码点自动回退到 YMGUI_Font_CJK
  *	              (fallback 链),故标签/按钮直接塞 UTF-8 中文即可,无一处控件改动。
  *	              YMGUI_FONT_CJK=0 时回退链断开,中文字符不出图(留作纯 ASCII 足迹裁减)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

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
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));

	//标题:纯中文
	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 8, SCR_W, 16);
	YMGUI_Label_SetText(title, "中文字体测试");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	//中英混排:ASCII 与 CJK 同一行自动切字模
	GYOBJ mix = YMGUI_Creat_Label_Creat(ctx->root, 20, 40, 280, 16);
	YMGUI_Label_SetText(mix, "YMGUI 一二三 ABC 你好世界");
	YMGUI_Label_SetTextColor(mix, GY_ARGB(0xFF, 0xC0, 0xC0, 0xC0));

	GYOBJ line2 = YMGUI_Creat_Label_Creat(ctx->root, 20, 68, 280, 16);
	YMGUI_Label_SetText(line2, "上下左右 大小人天");
	YMGUI_Label_SetTextColor(line2, GY_ARGB(0xFF, 0x80, 0xE0, 0xB0));

	//按钮:中文标签
	GYOBJ ok = YMGUI_Creat_Button_Creat(ctx->root, 40, 120, 100, 40);
	YMGUI_Button_SetText(ok, "确定");
	GYOBJ cancel = YMGUI_Creat_Button_Creat(ctx->root, 180, 120, 100, 40);
	YMGUI_Button_SetText(cancel, "取消");

	GYOBJ hint = YMGUI_Creat_Label_Creat(ctx->root, 0, 200, SCR_W, 16);
	YMGUI_Label_SetText(hint, "default font -> CJK fallback, no widget change");
	YMGUI_Label_SetTextColor(hint, GY_ARGB(0xFF, 0x70, 0x70, 0x78));

	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
		else if (max_frames <= 0)
			frame++;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_font_cjk exit ok\n");
	return 0;
}
