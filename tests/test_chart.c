#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Chart.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_chart.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-03
  *	@Description: 折线图单测:值域钳制/点数/序列上限/SetValue/SetNext 左移/GetValue,并验证折线用序列色渲染出像素
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static GYpx g_match;//要匹配统计的目标像素
static long g_match_cnt;
static void countFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d;
	long n = (long)a->w * a->h;
	for (long i = 0; i < n; i++)
		if (g_match != 0 && b[i] == g_match) g_match_cnt++;
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
	disp.flush_cb = countFlush;
	disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x00, 0x00, 0x00));

	GYOBJ ch = YMGUI_Creat_Chart_Creat(ctx->root, 20, 20, 280, 200);
	CHECK(ch != NULL, "chart created");
	YMGUI_Chart_SetRange(ch, 0, 100);
	YMGUI_Chart_SetPointCount(ch, 8);
	YMGUI_Chart_SetGrid(ch, 4, 4);

	//---- 序列与取值 ----
	GYcolor sc = GY_ARGB(0xFF, 0xF0, 0x50, 0x50);
	int32 s0 = YMGUI_Chart_AddSeries(ch, sc);
	CHECK(s0 == 0, "first series index 0");

	//SetValue + 钳制
	YMGUI_Chart_SetValue(ch, s0, 0, 50);
	CHECK(YMGUI_Chart_GetValue(ch, s0, 0) == 50, "set/get point value");
	YMGUI_Chart_SetValue(ch, s0, 1, 500);//超上限
	CHECK(YMGUI_Chart_GetValue(ch, s0, 1) == 100, "value clamped to max");
	YMGUI_Chart_SetValue(ch, s0, 2, -50);//超下限
	CHECK(YMGUI_Chart_GetValue(ch, s0, 2) == 0, "value clamped to min");

	//SetNext 左移:先全设已知,再推一个新值,首点应变原来的第2点
	for (uint16 i = 0; i < 8; i++) YMGUI_Chart_SetValue(ch, s0, i, (int32)(i * 10));
	int32 before1 = YMGUI_Chart_GetValue(ch, s0, 1);//=10
	YMGUI_Chart_SetNext(ch, s0, 99);
	CHECK(YMGUI_Chart_GetValue(ch, s0, 0) == before1, "SetNext shifts left");
	CHECK(YMGUI_Chart_GetValue(ch, s0, 7) == 99, "SetNext appends at tail");

	//值域收窄应钳制既有点
	YMGUI_Chart_SetRange(ch, 0, 50);
	CHECK(YMGUI_Chart_GetValue(ch, s0, 7) == 50, "SetRange clamps existing points");

	//序列上限
	for (int i = 1; i < GY_CHART_MAX_SERIES; i++)
		CHECK(YMGUI_Chart_AddSeries(ch, sc) == i, "series added within cap");
	CHECK(YMGUI_Chart_AddSeries(ch, sc) == -1, "series beyond cap rejected");

	//---- 渲染:折线用序列色画出像素 ----
	//恢复干净:新图,单序列,阶梯上升,应有序列色像素
	YMGUI_Free_ObjFree(ch);
	GYcolor line = GY_ARGB(0xFF, 0x30, 0xE0, 0x60);
	GYOBJ ch2 = YMGUI_Creat_Chart_Creat(ctx->root, 20, 20, 280, 200);
	YMGUI_Chart_SetRange(ch2, 0, 100);
	YMGUI_Chart_SetPointCount(ch2, 6);
	YMGUI_Chart_SetGrid(ch2, 0, 0);//关网格,避免干扰
	int32 sa = YMGUI_Chart_AddSeries(ch2, line);
	for (uint16 i = 0; i < 6; i++) YMGUI_Chart_SetValue(ch2, sa, i, (int32)(i * 20));
	g_match = GY_ColorToPx(line);
	g_match_cnt = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_match_cnt > 0, "polyline drawn in series color");

	//全平线(全 0)也应渲染出线(至少 1 像素,证明画了)
	for (uint16 i = 0; i < 6; i++) YMGUI_Chart_SetValue(ch2, sa, i, 0);
	g_match_cnt = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_match_cnt > 0, "flat polyline still draws pixels");
	g_match = 0;

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_chart: ALL PASS\n");
	else
		printf("test_chart: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
