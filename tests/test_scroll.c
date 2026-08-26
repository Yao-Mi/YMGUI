#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_scroll.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 滚动+子裁剪机制单测:父 scroll 使子 abs 偏移;ClipChildren 把超出父区的子裁掉
  ***************************************************************************************************************************/

#define SCR_W 200
#define SCR_H 200
#define BAND_H 200

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

//记录 flush 出的非零像素范围(验证裁剪:子项只在父区内出现)
static GYcoord g_minx, g_miny, g_maxx, g_maxy;
static long g_set;
static void flushCb(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d;
	for (GYcoord yy = 0; yy < a->h; yy++)
		for (GYcoord xx = 0; xx < a->w; xx++)
			if (b[yy * a->w + xx])
			{
				GYcoord sx = a->x + xx, sy = a->y + yy;
				if (sx < g_minx) g_minx = sx;
				if (sy < g_miny) g_miny = sy;
				if (sx > g_maxx) g_maxx = sx;
				if (sy > g_maxy) g_maxy = sy;
				g_set++;
			}
}

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = flushCb; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0, 0, 0));

	//视口:开 ClipChildren 的容器,位于 (50,50) 大小 80x80
	GYOBJ view = YMGUI_Creat_Obj_Creat(ctx->root, 50, 50, 80, 80);
	view->state |= GY_STATE_ClipChildren;
	view->bg_color = GY_ARGB(0xFF, 0, 0, 0);//黑=RGB565的0,使统计只计白色子项

	//子:相对父 (0,0),大小 40x300(比父高很多,超出下边界)
	GYOBJ child = YMGUI_Creat_Obj_Creat(view, 0, 0, 40, 300);
	child->bg_color = GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF);//白,便于统计

	//---- 无滚动:child abs 应为 (50,50) ----
	GYrect abs;
	YMGUI_Obj_GetAbsArea(child, &abs);
	CHECK(abs.x == 50 && abs.y == 50, "no scroll: child abs at parent origin (50,50)");
	CHECK(abs.w == 40 && abs.h == 300, "child keeps own w/h");

	//渲染,统计白色像素范围:应被裁到父区 [50,130)x[50,130)
	g_minx = 30000; g_miny = 30000; g_maxx = -1; g_maxy = -1; g_set = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_miny >= 50, "child clipped: top not above parent (>=50)");
	CHECK(g_maxy <= 129, "child clipped: bottom not below parent (<=129, was 300 tall)");
	CHECK(g_maxx <= 129, "child clipped: right within parent (child w=40 → x 50..89)");
	CHECK(g_minx == 50 && g_miny == 50, "child top-left at parent origin");

	//---- 滚动 scroll_y=100:child abs.y 应 = 50 - 100 = -50 ----
	view->scroll_y = 100;
	YMGUI_Obj_GetAbsArea(child, &abs);
	CHECK(abs.y == -50, "scroll_y=100: child abs.y shifts up to -50");
	CHECK(abs.x == 50, "scroll_y doesn't affect x");

	//滚动后重绘:白色区顶部被裁到父上沿(50),底部 = child底(-50+300=250)裁到父下沿(129)
	YMGUI_Ctx_InvalidateArea(ctx, &(GYrect){50, 50, 80, 80});
	g_minx = 30000; g_miny = 30000; g_maxx = -1; g_maxy = -1; g_set = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_miny >= 50 && g_maxy <= 129, "after scroll: still clipped to parent viewport");
	CHECK(g_set > 0, "after scroll: child still visible in viewport");

	//---- scroll_y 超过内容:child 完全滚出上方,视口内无白 ----
	view->scroll_y = 400;//child 高 300,滚 400 → 完全在上方
	YMGUI_Ctx_InvalidateArea(ctx, &(GYrect){50, 50, 80, 80});
	g_set = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_set == 0, "scroll past content: child fully scrolled out, no white in viewport");

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_scroll: ALL PASS\n");
	else
		printf("test_scroll: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
