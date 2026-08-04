#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_Switch.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Bar.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_form.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 纯矩形表单四件套交互 demo:checkbox/switch/slider/bar。滑块拖动实时更新进度条+数值标签
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static GYOBJ s_bar = NULL;
static GYOBJ s_val_label = NULL;

//滑块拖动 → 同步进度条 + 数值标签
static void onSliderChanged(GYOBJ sld, int32 v)
{
	(void)sld;
	if (s_bar) YMGUI_Bar_SetValue(s_bar, v);
	if (s_val_label)
	{
		char buf[24];
		snprintf(buf, sizeof(buf), "value: %d", v);
		YMGUI_Label_SetText(s_val_label, buf);
	}
}

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

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 8, SCR_W, 18);
	YMGUI_Label_SetText(title, "YMGUI Form Widgets");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	//复选框
	GYOBJ cb = YMGUI_Creat_Checkbox_Creat(ctx->root, 20, 40, 160, 20);
	YMGUI_Checkbox_SetText(cb, "Enable");

	//开关
	GYOBJ sw = YMGUI_Creat_Switch_Creat(ctx->root, 20, 74, 60, 28);

	//滑块
	GYOBJ sld = YMGUI_Creat_Slider_Creat(ctx->root, 20, 120, 200, 22);
	YMGUI_Slider_SetRange(sld, 0, 100);
	YMGUI_Slider_SetValue(sld, 30);
	YMGUI_Slider_SetChanged(sld, onSliderChanged);

	//进度条(跟随滑块)
	s_bar = YMGUI_Creat_Bar_Creat(ctx->root, 20, 160, 200, 16);
	YMGUI_Bar_SetRange(s_bar, 0, 100);
	YMGUI_Bar_SetValue(s_bar, 30);

	//数值标签
	s_val_label = YMGUI_Creat_Label_Creat(ctx->root, 20, 190, 200, 18);
	YMGUI_Label_SetText(s_val_label, "value: 30");
	YMGUI_Label_SetTextColor(s_val_label, GY_ARGB(0xFF, 0xC0, 0xC0, 0xC0));

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
	gy_log_print("demo_form exit ok\n");
	return 0;
}
