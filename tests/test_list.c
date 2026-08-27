#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_List.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_list.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 可滚动列表单测:加条目累积内容高、拖动改scroll并钳制、条目abs随scroll偏移
  ***************************************************************************************************************************/

#define SCR_W 200
#define SCR_H 200
#define BAND_H 200

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static GYpx g_fb[SCR_W * SCR_H];
static void dummyFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d;
	for (GYcoord y = 0; y < a->h; y++)
		for (GYcoord x = 0; x < a->w; x++)
			g_fb[(a->y + y) * SCR_W + a->x + x] = b[y * a->w + x];
}

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = dummyFlush; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Inject_SetCtx(ctx);

	//列表视口 100 高,加 10 条 * 30 高 = 300 内容高(远超视口 → 可滚)
	GYOBJ list = YMGUI_Creat_List_Creat(ctx->root, 20, 20, 160, 100);
	GYOBJ first = NULL;
	for (int i = 0; i < 10; i++)
	{
		char buf[16];
		snprintf(buf, sizeof(buf), "Item %d", i);
		GYOBJ it = YMGUI_List_AddItem(list, buf, 30);
		if (i == 0) first = it;
	}
	CHECK(first != NULL, "items created");

	//首帧是整屏脏区:第 4 项文字跨过列表底边也必须被列表视口裁掉。
	YMGUI_Refresh(ctx);
	int leaked = 0;
	for (int y = 120; y < 136; y++)
		for (int x = 20; x < 180; x++)
			if (g_fb[y * SCR_W + x] != 0) leaked = 1;
	CHECK(!leaked, "first full-frame draw clips partial bottom row to list viewport");

	//初始 scroll=0,第一条 abs.y == 视口 y(20)
	GYrect abs;
	YMGUI_Obj_GetAbsArea(first, &abs);
	CHECK(abs.y == 20, "scroll=0: first item at viewport top (y=20)");

	//SetScroll 60 → 第一条上移 60 → abs.y = 20-60 = -40
	YMGUI_List_SetScroll(list, 60);
	YMGUI_Obj_GetAbsArea(first, &abs);
	CHECK(abs.y == -40, "scroll=60: first item shifted up to -40");
	CHECK(YMGUI_List_GetScroll(list) == 60, "scroll value stored");

	//SetScroll 超过内容(999)→ 钳到 content_h(300) - viewport(100) = 200
	YMGUI_List_SetScroll(list, 999);
	CHECK(YMGUI_List_GetScroll(list) == 200, "scroll clamped to content_h - viewport (200)");

	//SetScroll 负 → 钳到 0
	YMGUI_List_SetScroll(list, -50);
	CHECK(YMGUI_List_GetScroll(list) == 0, "scroll clamped to 0 (no negative)");

	//---- 拖动滚动:在列表内按下(y=100)向上拖到 y=40(上移 60)→ scroll 增 60 ----
	YMGUI_List_SetScroll(list, 0);
	YMGUI_Inject_Pointer(100, 100, 1);//按下(命中某条目,转发给list)
	YMGUI_Inject_Pointer(100, 40, 1); //拖到 y=40(手指上移60 → 内容上滚60)
	CHECK(YMGUI_List_GetScroll(list) == 60, "drag up 60px → scroll 60");
	YMGUI_Inject_Pointer(100, 40, 0); //抬起

	//继续拖:再从 y=40 拖到 y=140(下移100)→ scroll 减到 max(0, 60-100)=0
	YMGUI_Inject_Pointer(100, 40, 1);
	YMGUI_Inject_Pointer(100, 140, 1);
	CHECK(YMGUI_List_GetScroll(list) == 0, "drag down past top → scroll clamps to 0");
	YMGUI_Inject_Pointer(100, 140, 0);

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_list: ALL PASS\n");
	else
		printf("test_list: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
