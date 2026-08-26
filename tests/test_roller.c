#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Roller.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_roller.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 滚轮/居中高亮列表单测:整批设行/追加/清空、SetSelected 钳制+回调、平滑缓动逼近
  *	              (Tick 逐步到位 + 到位后不再动)、非动画立即到位、拖动交互吸附到最近行+回调、
  *	              中间行高亮渲染出像素、边界钳制、析构不崩。判成败以 exit code 为准。
  ***************************************************************************************************************************/

#define SCR_W 200
#define SCR_H 180
#define BAND_H 180

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static GYpx g_fb[SCR_W * SCR_H];
static void fbFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d;
	for (GYcoord yy = 0; yy < a->h; yy++)
		for (GYcoord xx = 0; xx < a->w; xx++)
		{
			GYcoord sx = a->x + xx, sy = a->y + yy;
			if (sx >= 0 && sx < SCR_W && sy >= 0 && sy < SCR_H)
				g_fb[sy * SCR_W + sx] = b[yy * a->w + xx];
		}
}

static int g_changed_calls = 0;
static int32 g_changed_idx = -1;
static void onChanged(GYOBJ r, int32 idx) { (void)r; g_changed_calls++; g_changed_idx = idx; }

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = fbFlush; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0, 0, 0));
	YMGUI_Inject_SetCtx(ctx);

	//Roller (20,10) 160x160,行高 30 → 中间可见约 5 行
	GYOBJ roller = YMGUI_Creat_Roller_Creat(ctx->root, 20, 10, 160, 160);
	YMGUI_Roller_SetRowHeight(roller, 30);
	YMGUI_Roller_SetColors(roller, GY_ARGB(0xFF, 0x10, 0x10, 0x18),
	                        GY_ARGB(0xFF, 0x60, 0x60, 0x60), GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF), 0);
	YMGUI_Roller_SetChanged(roller, onChanged);

	//---- 整批设行 ----
	static const char* const lines[8] = {
		"line 0", "line 1", "line 2", "line 3", "line 4", "line 5", "line 6", "line 7"
	};
	YMGUI_Roller_SetLines(roller, lines, 8);
	CHECK(YMGUI_Roller_GetCount(roller) == 8, "设 8 行");
	CHECK(YMGUI_Roller_GetSelected(roller) == 0, "初始选中 0");

	//---- 追加 / 清空 ----
	CHECK(YMGUI_Roller_AddLine(roller, "line 8") == 9, "追加后 9 行");
	YMGUI_Roller_Clear(roller);
	CHECK(YMGUI_Roller_GetCount(roller) == 0, "清空后 0 行");
	YMGUI_Roller_SetLines(roller, lines, 8);

	//---- SetSelected 钳制 + 回调(变化才发)----
	g_changed_calls = 0; g_changed_idx = -1;
	YMGUI_Roller_SetSelected(roller, 3, 1);
	CHECK(YMGUI_Roller_GetSelected(roller) == 3, "选中 3");
	CHECK(g_changed_calls == 1 && g_changed_idx == 3, "SetSelected 触发回调 idx=3");
	YMGUI_Roller_SetSelected(roller, 3, 1); //同值不发
	CHECK(g_changed_calls == 1, "选同值不重复回调");
	YMGUI_Roller_SetSelected(roller, 100, 1); //越界钳到 7
	CHECK(YMGUI_Roller_GetSelected(roller) == 7, "越界钳到末行 7");
	YMGUI_Roller_SetSelected(roller, -5, 1);  //越界钳到 0
	CHECK(YMGUI_Roller_GetSelected(roller) == 0, "越界钳到首行 0");

	//---- 平滑缓动:目标 6,animate,Tick 逐帧逼近,到位后停 ----
	YMGUI_Roller_SetEaseDiv(roller, 4);
	YMGUI_Roller_SetSelected(roller, 0, 0); //先立即归 0
	YMGUI_Roller_SetSelected(roller, 6, 1); //目标 6,动画
	int moving = 0, guard = 0;
	//第一帧应还在移动(未立即到位)
	moving = YMGUI_Roller_Tick(roller);
	CHECK(moving == 1, "缓动首帧仍在移动(非瞬时到位)");
	//跑够多帧收敛
	while (YMGUI_Roller_Tick(roller) && guard < 200) guard++;
	CHECK(guard < 200, "缓动在有限帧内收敛");
	CHECK(YMGUI_Roller_Tick(roller) == 0, "到位后 Tick 返回 0(不再动)");

	//---- 非动画立即到位:一次 SetSelected(animate=0) 后 Tick 立即无移动 ----
	YMGUI_Roller_SetSelected(roller, 2, 0);
	CHECK(YMGUI_Roller_Tick(roller) == 0, "animate=0 立即到位");

	//---- 渲染:中间高亮行像素非全黑(有文字画出)----
	YMGUI_Refresh(ctx);
	int lit = 0;
	//控件中线 y≈10+80=90,采样中带
	for (int yy = 80; yy < 100; yy++)
		for (int xx = 20; xx < 180; xx++)
			if (g_fb[yy * SCR_W + xx] != g_fb[0]) { lit++; }
	CHECK(lit > 0, "中间高亮行渲染出非背景像素");

	//---- 交互态拖动吸附 ----
	YMGUI_Roller_SetInteractive(roller, 1);
	YMGUI_Roller_SetSelected(roller, 4, 0); //立即到 4,cur=4
	g_changed_calls = 0; g_changed_idx = -1;
	//在控件内按下并向上拖 ~2 行(60px),抬起吸附
	YMGUI_Inject_Pointer(100, 90, 1);   //Pressed
	YMGUI_Inject_Pointer(100, 30, 1);   //Pressing 向上 60px ≈ +2 行
	YMGUI_Inject_Pointer(100, 30, 0);   //Released → 吸附
	CHECK(YMGUI_Roller_GetSelected(roller) == 6, "上拖 2 行吸附到 6");
	CHECK(g_changed_calls >= 1 && g_changed_idx == 6, "拖动吸附触发回调 idx=6");

	//---- 交互态钳制:狂拖不越界 ----
	YMGUI_Roller_SetSelected(roller, 7, 0);
	YMGUI_Inject_Pointer(100, 90, 1);
	YMGUI_Inject_Pointer(100, -300, 1); //向上狂拖
	YMGUI_Inject_Pointer(100, -300, 0);
	CHECK(YMGUI_Roller_GetSelected(roller) == 7, "上拖越界仍钳末行 7");

	//---- 空列表不崩 ----
	YMGUI_Roller_Clear(roller);
	YMGUI_Roller_SetSelected(roller, 0, 1);
	YMGUI_Roller_Tick(roller);
	YMGUI_Refresh(ctx);
	CHECK(YMGUI_Roller_GetCount(roller) == 0, "空列表操作后仍 0 行");

	//---- 析构级联不崩 ----
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0) printf("test_roller: ALL PASS\n");
	else            printf("test_roller: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
