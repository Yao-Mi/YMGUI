#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Layout.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_layout.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 轻量布局助手单测:Stack 主轴累进(竖/横)、pad 起点、gap 间隔、交叉轴对齐、
  *	              Hidden 子不占位、加子后重排;Align 九点在父盒内定位。
  *	              直接查 area 值(无需渲染),判定以退出码为准(fails 计数),stdout 仅供人读。
  ***************************************************************************************************************************/

#define SCR_W 240
#define SCR_H 240
#define BAND_H 240

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = NULL; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);

#if YMGUI_LAYOUT
	//---- 竖排 Stack:pad=10, gap=5, 三个 40x20 子 → y = 10,35,60;交叉轴 START → x=10 ----
	GYOBJ panel = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, 100, 200);
	GYOBJ a = YMGUI_Creat_Obj_Creat(panel, 0, 0, 40, 20);
	GYOBJ b = YMGUI_Creat_Obj_Creat(panel, 0, 0, 40, 20);
	GYOBJ c = YMGUI_Creat_Obj_Creat(panel, 0, 0, 40, 20);
	YMGUI_Layout_Stack(panel, GY_LAYOUT_VER, 5, 10, GY_CROSS_START);
	CHECK(a->area.y == 10 && a->area.x == 10, "vstack child0 at pad");
	CHECK(b->area.y == 35, "vstack child1 = pad + h + gap");
	CHECK(c->area.y == 60, "vstack child2 = pad + 2*(h+gap)");
	CHECK(a->area.x == 10 && b->area.x == 10 && c->area.x == 10, "vstack cross START = pad");

	//---- 交叉轴 CENTER:父宽 100,pad=10,avail=80,子宽 40 → x = 10 + (80-40)/2 = 30 ----
	YMGUI_Layout_Stack(panel, GY_LAYOUT_VER, 5, 10, GY_CROSS_CENTER);
	CHECK(a->area.x == 30, "vstack cross CENTER centers child");
	//交叉轴 END:x = 10 + (80-40) = 50
	YMGUI_Layout_Stack(panel, GY_LAYOUT_VER, 5, 10, GY_CROSS_END);
	CHECK(a->area.x == 50, "vstack cross END = pad + avail - w");

	//---- Hidden 子不占位:隐藏 b 后,c 应接在 a 之后(y=35),不留 b 的空当 ----
	YMGUI_Obj_SetHidden(b, 1);
	YMGUI_Layout_Stack(panel, GY_LAYOUT_VER, 5, 10, GY_CROSS_START);
	CHECK(a->area.y == 10, "hidden-skip: a still at pad");
	CHECK(c->area.y == 35, "hidden-skip: c takes b's slot (b not occupying)");
	YMGUI_Obj_SetHidden(b, 0);

	//---- 横排 Stack:pad=8, gap=4, 子宽 40 → x = 8,52,96;交叉轴 START → y=8 ----
	GYOBJ row = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, 200, 60);
	GYOBJ r0 = YMGUI_Creat_Obj_Creat(row, 0, 0, 40, 20);
	GYOBJ r1 = YMGUI_Creat_Obj_Creat(row, 0, 0, 40, 20);
	GYOBJ r2 = YMGUI_Creat_Obj_Creat(row, 0, 0, 40, 20);
	YMGUI_Layout_Stack(row, GY_LAYOUT_HOR, 4, 8, GY_CROSS_START);
	CHECK(r0->area.x == 8 && r1->area.x == 52 && r2->area.x == 96, "hstack x accumulates by w+gap");
	CHECK(r0->area.y == 8 && r1->area.y == 8, "hstack cross START = pad");

	//---- 加子后重排:横排再加一个,重跑应接在 r2 之后(x=140) ----
	GYOBJ r3 = YMGUI_Creat_Obj_Creat(row, 0, 0, 40, 20);
	YMGUI_Layout_Stack(row, GY_LAYOUT_HOR, 4, 8, GY_CROSS_START);
	CHECK(r3->area.x == 140, "re-flow after add: new child appended");

	//---- Align 九点:60x40 子在 100x200 父里 ----
	GYOBJ box = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, 100, 200);
	GYOBJ it = YMGUI_Creat_Obj_Creat(box, 0, 0, 60, 40);
	YMGUI_Layout_Align(it, GY_ALIGN_CENTER, 0);
	CHECK(it->area.x == 20 && it->area.y == 80, "align CENTER = ((100-60)/2, (200-40)/2)");
	YMGUI_Layout_Align(it, GY_ALIGN_TL, 5);
	CHECK(it->area.x == 5 && it->area.y == 5, "align TL = pad,pad");
	YMGUI_Layout_Align(it, GY_ALIGN_BR, 5);
	CHECK(it->area.x == 35 && it->area.y == 155, "align BR = (bw-ow-pad, bh-oh-pad)");
	YMGUI_Layout_Align(it, GY_ALIGN_TM, 0);
	CHECK(it->area.x == 20 && it->area.y == 0, "align TM = centered x, top");
#else
	printf("test_layout: YMGUI_LAYOUT=0, layout compiled out (nothing to test)\n");
#endif

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_layout: ALL PASS\n");
	else
		printf("test_layout: %d FAILED\n", fails);
	return fails ? 1 : 0;
}

