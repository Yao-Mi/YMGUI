#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Event.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_MsgBox.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_msgbox.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-05
  *	@Description: 模态对话框 demo:三个底层按钮分别弹出不同模态(单按钮提示 / 双按钮确认)。
  *	              模态显示时全屏遮罩变暗底层并吞掉外部点击 —— 底层按钮点不动,必须点卡片按钮才关闭。
  *	              状态标签记录上次点了哪个模态按钮,直观看到"阻塞→选择→解锁"闭环。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60

static GYOBJ g_mb;      //复用同一个模态对象(反复重配置 + Show)
static GYOBJ g_status;
static int   g_bg_clicks;//底层按钮被点次数(演示模态期间点不动)

static void setStatus(const char* s)
{
	YMGUI_Label_SetText(g_status, s);
}

//模态按钮回调:记录选择
static void onConfirm(GYOBJ mb, int idx)
{
	(void)mb;
	setStatus(idx == 0 ? "You chose: OK" : "You chose: Cancel");
}
static void onNotice(GYOBJ mb, int idx)
{
	(void)mb; (void)idx;
	setStatus("Notice dismissed");
}

//底层按钮:弹单按钮提示
static void openNotice(GYOBJ btn)
{
	(void)btn;
	YMGUI_MsgBox_ClearButtons(g_mb);
	YMGUI_MsgBox_SetTitle(g_mb, "Notice");
	YMGUI_MsgBox_SetText(g_mb, "This is a blocking dialog.\nClick the button to close.");
	YMGUI_MsgBox_AddButton(g_mb, "Got it", onNotice);
	YMGUI_MsgBox_Show(g_mb);
}

//底层按钮:弹双按钮确认
static void openConfirm(GYOBJ btn)
{
	(void)btn;
	YMGUI_MsgBox_ClearButtons(g_mb);
	YMGUI_MsgBox_SetTitle(g_mb, "Confirm");
	YMGUI_MsgBox_SetText(g_mb, "Apply changes?");
	YMGUI_MsgBox_AddButton(g_mb, "OK", onConfirm);
	YMGUI_MsgBox_AddButton(g_mb, "Cancel", onConfirm);
	YMGUI_MsgBox_Show(g_mb);
}

//一个"底层普通按钮":用来演示模态期间点它无反应
static void bgPoke(GYOBJ btn)
{
	(void)btn;
	char buf[48];
	g_bg_clicks++;
	snprintf(buf, sizeof(buf), "Background poked x%d", g_bg_clicks);
	setStatus(buf);
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
	YMGUI_Label_SetText(title, "Modal dialog demo");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	GYOBJ bNotice = YMGUI_Creat_Button_Creat(ctx->root, 20, 40, 130, 34);
	YMGUI_Button_SetText(bNotice, "Show notice");
	YMGUI_Button_SetClicked(bNotice, openNotice);

	GYOBJ bConfirm = YMGUI_Creat_Button_Creat(ctx->root, 170, 40, 130, 34);
	YMGUI_Button_SetText(bConfirm, "Show confirm");
	YMGUI_Button_SetClicked(bConfirm, openConfirm);

	GYOBJ bBg = YMGUI_Creat_Button_Creat(ctx->root, 20, 90, 280, 34);
	YMGUI_Button_SetText(bBg, "Background button (locked while modal up)");
	YMGUI_Button_SetColors(bBg, GY_ARGB(0xFF, 0x50, 0x50, 0x5A), GY_ARGB(0xFF, 0x30, 0x30, 0x38));
	YMGUI_Button_SetClicked(bBg, bgPoke);

	g_status = YMGUI_Creat_Label_Creat(ctx->root, 0, 150, SCR_W, 16);
	YMGUI_Label_SetText(g_status, "Ready.");
	YMGUI_Label_SetTextColor(g_status, GY_ARGB(0xFF, 0xA0, 0xE0, 0xA0));

	//模态对话框(挂 top_layer,初始隐藏,反复复用)
	g_mb = YMGUI_Creat_MsgBox_Creat(ctx);

	YMGUI_Inject_SetCtx(ctx);

	//headless 自检:开确认框 → 点 OK → 应关闭并回状态
	if (max_frames > 0)
	{
		int fail = 0;
		openConfirm(NULL);
		if (YMGUI_MsgBox_IsShown(g_mb) != 1) { gy_log_print("selftest FAIL: modal not shown\n"); fail = 1; }
		//点第一个按钮(OK)中心
		GYOBJ card = g_mb->child_head;
		GYOBJ b0 = (card != NULL) ? card->child_head : NULL;
		if (b0 != NULL)
		{
			GYrect a0; YMGUI_Obj_GetAbsArea(b0, &a0);
			YMGUI_Event_Pointer(ctx, (GYcoord)(a0.x + a0.w / 2), (GYcoord)(a0.y + a0.h / 2), 1);
			YMGUI_Event_Pointer(ctx, (GYcoord)(a0.x + a0.w / 2), (GYcoord)(a0.y + a0.h / 2), 0);
		}
		else { gy_log_print("selftest FAIL: no modal button\n"); fail = 1; }
		if (YMGUI_MsgBox_IsShown(g_mb) != 0) { gy_log_print("selftest FAIL: modal still shown after OK\n"); fail = 1; }
		if (!fail) gy_log_print("selftest: modal open + button dismiss OK\n");
	}

	//截图模式(YMGUI_SHOT 已设):开一个双按钮模态定住,让截图截到弹窗展开态
	if (getenv("YMGUI_SHOT") != NULL)
	{
		openConfirm(NULL);
		setStatus("Modal is up - background locked");
	}

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
	gy_log_print("demo_msgbox exit ok\n");
	return 0;
}
