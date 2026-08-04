#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Button.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_event.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 无 SDL 的交互闭环单测:注入指针→命中→事件→状态改变标脏→Refresh 只重绘脏区
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

//记录 flush 覆盖的行范围(验证只重绘脏区)
static GYcoord g_flush_ymin, g_flush_ymax;
static int     g_flush_count;
static int     g_clicked;

static void testFlushCb(GYdisp* d, const GYrect* area, const GYpx* buf)
{
	(void)d; (void)buf;
	if (area->y < g_flush_ymin) g_flush_ymin = area->y;
	if (area->y + area->h > g_flush_ymax) g_flush_ymax = area->y + area->h;
	g_flush_count++;
}

static void onClicked(GYOBJ btn) { (void)btn; g_clicked++; }

static void resetFlush(void)
{
	g_flush_ymin = 30000;
	g_flush_ymax = -30000;
	g_flush_count = 0;
}

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL;
	disp.flush_cb = testFlushCb;
	disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	GYOBJ btn = YMGUI_Creat_Button_Creat(ctx->root, 100, 90, 120, 60);//屏幕 y=90..150
	YMGUI_Button_SetClicked(btn, onClicked);
	YMGUI_Inject_SetCtx(ctx);

	//首帧:整屏脏(root+btn),Refresh 后脏清空
	YMGUI_Refresh(ctx);
	CHECK(ctx->inv_cnt == 0, "after first refresh dirty cleared");

	//---- 命中测试 ----
	CHECK(YMGUI_HitTest(ctx, 110, 100) == btn, "hit inside button → btn");
	CHECK(YMGUI_HitTest(ctx, 10, 10) == ctx->root, "hit outside button → root");

	//---- 按下:btn 进入 pressed 态,并标脏 ----
	YMGUI_Inject_Pointer(110, 100, 1);
	CHECK(btn->state & GY_STATE_Pressed, "button pressed state set");
	CHECK(ctx->pressed_obj == btn, "ctx.pressed_obj == btn");
	CHECK(ctx->inv_cnt > 0, "press invalidates (dirty)");

	//刷新:脏区应只覆盖按钮所在行段(90..150),不是整屏(0..240)
	resetFlush();
	YMGUI_Refresh(ctx);
	CHECK(g_flush_ymin >= 90 && g_flush_ymax <= 150, "refresh only button's dirty rows (90..150)");
	CHECK(g_flush_ymin < g_flush_ymax, "flush happened");

	//---- 抬起(仍在按钮上):触发 Clicked,清 pressed ----
	YMGUI_Inject_Pointer(110, 100, 0);
	CHECK(!(btn->state & GY_STATE_Pressed), "button pressed cleared on release");
	CHECK(ctx->pressed_obj == NULL, "ctx.pressed_obj cleared");
	CHECK(g_clicked == 1, "clicked callback fired once");

	//---- 抬起在按钮外不算点击 ----
	YMGUI_Inject_Pointer(110, 100, 1);//按下
	YMGUI_Inject_Pointer(10, 10, 0);  //移出后抬起
	CHECK(g_clicked == 1, "release off-button does NOT click");

	//---- 释放:级联释放不崩,引用清理 ----
	YMGUI_Free_CtxFree(ctx);

	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_event: ALL PASS\n");
	else
		printf("test_event: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
