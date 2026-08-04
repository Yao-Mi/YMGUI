#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Layout.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_Switch.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_layout.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 布局助手 demo:一个设置面板全靠 Stack/Align 排布,零手写坐标。
  *	              标题 Align 顶部居中;面板里若干行用竖排 Stack 堆叠(交叉轴居中);
  *	              底部一排按钮用横排 Stack。加/删控件不必重算坐标,重跑 Stack 即可。
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

	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;

	SDL_LCD_Init(&disp, 2);
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));

	//标题:先建后 Align 到顶部居中(尺寸已知,Align 只写 x/y)
	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 0, 200, 16);
	YMGUI_Label_SetText(title, "Layout Demo (Stack + Align)");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));
	YMGUI_Layout_Align(title, GY_ALIGN_TM, 8);

	//设置面板:竖排堆叠若干行(交叉轴居中),不手写任何 y
	GYOBJ panel = YMGUI_Creat_Obj_Creat(ctx->root, 40, 34, 240, 130);
	YMGUI_Obj_SetBgColor(panel, GY_ARGB(0xFF, 0x24, 0x24, 0x30));
	GYOBJ row1 = YMGUI_Creat_Label_Creat(panel, 0, 0, 180, 20);
	YMGUI_Label_SetText(row1, "Wi-Fi");
	GYOBJ row2 = YMGUI_Creat_Switch_Creat(panel, 0, 0, 56, 26);
	GYOBJ row3 = YMGUI_Creat_Label_Creat(panel, 0, 0, 180, 20);
	YMGUI_Label_SetText(row3, "Bluetooth");
	GYOBJ row4 = YMGUI_Creat_Switch_Creat(panel, 0, 0, 56, 26);
	YMGUI_Layout_Stack(panel, GY_LAYOUT_VER, 8, 10, GY_CROSS_CENTER);

	//底部一排按钮:横排堆叠(交叉轴居中),不手写任何 x
	GYOBJ bar = YMGUI_Creat_Obj_Creat(ctx->root, 40, 176, 240, 44);
	YMGUI_Obj_SetBgColor(bar, GY_ARGB(0xFF, 0x18, 0x18, 0x20));
	GYOBJ bCancel = YMGUI_Creat_Button_Creat(bar, 0, 0, 100, 30);
	YMGUI_Button_SetText(bCancel, "Cancel");
	GYOBJ bOk = YMGUI_Creat_Button_Creat(bar, 0, 0, 100, 30);
	YMGUI_Button_SetText(bOk, "OK");
	YMGUI_Layout_Stack(bar, GY_LAYOUT_HOR, 16, 12, GY_CROSS_CENTER);

	YMGUI_Inject_SetCtx(ctx);

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
	gy_log_print("demo_layout exit ok\n");
	return 0;
}
