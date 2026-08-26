#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Tabview.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_tabview.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-31
  *	@Description: 标签页单测:AddTab 返回页容器、仅当前页可见(像素采样)、tab bar 点击切页+回调、
  *	              隐藏页被 hit-test 跳过、SetActive 越界忽略、析构级联释放。
  ***************************************************************************************************************************/

#define SCR_W 200
#define SCR_H 160
#define BAND_H 160

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
static GYpx px_at(GYcoord x, GYcoord y) { return g_fb[y * SCR_W + x]; }

static int g_cb_fired;
static uint16 g_cb_active;
static void onChanged(GYOBJ tv, uint16 a) { (void)tv; g_cb_fired = 1; g_cb_active = a; }

static void click(GYCTX ctx, GYcoord x, GYcoord y)
{
	YMGUI_Event_Pointer(ctx, x, y, 1);
	YMGUI_Event_Pointer(ctx, x, y, 0);
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

	//tabview 铺满全屏,bar_h=24 → 内容区 y=24..160
	GYOBJ tv = YMGUI_Creat_Tabview_Creat(ctx->root, 0, 0, SCR_W, SCR_H);
	YMGUI_Tabview_SetChangedCb(tv, onChanged);

	//三页,各放一个铺满页的纯色子对象(便于采样内容区颜色判断哪页可见)
	GYpx RED   = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0x00, 0x00));
	GYpx GREEN = GY_ColorToPx(GY_ARGB(0xFF, 0x00, 0xFF, 0x00));
	GYpx BLUE  = GY_ColorToPx(GY_ARGB(0xFF, 0x00, 0x00, 0xFF));

	GYOBJ p0 = YMGUI_Tabview_AddTab(tv, "One");
	GYOBJ p1 = YMGUI_Tabview_AddTab(tv, "Two");
	GYOBJ p2 = YMGUI_Tabview_AddTab(tv, "Three");
	CHECK(p0 != NULL && p1 != NULL && p2 != NULL, "AddTab returns page containers");
	CHECK(YMGUI_Tabview_GetTabCount(tv) == 3, "tab count 3");
	CHECK(YMGUI_Tabview_GetPage(tv, 1) == p1, "GetPage returns same page");
	CHECK(YMGUI_Tabview_GetPage(tv, 9) == NULL, "GetPage out-of-range NULL");

	//每页一个铺满的纯色子
	GYOBJ c0 = YMGUI_Creat_Obj_Creat(p0, 0, 0, SCR_W, SCR_H - 24);
	c0->bg_color = GY_ARGB(0xFF, 0xFF, 0x00, 0x00);
	GYOBJ c1 = YMGUI_Creat_Obj_Creat(p1, 0, 0, SCR_W, SCR_H - 24);
	c1->bg_color = GY_ARGB(0xFF, 0x00, 0xFF, 0x00);
	GYOBJ c2 = YMGUI_Creat_Obj_Creat(p2, 0, 0, SCR_W, SCR_H - 24);
	c2->bg_color = GY_ARGB(0xFF, 0x00, 0x00, 0xFF);

	//---- 初始:第 0 页可见 ----
	CHECK(YMGUI_Tabview_GetActive(tv) == 0, "initial active 0");
	YMGUI_Refresh(ctx);
	//内容区中心 (100, 90) 应为红(第 0 页)
	CHECK(px_at(100, 90) == RED, "page0 (red) visible initially");

	//隐藏页被 hit-test 跳过:内容区点击应命中第 0 页的子,不是别页
	GYOBJ hit = YMGUI_HitTest(ctx, 100, 90);
	CHECK(hit == c0, "content-region hit lands on active page0 child");

	//---- 点第 1 个 tab(段宽=200/3≈66;第 1 段中心 x≈100,y 在 bar 内 y=12)----
	g_cb_fired = 0; g_cb_active = 0xFFFF;
	click(ctx, 100, 12);
	CHECK(YMGUI_Tabview_GetActive(tv) == 1, "click tab1 → active 1");
	CHECK(g_cb_fired == 1 && g_cb_active == 1, "changed_cb fired with idx 1");
	YMGUI_Refresh(ctx);
	CHECK(px_at(100, 90) == GREEN, "page1 (green) now visible");
	CHECK(YMGUI_HitTest(ctx, 100, 90) == c1, "hit now lands on page1 child (page0 hidden, skipped)");

	//---- 点第 2 个 tab(第 2 段中心 x≈166)----
	g_cb_fired = 0;
	click(ctx, 166, 12);
	CHECK(YMGUI_Tabview_GetActive(tv) == 2, "click tab2 → active 2");
	YMGUI_Refresh(ctx);
	CHECK(px_at(100, 90) == BLUE, "page2 (blue) visible");

	//---- 点当前页 tab:无切换、不触发回调 ----
	g_cb_fired = 0;
	click(ctx, 166, 12);
	CHECK(g_cb_fired == 0, "clicking active tab does not fire changed_cb");
	CHECK(YMGUI_Tabview_GetActive(tv) == 2, "active unchanged");

	//---- 内容区点击不切页(留给页内控件)----
	g_cb_fired = 0;
	click(ctx, 100, 90);
	CHECK(g_cb_fired == 0, "click in content area does not switch tab");

	//---- SetActive 程序切页 + 越界忽略 ----
	YMGUI_Tabview_SetActive(tv, 0);
	CHECK(YMGUI_Tabview_GetActive(tv) == 0, "SetActive 0");
	YMGUI_Refresh(ctx);
	CHECK(px_at(100, 90) == RED, "SetActive back to page0 (red)");
	YMGUI_Tabview_SetActive(tv, 99);
	CHECK(YMGUI_Tabview_GetActive(tv) == 0, "SetActive out-of-range ignored");

	//---- 析构:页与其子随对象树级联释放,不崩 ----
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_tabview: ALL PASS\n");
	else
		printf("test_tabview: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
