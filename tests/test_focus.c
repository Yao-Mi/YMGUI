#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_EditView.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_focus.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: Tab 焦点轮转单测:注入 GY_KEY_TAB 驱动 YMGUI_FocusNext,验证前序序推进、
  *	              末尾回卷、无焦点起步、跳过 Hidden、无可聚焦对象不崩。判成败以 exit code 为准。
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static void flushNop(GYdisp* d, const GYrect* area, const GYpx* buf) { (void)d; (void)area; (void)buf; }

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;
	disp.flush_cb = flushNop;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Inject_SetCtx(ctx);

	//三个可聚焦输入框(前序序 = 添加序 a→b→c)
	GYOBJ a = YMGUI_Creat_TextInput_Creat(ctx->root, 10, 10, 120, 24, 64);
	GYOBJ b = YMGUI_Creat_TextInput_Creat(ctx->root, 10, 40, 120, 24, 64);
	GYOBJ c = YMGUI_Creat_TextInput_Creat(ctx->root, 10, 70, 120, 24, 64);

	//1) 无焦点起步:Tab 聚焦第一个
	CHECK(ctx->focus_obj == NULL, "初始应无焦点");
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == a, "Tab#1 应聚焦 a");

	//2) 逐次推进 a→b→c
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == b, "Tab#2 应聚焦 b");
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == c, "Tab#3 应聚焦 c");

	//3) 末尾回卷到第一个
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == a, "Tab#4 应回卷到 a");

	//4) 焦点状态位:只 a 有 Focused,b/c 无
	CHECK((a->state & GY_STATE_Focused) != 0, "a 应有 Focused 位");
	CHECK((b->state & GY_STATE_Focused) == 0, "b 不应有 Focused 位");

	//5) 跳过 Hidden:把 b 隐藏,从 a 起 Tab 应直接到 c
	YMGUI_SetFocus(ctx, a);
	b->state |= GY_STATE_Hidden;
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == c, "隐藏 b 后 Tab 应跳到 c");
	b->state &= (uint8)~GY_STATE_Hidden;

	//6) 当前焦点被隐藏:当作无焦点从头选第一个
	YMGUI_SetFocus(ctx, c);
	c->state |= GY_STATE_Hidden;
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == a, "焦点对象隐藏后 Tab 应回到 a");
	c->state &= (uint8)~GY_STATE_Hidden;

	//7) 焦点两级:SetFocus/轮转落"选择态"(不含 Editing),Tab 在选择态照旧轮转
	YMGUI_SetFocus(ctx, a);
	YMGUI_TextInput_SetText(a, "");
	CHECK((a->state & GY_STATE_Editing) == 0, "SetFocus 后应为选择态(无 Editing)");
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == b, "选择态 Tab 应轮转到 b(不插空格)");
	CHECK(strcmp(YMGUI_TextInput_GetText(a), "") == 0, "选择态 Tab 未在 a 里插空格");

	//8) 打字进编辑态:选择态下敲可打印字符 → 进编辑 + 落下该字符
	YMGUI_SetFocus(ctx, a);
	YMGUI_Inject_Key((uint32)'x', 1);
	CHECK((a->state & GY_STATE_Editing) != 0, "打字后应进编辑态");
	CHECK(strcmp(YMGUI_TextInput_GetText(a), "x") == 0, "打字应落下字符");

	//9) 编辑态 Tab = 插空格(B 方案,单行也插),不轮转
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == a, "编辑态 Tab 不应轮转");
	CHECK(strcmp(YMGUI_TextInput_GetText(a), "x ") == 0, "编辑态 Tab 应插空格");

	//10) 编辑态 Enter = 结束编辑 + 轮转到下一个(表单手感)
	YMGUI_Inject_Key(GY_KEY_ENTER, 1);
	CHECK((a->state & GY_STATE_Editing) == 0, "Enter 后 a 应退出编辑态");
	CHECK(ctx->focus_obj == b, "Enter 应轮转到 b");

	//11) EditView:选择态 Tab 轮转走(不插),编辑态 Tab 插 GY_EV_TAB_WIDTH 空格
	GYOBJ ev = YMGUI_Creat_EditView_Creat(ctx->root, 10, 100, 200, 80, 4096);
	YMGUI_EditView_SetText(ev, "");
	YMGUI_SetFocus(ctx, ev);
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj != ev, "选择态 EditView 里 Tab 应轮转走");
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "") == 0, "选择态 EditView 未插空格");
	YMGUI_SetFocus(ctx, ev);
	YMGUI_Inject_Key(GY_KEY_ENTER, 1);//进编辑态(多行:首个 Enter 只进编辑不换行)
	CHECK((ev->state & GY_STATE_Editing) != 0, "EditView Enter 进编辑态");
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == ev, "编辑态 EditView Tab 不轮转");
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "    ") == 0, "编辑态 EditView Tab 插 4 空格");
	YMGUI_Free_ObjFree(ev);

	//12) 树里无可聚焦对象:Tab 不崩、焦点保持清空
	YMGUI_Free_ObjFree(a);
	YMGUI_Free_ObjFree(b);
	YMGUI_Free_ObjFree(c);
	YMGUI_SetFocus(ctx, NULL);
	YMGUI_Inject_Key(GY_KEY_TAB, 1);
	CHECK(ctx->focus_obj == NULL, "无可聚焦对象时焦点应保持 NULL");

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);
	printf(fails ? "test_focus: %d FAIL\n" : "test_focus: ALL PASS\n", fails);
	return fails ? 1 : 0;
}
