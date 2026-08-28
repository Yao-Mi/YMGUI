#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_BarChart.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_barchart.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 通用柱状图单测:柱数钳制、SetValue/SetValues 钳制、顶标保持(顶≥值)、Tick 衰减
  *	              回落(柱身/顶标各自回落且钳 0)、越界读写、柱身渲染出像素(高柱像素多于矮柱)、
  *	              TopMode NONE 不画顶标 / BAR 画顶标、配色模式 BY_HEIGHT/PER_BAR 切换重绘不崩、
  *	              PER_BAR 各柱独立设色、析构不崩。判成败以 exit code 为准。
  ***************************************************************************************************************************/

#define SCR_W 200
#define SCR_H 120
#define BAND_H 120

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

//统计矩形内非背景像素数
static int litIn(GYpx bgpx0, int x0, int x1, int y0, int y1)
{
	int n = 0;
	for (int yy = y0; yy < y1; yy++)
		for (int xx = x0; xx < x1; xx++)
			if (!GY_PxEqual(g_fb[yy * SCR_W + xx], bgpx0)) n++;
	return n;
}

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

	GYOBJ bc = YMGUI_Creat_BarChart_Creat(ctx->root, 0, 0, 200, 120);
	YMGUI_BarChart_SetRange(bc, 100);
	YMGUI_BarChart_SetBarCount(bc, 8);
	YMGUI_BarChart_SetGap(bc, 2);
	YMGUI_BarChart_SetGradient(bc, GY_ARGB(0xFF, 0x20, 0x60, 0xE0), GY_ARGB(0xFF, 0xE0, 0x40, 0x40));
	YMGUI_BarChart_SetTopColor(bc, GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	YMGUI_BarChart_SetBgColor(bc, GY_ARGB(0xFF, 0, 0, 0));

	CHECK(YMGUI_BarChart_GetBarCount(bc) == 8, "柱数 8");
	CHECK(YMGUI_BarChart_GetColorMode(bc) == GY_BARCHART_COLOR_BY_HEIGHT, "默认配色 BY_HEIGHT");

	//---- 柱数钳制 ----
	YMGUI_BarChart_SetBarCount(bc, 999);
	CHECK(YMGUI_BarChart_GetBarCount(bc) == GY_BARCHART_MAX_BARS, "柱数上限钳制");
	YMGUI_BarChart_SetBarCount(bc, 0);
	CHECK(YMGUI_BarChart_GetBarCount(bc) == 1, "柱数下限钳到 1");
	YMGUI_BarChart_SetBarCount(bc, 8);

	//---- SetValue 钳制 + 顶标保持 ----
	YMGUI_BarChart_SetValue(bc, 0, 80);
	CHECK(YMGUI_BarChart_GetValue(bc, 0) == 80, "柱0 值 80");
	YMGUI_BarChart_SetValue(bc, 1, 200); //越域钳到 100
	CHECK(YMGUI_BarChart_GetValue(bc, 1) == 100, "越域钳到 max=100");
	YMGUI_BarChart_SetValue(bc, -1, 50); //越界 index 无副作用
	YMGUI_BarChart_SetValue(bc, 99, 50);
	CHECK(YMGUI_BarChart_GetValue(bc, 99) == 0, "越界读返回 0");

	//---- SetValues 整批 ----
	int32 arr[8] = {10, 20, 30, 40, 50, 60, 70, 90};
	YMGUI_BarChart_SetValues(bc, arr, 8);
	CHECK(YMGUI_BarChart_GetValue(bc, 7) == 90, "整批设值柱7=90");

	//---- 渲染:高柱像素多于矮柱 ----
	YMGUI_Refresh(ctx);
	GYpx bg0 = g_fb[0]; //左上角=背景
	//柱0(x≈0..23,值10)对比柱7(x≈176..199,值90)
	int lit_low  = litIn(bg0, 2, 22, 0, 120);
	int lit_high = litIn(bg0, 177, 197, 0, 120);
	CHECK(lit_high > lit_low, "高柱(90)渲染像素多于矮柱(10)");
	CHECK(lit_high > 0, "高柱渲染出非背景像素");

	//---- BY_HEIGHT 逐行竖直渐变:同一根高柱,顶部行与底部行颜色应不同 ----
	//柱7 值 90(h=120 → 柱高≈108,柱顶 y≈12,柱底 y≈119),取柱内一列(x≈186)
	{
		int col = 186;
		GYpx px_top = g_fb[16 * SCR_W + col];  //近柱顶
		GYpx px_bot = g_fb[115 * SCR_W + col]; //近柱底
		CHECK(!GY_PxEqual(px_top, bg0) && !GY_PxEqual(px_bot, bg0), "柱7 顶/底都落在柱身内(非背景)");
		CHECK(!GY_PxEqual(px_top, px_bot), "BY_HEIGHT:同柱顶部与底部颜色不同(竖直渐变)");
	}

	//---- TopMode:NONE 顶标不画,BAR 顶标画 ----
	//先把所有柱压到 0、顶标也随之落到 0,腾出上半区只留顶标可见性对比
	YMGUI_BarChart_SetBarCount(bc, 4); //清值/顶标
	YMGUI_BarChart_SetTopMode(bc, GY_BARCHART_TOP_BAR);
	YMGUI_BarChart_SetValue(bc, 0, 90); //柱身升到 90,顶标顶到 90
	YMGUI_BarChart_SetValue(bc, 0, 10); //柱身瞬时压到 10,顶标仍在 90(保持)
	YMGUI_Refresh(ctx);
	bg0 = g_fb[0];
	//柱0 顶标应在 y≈12 处(h=120,top=90 → y=120-108=12),柱身只到 y≈108
	//在 y=[6,20) 的柱0 列区应有顶标像素
	int top_lit_bar = litIn(bg0, 2, 40, 6, 20);
	CHECK(top_lit_bar > 0, "TopMode=BAR 顶标在高处画出像素");
	//切 NONE,同状态重绘,顶标不该再出现在高处(柱身仍在低处)
	YMGUI_BarChart_SetTopMode(bc, GY_BARCHART_TOP_NONE);
	YMGUI_Refresh(ctx);
	bg0 = g_fb[0];
	int top_lit_none = litIn(bg0, 2, 40, 6, 20);
	CHECK(top_lit_none == 0, "TopMode=NONE 高处无顶标像素");

	//---- 配色模式 PER_BAR:各柱独立设色,切换重绘不崩 ----
	YMGUI_BarChart_SetColorMode(bc, GY_BARCHART_COLOR_PER_BAR);
	CHECK(YMGUI_BarChart_GetColorMode(bc) == GY_BARCHART_COLOR_PER_BAR, "配色切到 PER_BAR");
	GYcolor pal[4] = { GY_ARGB(0xFF,0xF0,0x20,0x20), GY_ARGB(0xFF,0x20,0xF0,0x20),
	                   GY_ARGB(0xFF,0x20,0x20,0xF0), GY_ARGB(0xFF,0xF0,0xF0,0x20) };
	YMGUI_BarChart_SetBarColors(bc, pal, 4);
	YMGUI_BarChart_SetBarColor(bc, 1, GY_ARGB(0xFF, 0x00, 0xC0, 0xC0)); //单柱覆盖
	int32 all[4] = {80, 80, 80, 80};
	YMGUI_BarChart_SetValues(bc, all, 4);
	YMGUI_Refresh(ctx); //PER_BAR 同高不同色渲染不崩

	//---- Tick 衰减:柱身回落,顶标独立回落且顶≥值 ----
	YMGUI_BarChart_SetColorMode(bc, GY_BARCHART_COLOR_BY_HEIGHT);
	YMGUI_BarChart_SetTopMode(bc, GY_BARCHART_TOP_BAR);
	YMGUI_BarChart_SetBarCount(bc, 4);
	YMGUI_BarChart_SetValue(bc, 0, 100); //顶标=100 值=100
	YMGUI_BarChart_SetDecay(bc, 10, 2);
	int v_before = YMGUI_BarChart_GetValue(bc, 0);
	YMGUI_BarChart_Tick(bc);
	int v_after = YMGUI_BarChart_GetValue(bc, 0);
	CHECK(v_after == v_before - 10, "Tick 后柱身回落 10");
	CHECK(v_after >= 0, "柱身不为负");
	//再喂一个更低值,顶标应仍高于当前值(保持)
	YMGUI_BarChart_SetValue(bc, 0, 20);
	//多帧 Tick,值降到 0,顶标随后跟降
	for (int k = 0; k < 50; k++) YMGUI_BarChart_Tick(bc);
	CHECK(YMGUI_BarChart_GetValue(bc, 0) == 0, "长期 Tick 柱身回落到 0");

	//---- 关顶标(NONE)后重绘不崩 ----
	YMGUI_BarChart_SetTopMode(bc, GY_BARCHART_TOP_NONE);
	YMGUI_BarChart_SetValue(bc, 0, 50);
	YMGUI_Refresh(ctx);

	//---- 析构不崩 ----
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0) printf("test_barchart: ALL PASS\n");
	else            printf("test_barchart: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
