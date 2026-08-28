#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Table.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_table.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-31
  *	@Description: 表格单测:列/行 API、单元格读写、滚动钳制、sticky 表头(滚动后仍在顶)、
  *	              行体裁剪(滚出的行不盖表头)、行点击选中+回调、拖动阈值(滚动不算点击)。
  ***************************************************************************************************************************/

#define SCR_W 240
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
static int32 g_cb_row;
static void onRow(GYOBJ t, int32 row) { (void)t; g_cb_fired = 1; g_cb_row = row; }

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = fbFlush; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0, 0, 0));

	//表格 (10,10) 200x110;head_h=22,row_h=22 → 表体 88px = 4 行可见
	GYOBJ tbl = YMGUI_Creat_Table_Creat(ctx->root, 10, 10, 200, 110);
	YMGUI_Table_SetRowCb(tbl, onRow);

	//---- 列 API ----
	CHECK(YMGUI_Table_AddColumn(tbl, "Name", 100) == 0, "AddColumn 0");
	CHECK(YMGUI_Table_AddColumn(tbl, "Val", 100) == 1, "AddColumn 1");
	CHECK(YMGUI_Table_GetColCount(tbl) == 2, "col count 2");

	//---- 行 API + 单元格读写 ----
	for (int i = 0; i < 10; i++)
	{
		int r = YMGUI_Table_AddRow(tbl);
		CHECK(r == i, "AddRow returns sequential index");
		char a[24], b[24];
		snprintf(a, sizeof(a), "row%d", i);
		snprintf(b, sizeof(b), "%d", i * 10);
		YMGUI_Table_SetCell(tbl, i, 0, a);
		YMGUI_Table_SetCell(tbl, i, 1, b);
	}
	CHECK(YMGUI_Table_GetRowCount(tbl) == 10, "row count 10");
	CHECK(strcmp(YMGUI_Table_GetCell(tbl, 3, 0), "row3") == 0, "GetCell 3,0");
	CHECK(strcmp(YMGUI_Table_GetCell(tbl, 3, 1), "30") == 0, "GetCell 3,1");
	//越界读写安全
	YMGUI_Table_SetCell(tbl, 99, 0, "x");//忽略
	CHECK(strcmp(YMGUI_Table_GetCell(tbl, 99, 0), "") == 0, "GetCell out-of-range → empty");
	CHECK(strcmp(YMGUI_Table_GetCell(tbl, 0, 5), "") == 0, "GetCell col out-of-range → empty");

	//---- 滚动钳制 ----
	//内容高 = 10*22 = 220;表体 = 110-22 = 88;maxs = 132
	YMGUI_Table_SetScroll(tbl, -50);
	CHECK(YMGUI_Table_GetScroll(tbl) == 0, "scroll clamps at 0");
	YMGUI_Table_SetScroll(tbl, 9999);
	CHECK(YMGUI_Table_GetScroll(tbl) == 132, "scroll clamps at content-viewport (132)");
	YMGUI_Table_SetScroll(tbl, 0);

	//---- sticky 表头:滚动后表头仍占据顶部 ----
	GYpx HEAD = GY_ColorToPx(GY_ARGB(0xFF, 0x2A, 0x2A, 0x34));
	YMGUI_Table_SetScroll(tbl, 0);
	YMGUI_Refresh(ctx);
	//表头区中心(避开文字/列线):x=50, y=10+11=21
	CHECK(GY_PxEqual(px_at(50, 21), HEAD), "header bg present at top (no scroll)");
	//滚到底再看表头:仍应是表头色(sticky)
	YMGUI_Table_SetScroll(tbl, 132);
	YMGUI_Refresh(ctx);
	CHECK(GY_PxEqual(px_at(50, 21), HEAD), "sticky header still at top after scroll to bottom");

	//---- 行体裁剪 + 选中高亮不越界到表头 ----
	GYpx SEL = GY_ColorToPx(GY_ARGB(0xFF, 0x35, 0x5A, 0x8A));
	YMGUI_Table_SetScroll(tbl, 0);
	YMGUI_Table_SetSelectedRow(tbl, 0);//选第 0 行(在表体顶部)
	YMGUI_Refresh(ctx);
	CHECK(GY_PxEqual(px_at(50, 21), HEAD), "selected row0 highlight does NOT bleed into header");
	//第 0 行在表体内 (y≈32..54),中心 y≈43 应为选中色
	CHECK(GY_PxEqual(px_at(50, 43), SEL), "row0 highlight visible in body");
	//把选中行滚出视口上方:表头区不应出现选中色
	YMGUI_Table_SetScroll(tbl, 100);//row0 顶到 body_top-100,远在上方
	YMGUI_Refresh(ctx);
	CHECK(GY_PxEqual(px_at(50, 21), HEAD), "scrolled-out selected row clipped (header intact)");

	//---- 行点击选中 + 回调 ----
	YMGUI_Table_SetScroll(tbl, 0);
	YMGUI_Table_SetSelectedRow(tbl, -1);
	g_cb_fired = 0; g_cb_row = -99;
	//点击表体第 0 行:body_top=32,点 y=40 → row 0
	YMGUI_Event_Pointer(ctx, 60, 40, 1);
	YMGUI_Event_Pointer(ctx, 60, 40, 0);
	CHECK(g_cb_fired == 1, "row click fired row_cb");
	CHECK(g_cb_row == 0, "row_cb reports row 0");
	CHECK(YMGUI_Table_GetSelectedRow(tbl) == 0, "selected row updated to 0");

	//点击第 2 行:y = 32 + 2*22 + 10 = 86 → row 2
	g_cb_fired = 0;
	YMGUI_Event_Pointer(ctx, 60, 86, 1);
	YMGUI_Event_Pointer(ctx, 60, 86, 0);
	CHECK(g_cb_row == 2, "click lands on row 2");

	//---- 点表头不选行 ----
	g_cb_fired = 0;
	YMGUI_Event_Pointer(ctx, 60, 20, 1);//表头内
	YMGUI_Event_Pointer(ctx, 60, 20, 0);
	CHECK(g_cb_fired == 0, "click in header does NOT select a row");

	//---- 拖动超阈值 → 不算点击(是滚动)----
	YMGUI_Table_SetScroll(tbl, 0);
	YMGUI_Table_SetSelectedRow(tbl, 5);
	g_cb_fired = 0;
	YMGUI_Event_Pointer(ctx, 60, 40, 1);   //按下于 row0
	YMGUI_Event_Pointer(ctx, 60, 10, 1);   //按住上移 30px(>阈值)→ Pressing 滚动
	YMGUI_Event_Pointer(ctx, 60, 10, 0);   //抬起
	CHECK(g_cb_fired == 0, "drag beyond threshold does NOT count as row click");
	CHECK(YMGUI_Table_GetSelectedRow(tbl) == 5, "selection unchanged after scroll-drag");
	CHECK(YMGUI_Table_GetScroll(tbl) == 30, "scroll-drag moved content by drag delta (30)");

	//---- 析构不崩(行链表级联释放)----
	YMGUI_Free_ObjFree(tbl);

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_table: ALL PASS\n");
	else
		printf("test_table: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
