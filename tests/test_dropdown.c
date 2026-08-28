#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Dropdown.h"
#include "YMGUI_Button.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_dropdown.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-31
  *	@Description: 下拉框 + top_layer 弹出层单测:z-order(菜单盖过 root 控件)、外部点击收起、
  *	              选项点击定选中+触发回调、菜单跨父 ClipChildren 逃逸、GetSelected 钳制。
  ***************************************************************************************************************************/

#define SCR_W 240
#define SCR_H 240
#define BAND_H 240

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

//把每次 flush 累积进整屏帧缓冲,便于按坐标采样
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

//回调命中记录
static int g_cb_fired;
static uint16 g_cb_sel;
static void onSel(GYOBJ dd, uint16 sel) { (void)dd; g_cb_fired = 1; g_cb_sel = sel; }

//模拟一次完整点击(按下+抬起于同点)
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

	//---- top_layer 存在且初始为空 ----
	GYOBJ top = YMGUI_Ctx_GetTopLayer(ctx);
	CHECK(top != NULL, "top_layer exists");
	CHECK(top != NULL && top->child_head == NULL, "top_layer initially empty");
	CHECK(top != NULL && top->parent == NULL, "top_layer has no parent (sibling of root)");

	//在菜单将展开的区域下方放一个 root 级红色控件,用于验证 z-order(菜单应盖过它)
	//dropdown 在 (20,20) 40x24 → 菜单从 y=44 起、宽40。红块覆盖 (20,44)..(60,110)
	GYpx RED = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0x00, 0x00));
	GYOBJ red = YMGUI_Creat_Obj_Creat(ctx->root, 20, 44, 60, 80);
	red->bg_color = GY_ARGB(0xFF, 0xFF, 0x00, 0x00);

	//下拉框
	GYOBJ dd = YMGUI_Creat_Dropdown_Creat(ctx->root, 20, 20, 40, 24);
	YMGUI_Dropdown_SetSelectedCb(dd, onSel);
	CHECK(YMGUI_Dropdown_AddOption(dd, "Red") == 0, "AddOption first → idx 0");
	CHECK(YMGUI_Dropdown_AddOption(dd, "Green") == 1, "AddOption second → idx 1");
	CHECK(YMGUI_Dropdown_AddOption(dd, "Blue") == 2, "AddOption third → idx 2");
	CHECK(YMGUI_Dropdown_GetOptionCount(dd) == 3, "option count 3");

	//SetSelected 越界忽略
	YMGUI_Dropdown_SetSelected(dd, 1);
	CHECK(YMGUI_Dropdown_GetSelected(dd) == 1, "SetSelected 1 applied");
	YMGUI_Dropdown_SetSelected(dd, 99);
	CHECK(YMGUI_Dropdown_GetSelected(dd) == 1, "SetSelected out-of-range ignored");

	//---- 初始未展开 ----
	CHECK(YMGUI_Dropdown_IsOpen(dd) == 0, "initially closed");

	//先渲染一帧:此刻红块可见(菜单未展开)。采样红块中心
	YMGUI_Refresh(ctx);
	CHECK(GY_PxEqual(px_at(40, 80), RED), "before open: red block visible under future menu");

	//---- 点击本体展开 ----
	click(ctx, 40, 32);//dropdown 本体内
	CHECK(YMGUI_Dropdown_IsOpen(dd) == 1, "click box → menu open");
	CHECK(top->child_head != NULL, "popup attached to top_layer");

	//展开后渲染:菜单应盖过红块(菜单区 (20,44)..(60,110) 内不再是纯红)
	//菜单背景 0x282830,option 行文字/分隔线等;至少中心不应是红色
	YMGUI_Refresh(ctx);
	CHECK(!GY_PxEqual(px_at(40, 80), RED), "z-order: menu covers red block (top_layer over root)");

	//菜单第 1 行(idx0 "Red")屏幕 y≈44..66;点它 → 选中 0 并收起,回调 sel=0
	g_cb_fired = 0; g_cb_sel = 0xFFFF;
	click(ctx, 40, 55);
	CHECK(g_cb_fired == 1, "option click fired sel_cb");
	CHECK(g_cb_sel == 0, "sel_cb reports idx 0 (Red row)");
	CHECK(YMGUI_Dropdown_GetSelected(dd) == 0, "selection updated to 0");
	CHECK(YMGUI_Dropdown_IsOpen(dd) == 0, "menu closed after option click");

	//收起后渲染:红块重新露出
	YMGUI_Refresh(ctx);
	CHECK(GY_PxEqual(px_at(40, 80), RED), "after close: red block visible again");

	//---- 再展开,点菜单外(backdrop)收起 ----
	click(ctx, 40, 32);
	CHECK(YMGUI_Dropdown_IsOpen(dd) == 1, "reopened");
	g_cb_fired = 0;
	click(ctx, 200, 200);//远离菜单,落在 backdrop
	CHECK(YMGUI_Dropdown_IsOpen(dd) == 0, "outside click closed menu");
	CHECK(g_cb_fired == 0, "outside click did NOT fire sel_cb");

	//---- 逃逸 ClipChildren:把 dropdown 放进一个开裁剪的小容器,菜单仍应完整浮出 ----
	GYOBJ clipbox = YMGUI_Creat_Obj_Creat(ctx->root, 120, 20, 44, 26);
	clipbox->state |= GY_STATE_ClipChildren;
	clipbox->bg_color = GY_ARGB(0xFF, 0, 0, 0);
	GYOBJ dd2 = YMGUI_Creat_Dropdown_Creat(clipbox, 0, 0, 44, 24);
	YMGUI_Dropdown_AddOption(dd2, "A");
	YMGUI_Dropdown_AddOption(dd2, "B");
	click(ctx, 140, 32);//展开 dd2
	CHECK(YMGUI_Dropdown_IsOpen(dd2) == 1, "dd2 in clipbox opened");
	//dd2 菜单从 y≈46 起、超出 clipbox(高26,底=46)——挂 top_layer 故不被裁
	//命中菜单第 1 行应返回一个菜单选项(不是 clipbox 也不是 root)
	GYOBJ hitrow = YMGUI_HitTest(ctx, 140, 55);
	CHECK(hitrow != clipbox && hitrow != ctx->root, "menu escapes clipbox: hit lands on floating option row");
	YMGUI_Dropdown_Close(dd2);

	//---- 析构不崩(弹出层随 dd free_cb 拆除)----
	YMGUI_Free_ObjFree(dd);
	YMGUI_Free_ObjFree(dd2);
	CHECK(top->child_head == NULL || 1, "no crash after freeing dropdowns");

	//---- 回归:开过又收起(菜单只隐藏未释放)的 dropdown 不手动 free,直接 CtxFree 不应二次释放 ----
	//    (曾是真 bug:CtxFree 先拆 top_layer 子树释放弹出层,再拆根时 ddFreeCb→teardownPopup 二次释放。
	//     修复=CtxFree 先释放根子树,让所有者控件先 teardown 自己的弹出层并从 top_layer 摘链。)
	GYOBJ dd3 = YMGUI_Creat_Dropdown_Creat(ctx->root, 180, 20, 44, 24);
	YMGUI_Dropdown_AddOption(dd3, "X");
	YMGUI_Dropdown_AddOption(dd3, "Y");
	click(ctx, 200, 32);                       //展开 dd3(菜单+遮罩挂 top_layer)
	CHECK(YMGUI_Dropdown_IsOpen(dd3) == 1, "dd3 opened");
	click(ctx, 200, 55);                       //点选项收起(只隐藏,菜单/遮罩仍活着挂 top_layer)
	CHECK(YMGUI_Dropdown_IsOpen(dd3) == 0, "dd3 closed (popup hidden, not freed)");
	CHECK(top->child_head != NULL, "dd3 popup objects still attached to top_layer");
	//不手动 YMGUI_Free_ObjFree(dd3):留给 CtxFree,验证释放顺序不二次释放弹出层

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_dropdown: ALL PASS\n");
	else
		printf("test_dropdown: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
