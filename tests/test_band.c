#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_band.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 分块刷新回归:draw buffer 远小于屏宽/脏区宽时,横向+纵向切块,不越界且覆盖完整
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

//flush 校验:每块必须落在屏幕内,且 w*h 不超过 buf 容量(否则说明越界写过)
static uint32 g_cap;      //buf_px_cnt
static long   g_total_px; //累计 flush 面积
static int    g_bad_block;//有块超容量或越界
static GYcoord g_minx, g_miny, g_maxx, g_maxy;

static void checkFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d; (void)b;
	if ((uint32)(a->w * a->h) > g_cap) g_bad_block = 1;//块超 buf 容量 = 曾越界
	if (a->x < 0 || a->y < 0 || a->x + a->w > SCR_W || a->y + a->h > SCR_H) g_bad_block = 1;
	g_total_px += (long)a->w * a->h;
	if (a->x < g_minx) g_minx = a->x;
	if (a->y < g_miny) g_miny = a->y;
	if (a->x + a->w > g_maxx) g_maxx = a->x + a->w;
	if (a->y + a->h > g_maxy) g_maxy = a->y + a->h;
}

int main(void)
{
	//关键:buf 只有 100 像素 —— 远小于屏宽 320,旧代码在此越界
	uint32 buf_px = 100;
	GYdisp disp;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = checkFlush; disp.user_data = NULL;
	g_cap = buf_px;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x40, 0x40, 0x40));
	//先刷掉创建/设色产生的初始全屏脏区,清空脏列表,再做定向测试
	YMGUI_Refresh(ctx);

	//标脏一块 200x80 的区域(宽 200 > buf 容量 100 → 必须横向切块)
	GYrect dirty = {20, 30, 200, 80};
	g_total_px = 0; g_bad_block = 0;
	g_minx = 30000; g_miny = 30000; g_maxx = -30000; g_maxy = -30000;
	YMGUI_Ctx_InvalidateArea(ctx, &dirty);
	YMGUI_Refresh(ctx);

	CHECK(!g_bad_block, "no flush block exceeds buffer capacity or screen bounds");
	//覆盖范围应正好等于脏区(切块拼回完整)
	CHECK(g_minx == 20 && g_miny == 30, "coverage starts at dirty top-left");
	CHECK(g_maxx == 220 && g_maxy == 110, "coverage ends at dirty bottom-right");
	//总面积 == 脏区面积(200*80),无重叠无遗漏
	CHECK(g_total_px == 200L * 80, "total flushed area == dirty area (200*80)");

	//全屏脏(320x240)也不越界
	g_total_px = 0; g_bad_block = 0;
	GYrect full = {0, 0, SCR_W, SCR_H};
	YMGUI_Ctx_InvalidateArea(ctx, &full);
	YMGUI_Refresh(ctx);
	CHECK(!g_bad_block, "full-screen dirty with tiny buffer: no overflow");
	CHECK(g_total_px == (long)SCR_W * SCR_H, "full-screen coverage complete");

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_band: ALL PASS\n");
	else
		printf("test_band: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
