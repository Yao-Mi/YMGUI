#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_MsgBox.h"
#include "YMGUI_Button.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_msgbox.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-05
  *	@Description: 模态对话框单测:输入模态锁死(显示时点底层被遮罩吞掉、底层回调不触发)、
  *	              点卡片按钮触发对应回调 + 隐藏、隐藏后底层重新可点、多按钮 index 正确、
  *	              CtxFree 不崩(不手动 free,验证挂 top_layer 的模态由 CtxFree 兜底级联)。
  ***************************************************************************************************************************/

#define SCR_W 320
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

//底层控件点击记录
static int g_bg_clicked;
static void bgClickedCb(GYOBJ btn) { (void)btn; g_bg_clicked++; }

//模态按钮回调记录
static int g_mb_fired;
static int g_mb_idx;
static void mbBtn(GYOBJ mb, int idx) { (void)mb; g_mb_fired++; g_mb_idx = idx; }

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
	GYOBJ top = YMGUI_Ctx_GetTopLayer(ctx);

	//底层放一个真按钮(全屏左上角一块),用于验证"模态锁死时点它无效、解锁后又生效"
	GYOBJ bg_btn = YMGUI_Creat_Button_Creat(ctx->root, 10, 10, 80, 30);
	YMGUI_Button_SetClicked(bg_btn, bgClickedCb);

	//---- 创建模态,初始隐藏 ----
	GYOBJ mb = YMGUI_Creat_MsgBox_Creat(ctx);
	CHECK(mb != NULL, "MsgBox created");
	CHECK(mb != NULL && mb->parent == top, "MsgBox attached to top_layer");
	CHECK(YMGUI_MsgBox_IsShown(mb) == 0, "MsgBox initially hidden");

	YMGUI_MsgBox_SetTitle(mb, "Alarm");
	YMGUI_MsgBox_SetText(mb, "Time is up!\nSecond line");
	CHECK(YMGUI_MsgBox_AddButton(mb, "OK", mbBtn) == 0, "AddButton OK → idx 0");
	CHECK(YMGUI_MsgBox_AddButton(mb, "Cancel", mbBtn) == 1, "AddButton Cancel → idx 1");

	//---- 隐藏时:点底层按钮应生效(无模态锁),因 top_layer 里模态是 Hidden 被 hit-test 跳过 ----
	g_bg_clicked = 0;
	click(ctx, 40, 25);
	CHECK(g_bg_clicked == 1, "hidden modal: bg button clickable");

	//---- Show:显示模态 ----
	YMGUI_MsgBox_Show(mb);
	CHECK(YMGUI_MsgBox_IsShown(mb) == 1, "Show → shown");
	CHECK(mb->child_head != NULL, "card built as child of backdrop after Show");

	//渲染一帧:遮罩变暗底层。底层按钮区 (10,10)-(90,40) 中心此刻被半透明遮罩覆盖,
	//像素应不再是按钮常态纯色(混了黑遮罩)。用"点击不生效"作为模态硬证据(下条)。
	YMGUI_Refresh(ctx);

	//---- 模态锁死:点底层按钮区应被遮罩吞掉,底层回调不触发 ----
	g_bg_clicked = 0;
	click(ctx, 40, 25);//落在底层按钮位置,但被全屏 backdrop 拦截
	CHECK(g_bg_clicked == 0, "modal shown: bg button click swallowed by backdrop");

	//命中测试:点底层按钮位置应命中 backdrop(mb)或其子,而非底层按钮
	GYOBJ h = YMGUI_HitTest(ctx, 40, 25);
	CHECK(h != bg_btn, "hit-test at bg button lands on modal, not bg button");

	//---- 点模态"OK"按钮(idx 0)→ 触发回调 + 隐藏 ----
	//卡片居中,按钮在卡片底部。回读第一个按钮的绝对矩形算中心点。
	GYOBJ card = mb->child_head;
	GYOBJ b0 = (card != NULL) ? card->child_head : NULL;//第一个按钮
	CHECK(b0 != NULL, "first button object exists");
	g_mb_fired = 0; g_mb_idx = -1;
	if (b0 != NULL)
	{
		GYrect a0;
		YMGUI_Obj_GetAbsArea(b0, &a0);
		click(ctx, (GYcoord)(a0.x + a0.w / 2), (GYcoord)(a0.y + a0.h / 2));
	}
	CHECK(g_mb_fired == 1, "OK button fired callback");
	CHECK(g_mb_idx == 0, "callback reports idx 0 (OK)");
	CHECK(YMGUI_MsgBox_IsShown(mb) == 0, "modal hidden after button click");

	//---- 解锁后:底层按钮重新可点 ----
	YMGUI_Refresh(ctx);
	g_bg_clicked = 0;
	click(ctx, 40, 25);
	CHECK(g_bg_clicked == 1, "after close: bg button clickable again");

	//---- 再 Show,点第二个按钮(Cancel, idx 1)验证 index ----
	YMGUI_MsgBox_Show(mb);
	card = mb->child_head;
	//第二个按钮 = card 的第二个子(按钮按建序为子链;child_head 是先建的 b0,其 sibling 是 b1)
	GYOBJ b1 = (card != NULL && card->child_head != NULL) ? card->child_head->sibling : NULL;
	CHECK(b1 != NULL, "second button object exists");
	g_mb_fired = 0; g_mb_idx = -1;
	if (b1 != NULL)
	{
		GYrect a1;
		YMGUI_Obj_GetAbsArea(b1, &a1);
		click(ctx, (GYcoord)(a1.x + a1.w / 2), (GYcoord)(a1.y + a1.h / 2));
	}
	CHECK(g_mb_fired == 1, "Cancel button fired callback");
	CHECK(g_mb_idx == 1, "callback reports idx 1 (Cancel)");
	CHECK(YMGUI_MsgBox_IsShown(mb) == 0, "modal hidden after Cancel");

	//---- ClearButtons + 重配置:Show 一个纯提示(单按钮)----
	YMGUI_MsgBox_ClearButtons(mb);
	CHECK(YMGUI_MsgBox_AddButton(mb, "Got it", NULL) == 0, "re-add single button");
	YMGUI_MsgBox_Show(mb);
	card = mb->child_head;
	int nbtn = 0;
	for (GYOBJ c = (card ? card->child_head : NULL); c != NULL; c = c->sibling)
		nbtn++;
	CHECK(nbtn == 1, "rebuilt card has exactly 1 button after ClearButtons+AddButton");
	YMGUI_MsgBox_Close(mb);
	CHECK(YMGUI_MsgBox_IsShown(mb) == 0, "Close hides modal");

	//---- 析构不崩:不手动 free 模态(显示态留着),直接 CtxFree 验证 top_layer 兜底级联 ----
	YMGUI_MsgBox_Show(mb);//故意留在显示态
	CHECK(YMGUI_MsgBox_IsShown(mb) == 1, "modal shown before CtxFree");
	YMGUI_Free_CtxFree(ctx);//应先释放 root 子树再兜底释放 top_layer(含模态),不崩、不二次释放
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_msgbox: ALL PASS\n");
	else
		printf("test_msgbox: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
