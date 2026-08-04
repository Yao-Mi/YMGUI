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
#include "YMGUI_ArcWidget.h"
#include "YMGUI_Meter.h"
#include "YMGUI_State.h"
#include "YMGUI_Bind.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_bind.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 数据驱动 demo:UI = f(state)。对照 demo_form(用回调手动 poke 各控件),这里全靠绑定,
  *	              无一行胶水回调。三个 subject:
  *	                control(int) —— 拖滑块写它,进度条+数值标签自动跟(UI→state→UI 扇出)
  *	                sensor(int)  —— 后端每帧扫它(三角波),进度条+标签自动跟(state→UI,无控件 poke)
  *	                power(bool)  —— 开关与复选框绑同一个,点任一另一个自动同步(UI↔UI)
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

//三个状态就是全部真相;控件只是它们的外显
static GY_SUBJECT_INT(sv_control, 30);
static GY_SUBJECT_INT(sv_sensor, 0);
static GY_SUBJECT_BOOL(sv_power, 1);

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;
	int sweep = 0, sweep_dir = 2;//后端传感器三角波

	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL;
	disp.user_data = NULL;

	SDL_LCD_Init(&disp, 2);

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 6, SCR_W, 16);
	YMGUI_Label_SetText(title, "YMGUI Data Binding (UI = f(state))");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	//---- control:用户拖滑块 → 写 sv_control → 进度条+标签自动跟 ----
	GYOBJ sld = YMGUI_Creat_Slider_Creat(ctx->root, 20, 34, 200, 20);
	YMGUI_Slider_SetRange(sld, 0, 100);
	GYOBJ ctl_bar = YMGUI_Creat_Bar_Creat(ctx->root, 20, 60, 200, 12);
	YMGUI_Bar_SetRange(ctl_bar, 0, 100);
	GYOBJ ctl_lb = YMGUI_Creat_Label_Creat(ctx->root, 228, 40, 84, 18);
	YMGUI_Label_SetTextColor(ctl_lb, GY_ARGB(0xFF, 0xC0, 0xC0, 0xC0));
	//三个控件绑同一 subject:一处写,处处跟(拖滑块 → bar + label)
	YMGUI_Slider_Bind(sld, &sv_control);
	YMGUI_Bar_Bind(ctl_bar, &sv_control);
	YMGUI_Label_Bind(ctl_lb, &sv_control);

	//---- sensor:后端每帧写 sv_sensor → 进度条+标签自动跟(无控件 poke) ----
	GYOBJ sen_bar = YMGUI_Creat_Bar_Creat(ctx->root, 20, 96, 200, 12);
	YMGUI_Bar_SetRange(sen_bar, 0, 100);
	YMGUI_Bar_SetColors(sen_bar, GY_ARGB(0xFF, 0x30, 0x30, 0x38), GY_ARGB(0xFF, 0x40, 0xC0, 0x80));
	GYOBJ sen_lb = YMGUI_Creat_Label_Creat(ctx->root, 228, 92, 84, 18);
	YMGUI_Label_SetTextColor(sen_lb, GY_ARGB(0xFF, 0x80, 0xE0, 0xB0));
	YMGUI_Bar_Bind(sen_bar, &sv_sensor);
	YMGUI_Label_Bind(sen_lb, &sv_sensor);

	//---- 仪表类:环形进度绑 control(交互驱动)、仪表盘绑 sensor(后端驱动),单向显示 ----
	GYOBJ ctl_arc = YMGUI_Creat_Arc_Creat(ctx->root, 232, 116, 40, 40);
	YMGUI_Arc_SetRange(ctl_arc, 0, 100);
	YMGUI_Arc_Bind(ctl_arc, &sv_control);   //拖滑块 → 环形进度也跟
	GYOBJ sen_meter = YMGUI_Creat_Meter_Creat(ctx->root, 276, 112, 44, 44);
	YMGUI_Meter_SetRange(sen_meter, 0, 100);
	YMGUI_Meter_Bind(sen_meter, &sv_sensor);//后端三角波 → 仪表盘自动摆

	//---- power:开关与复选框绑同一 bool,点任一另一个同步 ----
	GYOBJ sw = YMGUI_Creat_Switch_Creat(ctx->root, 20, 130, 56, 26);
	GYOBJ ckb = YMGUI_Creat_Checkbox_Creat(ctx->root, 100, 132, 140, 20);
	YMGUI_Checkbox_SetText(ckb, "Power");
	YMGUI_Switch_Bind(sw, &sv_power);
	YMGUI_Checkbox_Bind(ckb, &sv_power);

	GYOBJ hint = YMGUI_Creat_Label_Creat(ctx->root, 0, 200, SCR_W, 16);
	YMGUI_Label_SetText(hint, "drag slider / toggle either power - all bound widgets follow");
	YMGUI_Label_SetTextColor(hint, GY_ARGB(0xFF, 0x70, 0x70, 0x78));

	YMGUI_Inject_SetCtx(ctx);

	while (SDL_LCD_PumpEvents())
	{
		//后端逻辑:只改状态,从不碰控件。每 6 帧推进一次传感器三角波。
		if ((frame % 6) == 0)
		{
			sweep += sweep_dir;
			if (sweep >= 100) { sweep = 100; sweep_dir = -2; }
			else if (sweep <= 0) { sweep = 0; sweep_dir = 2; }
			YMGUI_State_SetInt(&sv_sensor, sweep);//界面自动跟
		}

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
	gy_log_print("demo_bind exit ok\n");
	return 0;
}
