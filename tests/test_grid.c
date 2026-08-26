#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Grid.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_grid.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 网格控件单测:建网格/行列数、单元格文本存取、单击选中+回调、双击编辑回调、
  *	              SetSelected 不触发回调、二维滚动钳位、EnsureVisible、方向键移动选中、
  *	              表头 sticky 渲染不崩、析构级联不崩。判成败以 exit code 为准。
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 200
#define BAND_H 200

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

static int g_sel_calls = 0;
static int g_sel_row = -1, g_sel_col = -1;
static void onSelect(GYOBJ g, uint16 r, uint16 c) { (void)g; g_sel_calls++; g_sel_row = r; g_sel_col = c; }

static int g_edit_calls = 0;
static int g_edit_row = -1, g_edit_col = -1;
static void onEdit(GYOBJ g, uint16 r, uint16 c) { (void)g; g_edit_calls++; g_edit_row = r; g_edit_col = c; }

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

	//网格 (10,10) 300x180;10 行 x 8 列
	GYOBJ grid = YMGUI_Creat_Grid_Creat(ctx->root, 10, 10, 300, 180, 10, 8);
	CHECK(grid != NULL, "Creat 返回非空");
	YMGUI_Grid_SetSelectCb(grid, onSelect);
	YMGUI_Grid_SetEditCb(grid, onEdit);
	//默认 row_h=22 head_h=22 head_col_w=40 col_w=72

	//---- 行列数(容量钳制不越界)----
	CHECK(YMGUI_Grid_GetRowCount(grid) == 10, "行数 10");
	CHECK(YMGUI_Grid_GetColCount(grid) == 8, "列数 8");

	//---- 单元格文本存取 ----
	YMGUI_Grid_SetCellText(grid, 0, 0, "10");
	YMGUI_Grid_SetCellText(grid, 2, 3, "hello");
	CHECK(strcmp(YMGUI_Grid_GetCellText(grid, 0, 0), "10") == 0, "格 (0,0)=10");
	CHECK(strcmp(YMGUI_Grid_GetCellText(grid, 2, 3), "hello") == 0, "格 (2,3)=hello");
	CHECK(strcmp(YMGUI_Grid_GetCellText(grid, 1, 1), "") == 0, "未设格为空串");
	//越界存取不崩、返回空串
	YMGUI_Grid_SetCellText(grid, 100, 100, "x");
	CHECK(strcmp(YMGUI_Grid_GetCellText(grid, 100, 100), "") == 0, "越界读返回空串");
	//超长截断到 CELL_LEN-1
	char longtxt[64];
	memset(longtxt, 'A', sizeof(longtxt));
	longtxt[63] = '\0';
	YMGUI_Grid_SetCellText(grid, 0, 1, longtxt);
	CHECK(strlen(YMGUI_Grid_GetCellText(grid, 0, 1)) == GY_GRID_CELL_LEN - 1, "超长截断");

	//---- SetSelected 不触发回调 ----
	int sc = g_sel_calls;
	YMGUI_Grid_SetSelected(grid, 1, 2);
	int32 sr, scl;
	YMGUI_Grid_GetSelected(grid, &sr, &scl);
	CHECK(sr == 1 && scl == 2, "SetSelected 生效");
	CHECK(g_sel_calls == sc, "SetSelected 不触发选中回调");
	//越界 → 取消选中
	YMGUI_Grid_SetSelected(grid, 100, 0);
	YMGUI_Grid_GetSelected(grid, &sr, &scl);
	CHECK(sr == -1 && scl == -1, "越界选中 → 取消");

	//---- 单击选中 + 回调:格 (0,0) 屏幕位置 = 网格(10,10)+行号列40+列名行22 = (50,32)..(122,54) ----
	//点 (60,40) 落在数据格 row0 col0
	g_sel_calls = 0;
	YMGUI_Inject_Pointer(60, 40, 1);
	YMGUI_Inject_Pointer(60, 40, 0);//Clicked
	CHECK(g_sel_calls == 1, "单击触发选中回调一次");
	CHECK(g_sel_row == 0 && g_sel_col == 0, "单击选中格 (0,0)");
	YMGUI_Grid_GetSelected(grid, &sr, &scl);
	CHECK(sr == 0 && scl == 0, "选中态 = (0,0)");

	//点第二列(col1 屏幕 x = 50+72=122..194),y=40 → (0,1)
	YMGUI_Inject_Pointer(140, 40, 1);
	YMGUI_Inject_Pointer(140, 40, 0);
	CHECK(g_sel_row == 0 && g_sel_col == 1, "单击选中格 (0,1)");

	//点表头区(y=20,在列名行内)不应选中数据格
	int before = g_sel_calls;
	YMGUI_Inject_Pointer(60, 20, 1);
	YMGUI_Inject_Pointer(60, 20, 0);
	CHECK(g_sel_calls == before, "点列名行不选中数据格");

	//---- 双击编辑回调:双击格 (0,0) ----
	g_edit_calls = 0;
	YMGUI_Inject_DoubleClick(60, 40);
	CHECK(g_edit_calls == 1, "双击触发编辑回调一次");
	CHECK(g_edit_row == 0 && g_edit_col == 0, "双击编辑格 (0,0)");

	//---- 二维滚动钳位 ----
	//内容宽 = 8*72=576,视口宽 = 300-40=260 → maxx=316;内容高=10*22=220,视口高=180-22=158 → maxy=62
	YMGUI_Grid_SetScroll(grid, 100000, 100000);
	int32 gx, gy;
	YMGUI_Grid_GetScroll(grid, &gx, &gy);
	CHECK(gx == 576 - 260, "scroll_x 钳到 316");
	CHECK(gy == 220 - 158, "scroll_y 钳到 62");
	YMGUI_Grid_SetScroll(grid, -50, -50);
	YMGUI_Grid_GetScroll(grid, &gx, &gy);
	CHECK(gx == 0 && gy == 0, "负滚动钳到 0");

	//---- EnsureVisible:滚到 0 后 EnsureVisible 最后一格,应把它拉进视口 ----
	YMGUI_Grid_SetScroll(grid, 0, 0);
	YMGUI_Grid_EnsureVisible(grid, 9, 7);
	YMGUI_Grid_GetScroll(grid, &gx, &gy);
	CHECK(gx > 0 && gy > 0, "EnsureVisible 最后一格产生滚动");
	//再 EnsureVisible 首格 → 滚回 0
	YMGUI_Grid_EnsureVisible(grid, 0, 0);
	YMGUI_Grid_GetScroll(grid, &gx, &gy);
	CHECK(gx == 0 && gy == 0, "EnsureVisible 首格滚回 0");

	//---- GetCellRect:滚动 0 时 (0,0) 格屏幕矩形 = 网格(10,10)+行号列40+列名行22 起,72x22 ----
	GYrect cr;
	uint8 vis = YMGUI_Grid_GetCellRect(grid, 0, 0, &cr);
	CHECK(vis == 1, "GetCellRect (0,0) 可见");
	CHECK(cr.x == 50 && cr.y == 32 && cr.w == 72 && cr.h == 22, "GetCellRect (0,0) 矩形正确");
	CHECK(YMGUI_Grid_GetCellRect(grid, 99, 99, &cr) == 0, "GetCellRect 越界返回 0");
	//滚到底后 (0,0) 完全滚出单元格视口(被裁没)→ 不可见
	YMGUI_Grid_SetScroll(grid, 100000, 100000);
	CHECK(YMGUI_Grid_GetCellRect(grid, 0, 0, &cr) == 0, "GetCellRect 滚出格返回 0");
	YMGUI_Grid_SetScroll(grid, 0, 0);

	//---- 方向键移动选中格(控件 Focusable,聚焦后收键)----
	YMGUI_Grid_SetScroll(grid, 0, 0);
	YMGUI_Grid_SetSelected(grid, 0, 0);
	YMGUI_SetFocus(ctx, grid);
	YMGUI_Inject_Key(GY_KEY_RIGHT, 1);
	YMGUI_Grid_GetSelected(grid, &sr, &scl);
	CHECK(sr == 0 && scl == 1, "方向键右移到 (0,1)");
	YMGUI_Inject_Key(GY_KEY_DOWN, 1);
	YMGUI_Grid_GetSelected(grid, &sr, &scl);
	CHECK(sr == 1 && scl == 1, "方向键下移到 (1,1)");
	YMGUI_Inject_Key(GY_KEY_LEFT, 1);
	YMGUI_Inject_Key(GY_KEY_UP, 1);
	YMGUI_Grid_GetSelected(grid, &sr, &scl);
	CHECK(sr == 0 && scl == 0, "方向键回到 (0,0)");
	//回车触发编辑
	g_edit_calls = 0;
	YMGUI_Inject_Key(GY_KEY_ENTER, 1);
	CHECK(g_edit_calls == 1, "回车触发编辑回调");
	CHECK(g_edit_row == 0 && g_edit_col == 0, "回车编辑格 (0,0)");

	//---- 渲染一帧(含 sticky 表头/滚动/选中高亮)不崩 ----
	YMGUI_Grid_SetScroll(grid, 30, 20);
	YMGUI_Refresh(ctx);
	//采样:左上角块区域(网格左上 10,10 起 40x22)应被表头底色填充(非黑背景)
	GYpx corner = g_fb[12 * SCR_W + 14];
	CHECK(corner != GY_ColorToPx(GY_ARGB(0xFF, 0, 0, 0)), "左上角块非黑(表头绘制)");

	//---- 列宽/行高/行号列宽设置 ----
	YMGUI_Grid_SetColWidth(grid, 0, 120);
	YMGUI_Grid_SetRowHeight(grid, 26, 24);
	YMGUI_Grid_SetHeadColWidth(grid, 50);
	YMGUI_Refresh(ctx);//重绘不崩

	//---- 矩形选区:SetSelectedRange / GetSelectedRange 往返(含反向归一)----
	YMGUI_Grid_SetScroll(grid, 0, 0);
	YMGUI_Grid_SetSelectedRange(grid, 2, 3, 1, 1);//反向:活动格在锚点左上
	{
		int32 r0 = -9, c0 = -9, r1 = -9, c1 = -9;
		YMGUI_Grid_GetSelectedRange(grid, &r0, &c0, &r1, &c1);
		CHECK(r0 == 1 && c0 == 1 && r1 == 2 && c1 == 3, "选区归一 (1,1)-(2,3)");
		//活动格 = 传入的 (1,1);GetSelected 向后兼容返回活动格
		YMGUI_Grid_GetSelected(grid, &sr, &scl);
		CHECK(sr == 1 && scl == 1, "GetSelected 返回活动格 (1,1)");
	}
	//越界 → 取消选中
	YMGUI_Grid_SetSelectedRange(grid, 0, 0, 999, 0);
	{
		int32 r0 = 0, c0 = 0, r1 = 0, c1 = 0;
		YMGUI_Grid_GetSelectedRange(grid, &r0, &c0, &r1, &c1);
		CHECK(r0 == -1 && c0 == -1 && r1 == -1 && c1 == -1, "越界选区取消");
	}

	//---- 合并区:MergeCells / GetMergeAt / 命中归一 / Unmerge ----
	CHECK(YMGUI_Grid_MergeCells(grid, 1, 1, 2, 3) == 1, "合并 (1,1)-(2,3) 成功");
	{
		int32 r0 = 0, c0 = 0, r1 = 0, c1 = 0;
		//区内任一格都能查到整片(归一)
		CHECK(YMGUI_Grid_GetMergeAt(grid, 2, 2, &r0, &c0, &r1, &c1) == 1, "区内格命中合并");
		CHECK(r0 == 1 && c0 == 1 && r1 == 2 && c1 == 3, "合并区归一正确");
		CHECK(YMGUI_Grid_GetMergeAt(grid, 0, 0, NULL, NULL, NULL, NULL) == 0, "区外格不命中");
	}
	CHECK(YMGUI_Grid_MergeCells(grid, 2, 2, 4, 4) == 0, "重叠合并被拒");
	CHECK(YMGUI_Grid_MergeCells(grid, 5, 5, 5, 5) == 0, "单格合并被拒");
	//GetCellRect 对区内非锚点格应归一到整片(宽 = 3 列之和,高 = 2 行)
	{
		GYrect mr;
		if (YMGUI_Grid_GetCellRect(grid, 2, 3, &mr))
			CHECK(mr.w > 0 && mr.h > 0, "合并区 GetCellRect 有效");
	}
	YMGUI_Refresh(ctx);//含合并区渲染不崩
	YMGUI_Grid_UnmergeAt(grid, 2, 2);
	CHECK(YMGUI_Grid_GetMergeAt(grid, 2, 2, NULL, NULL, NULL, NULL) == 0, "取消合并后不命中");

	//---- 每格对齐:SetCellAlign / GetCellAlign 往返 ----
	YMGUI_Grid_SetCellAlign(grid, 0, 0, GY_ALIGN_CENTER);
	YMGUI_Grid_SetCellAlign(grid, 0, 1, GY_ALIGN_RIGHT);
	CHECK(YMGUI_Grid_GetCellAlign(grid, 0, 0) == GY_ALIGN_CENTER, "对齐=居中");
	CHECK(YMGUI_Grid_GetCellAlign(grid, 0, 1) == GY_ALIGN_RIGHT, "对齐=右");
	CHECK(YMGUI_Grid_GetCellAlign(grid, 0, 2) == GY_ALIGN_LEFT, "默认对齐=左");
	YMGUI_Grid_SetCellAlign(grid, 0, 0, 99);//非法值忽略
	CHECK(YMGUI_Grid_GetCellAlign(grid, 0, 0) == GY_ALIGN_CENTER, "非法对齐值被忽略");
	YMGUI_Refresh(ctx);//对齐渲染不崩

	//---- 尺寸 getter + 单行行高(修:行高应只改选中行,不是全体)----
	YMGUI_Grid_SetColWidth(grid, 2, 88);
	CHECK(YMGUI_Grid_GetColWidth(grid, 2) == 88, "GetColWidth 读到 88");
	CHECK(YMGUI_Grid_GetColWidth(grid, 200) == 0, "GetColWidth 越界=0");
	//先全局铺 22,再只把第 3 行设 40,验证别的行不变
	YMGUI_Grid_SetRowHeight(grid, 22, 22);
	YMGUI_Grid_SetRowHeightAt(grid, 3, 40);
	CHECK(YMGUI_Grid_GetRowHeightAt(grid, 3) == 40, "第 3 行高=40");
	CHECK(YMGUI_Grid_GetRowHeightAt(grid, 2) == 22, "第 2 行仍=22(单行不牵连)");
	CHECK(YMGUI_Grid_GetRowHeightAt(grid, 4) == 22, "第 4 行仍=22");
	YMGUI_Grid_SetRowHeightAt(grid, 3, 0);//<=0 忽略
	CHECK(YMGUI_Grid_GetRowHeightAt(grid, 3) == 40, "行高<=0 被忽略");
	YMGUI_Grid_SetRowHeightAt(grid, 22, 40);
	CHECK(YMGUI_Grid_GetRowHeightAt(grid, 22) == 22, "越界设行高无效,读回默认");

	//---- 插入行:合并区随之移动 ----
	YMGUI_Grid_SetRowHeight(grid, 22, 22);//归位
	CHECK(YMGUI_Grid_MergeCells(grid, 4, 1, 5, 2) == 1, "为插入测试建合并 (4,1)-(5,2)");
	YMGUI_Grid_InsertRow(grid, 2);//插在第 2 行前:合并区整体下移一行 → (5,1)-(6,2)
	{
		int32 r0 = 0, c0 = 0, r1 = 0, c1 = 0;
		CHECK(YMGUI_Grid_GetMergeAt(grid, 5, 1, &r0, &c0, &r1, &c1) == 1, "插入后合并区在 (5,1)");
		CHECK(r0 == 5 && c0 == 1 && r1 == 6 && c1 == 2, "插入行后合并区下移正确");
	}
	//跨插入点的合并区应增高
	YMGUI_Grid_UnmergeAt(grid, 5, 1);
	CHECK(YMGUI_Grid_MergeCells(grid, 3, 0, 4, 0) == 1, "建跨插入点合并 (3,0)-(4,0)");
	YMGUI_Grid_InsertRow(grid, 4);//插在合并区中间 → 高度 +1 → (3,0)-(5,0)
	{
		int32 r0 = 0, c0 = 0, r1 = 0, c1 = 0;
		CHECK(YMGUI_Grid_GetMergeAt(grid, 3, 0, &r0, &c0, &r1, &c1) == 1, "跨插入点合并仍在");
		CHECK(r1 == 5, "跨插入点合并增高到 r1=5");
	}
	YMGUI_Grid_UnmergeAt(grid, 3, 0);

	//---- 删除行:整片落在被删行的合并区丢弃;插入/删除后选区清空 ----
	CHECK(YMGUI_Grid_MergeCells(grid, 8, 0, 8, 2) == 1, "建单行合并 (8,0)-(8,2)");
	YMGUI_Grid_SetSelected(grid, 8, 0);
	YMGUI_Grid_DeleteRow(grid, 8);//删掉该行 → 合并区整片消失
	CHECK(YMGUI_Grid_GetMergeAt(grid, 8, 0, NULL, NULL, NULL, NULL) == 0, "删行后合并区消失");
	{
		int32 sr2 = 9, sc2 = 9;
		YMGUI_Grid_GetSelected(grid, &sr2, &sc2);
		CHECK(sr2 == -1 && sc2 == -1, "插入/删除后选区清空");
	}

	//---- 插入列 / 删除列:合并区水平移动 ----
	CHECK(YMGUI_Grid_MergeCells(grid, 0, 3, 1, 4) == 1, "建合并 (0,3)-(1,4)");
	YMGUI_Grid_InsertCol(grid, 1);//列右移 → (0,4)-(1,5)
	{
		int32 r0 = 0, c0 = 0, r1 = 0, c1 = 0;
		CHECK(YMGUI_Grid_GetMergeAt(grid, 0, 4, &r0, &c0, &r1, &c1) == 1, "插入列后合并在 (0,4)");
		CHECK(c0 == 4 && c1 == 5, "插入列后合并右移正确");
	}
	YMGUI_Grid_DeleteCol(grid, 1);//撤回 → (0,3)-(1,4)
	{
		int32 r0 = 0, c0 = 0, r1 = 0, c1 = 0;
		CHECK(YMGUI_Grid_GetMergeAt(grid, 0, 3, &r0, &c0, &r1, &c1) == 1, "删列后合并回 (0,3)");
		CHECK(c0 == 3 && c1 == 4, "删列后合并左移正确");
	}
	YMGUI_Grid_UnmergeAt(grid, 0, 3);
	YMGUI_Refresh(ctx);//插入/删除后重绘不崩

	//---- 越界插入/删除:忽略,不崩 ----
	YMGUI_Grid_InsertRow(grid, 999);
	YMGUI_Grid_DeleteRow(grid, 999);
	YMGUI_Grid_InsertCol(grid, 999);
	YMGUI_Grid_DeleteCol(grid, 999);

	//---- 析构级联不崩 ----
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_grid: ALL PASS\n");
	else
		printf("test_grid: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
