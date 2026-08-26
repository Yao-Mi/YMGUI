#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_invalidate.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 多矩形脏区列表单测:远块保持分离(不塌成大包围盒)、接触块合并、列表满退化、刷新面积统计
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

//累计 flush 的像素面积(验证多矩形比单包围盒省)
static long g_flush_px;
static void areaFlushCb(GYdisp* d, const GYrect* area, const GYpx* buf)
{
	(void)d; (void)buf;
	g_flush_px += (long)area->w * area->h;
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
	disp.flush_cb = areaFlushCb;
	disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);

	//---- 两块相距很远 → 应保持 2 块独立,不合并 ----
	ctx->inv_cnt = 0;
	GYrect topleft  = {0, 0, 20, 20};
	GYrect botright = {300, 220, 20, 20};
	YMGUI_Ctx_InvalidateArea(ctx, &topleft);
	YMGUI_Ctx_InvalidateArea(ctx, &botright);
	CHECK(ctx->inv_cnt == 2, "two far rects stay separate (not merged)");

	//关键:多矩形刷新面积 = 2*400 = 800;若塌成单包围盒则是 320*240=76800
	g_flush_px = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_flush_px < 2000, "multi-rect flush area small (~800, not full-screen 76800)");
	CHECK(ctx->inv_cnt == 0, "refresh clears dirty list");

	//---- 两块接触 → 应合并成 1 块 ----
	ctx->inv_cnt = 0;
	GYrect a = {50, 50, 40, 40};   //x:50..90
	GYrect b = {90, 50, 40, 40};   //x:90..130,与 a 边相邻
	YMGUI_Ctx_InvalidateArea(ctx, &a);
	YMGUI_Ctx_InvalidateArea(ctx, &b);
	CHECK(ctx->inv_cnt == 1, "touching rects merge into one");
	CHECK(ctx->inv_areas[0].w == 80, "merged rect spans both (w=80)");

	//---- 传递合并:c 桥接两个原本分离的块 → 最终 1 块 ----
	ctx->inv_cnt = 0;
	GYrect r1 = {0, 0, 30, 30};
	GYrect r2 = {100, 0, 30, 30};
	YMGUI_Ctx_InvalidateArea(ctx, &r1);
	YMGUI_Ctx_InvalidateArea(ctx, &r2);
	CHECK(ctx->inv_cnt == 2, "r1,r2 separate first");
	GYrect bridge = {25, 0, 80, 30};//跨接 r1(..30) 和 r2(100..)
	YMGUI_Ctx_InvalidateArea(ctx, &bridge);
	CHECK(ctx->inv_cnt == 1, "bridge merges both into one (connected component)");

	//---- 列表满 → 退化为单包围盒,仍只 1 块 ----
	ctx->inv_cnt = 0;
	for (int i = 0; i < GY_INV_MAX + 5; i++)
	{
		GYrect small = {(GYcoord)(i * 15 % 300), (GYcoord)(i * 7 % 200), 5, 5};
		YMGUI_Ctx_InvalidateArea(ctx, &small);
	}
	CHECK(ctx->inv_cnt >= 1 && ctx->inv_cnt <= GY_INV_MAX, "list never exceeds cap");

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_invalidate: ALL PASS\n");
	else
		printf("test_invalidate: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
