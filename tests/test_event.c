#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Button.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_event.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 无 SDL 的交互闭环单测:注入指针→命中→事件→状态改变标脏→Refresh 只重绘脏区
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

//记录 flush 覆盖的行范围(验证只重绘脏区)
static GYcoord g_flush_ymin, g_flush_ymax;
static int     g_flush_count;
static int     g_clicked;
static int     g_context_requested;
static int     g_context_dragging;
static int     g_context_released;
static int     g_context_cancelled;
static int     g_capture_during_request;
static int     g_capture_during_finish;
static int     g_released_off;
static int     g_event_clicked;
static int     g_wheel_count;
static GYcoord g_context_x, g_context_y;
static int32   g_wheel_x, g_wheel_y;

static void testFlushCb(GYdisp* d, const GYrect* area, const GYpx* buf)
{
	(void)d; (void)buf;
	if (area->y < g_flush_ymin) g_flush_ymin = area->y;
	if (area->y + area->h > g_flush_ymax) g_flush_ymax = area->y + area->h;
	g_flush_count++;
}

static void onClicked(GYOBJ btn) { (void)btn; g_clicked++; }

static void onEvent(GYOBJ obj, GYEvent e)
{
	if (e == GY_EVENT_ContextRequested)
	{
		g_context_requested++;
		g_context_x = obj->ctx->point_x;
		g_context_y = obj->ctx->point_y;
		g_capture_during_request = (obj->ctx->context_obj == obj);
	}
	else if (e == GY_EVENT_ContextDragging)
	{
		g_context_dragging++;
		g_context_x = obj->ctx->point_x;
		g_context_y = obj->ctx->point_y;
	}
	else if (e == GY_EVENT_ContextReleased)
	{
		g_context_released++;
		g_capture_during_finish = (obj->ctx->context_obj != NULL);
	}
	else if (e == GY_EVENT_ContextCancelled)
	{
		g_context_cancelled++;
		g_capture_during_finish = (obj->ctx->context_obj != NULL);
	}
	else if (e == GY_EVENT_ReleasedOff)
		g_released_off++;
	else if (e == GY_EVENT_Clicked)
		g_event_clicked++;
	else if (e == GY_EVENT_Wheel)
	{
		g_wheel_count++;
		g_wheel_x = obj->ctx->wheel_x;
		g_wheel_y = obj->ctx->wheel_y;
	}
}

static void resetFlush(void)
{
	g_flush_ymin = 30000;
	g_flush_ymax = -30000;
	g_flush_count = 0;
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
	disp.flush_cb = testFlushCb;
	disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	GYOBJ btn = YMGUI_Creat_Button_Creat(ctx->root, 100, 90, 120, 60);//屏幕 y=90..150
	YMGUI_Button_SetClicked(btn, onClicked);
	YMGUI_Inject_SetCtx(ctx);

	//首帧:整屏脏(root+btn),Refresh 后脏清空
	YMGUI_Refresh(ctx);
	CHECK(ctx->inv_cnt == 0, "after first refresh dirty cleared");

	//---- 命中测试 ----
	CHECK(YMGUI_HitTest(ctx, 110, 100) == btn, "hit inside button → btn");
	CHECK(YMGUI_HitTest(ctx, 10, 10) == ctx->root, "hit outside button → root");

	//---- 按下:btn 进入 pressed 态,并标脏 ----
	YMGUI_Inject_Pointer(110, 100, 1);
	CHECK(btn->state & GY_STATE_Pressed, "button pressed state set");
	CHECK(ctx->pressed_obj == btn, "ctx.pressed_obj == btn");
	CHECK(ctx->inv_cnt > 0, "press invalidates (dirty)");

	//刷新:脏区应只覆盖按钮所在行段(90..150),不是整屏(0..240)
	resetFlush();
	YMGUI_Refresh(ctx);
	CHECK(g_flush_ymin >= 90 && g_flush_ymax <= 150, "refresh only button's dirty rows (90..150)");
	CHECK(g_flush_ymin < g_flush_ymax, "flush happened");

	//---- 抬起(仍在按钮上):触发 Clicked,清 pressed ----
	YMGUI_Inject_Pointer(110, 100, 0);
	CHECK(!(btn->state & GY_STATE_Pressed), "button pressed cleared on release");
	CHECK(ctx->pressed_obj == NULL, "ctx.pressed_obj cleared");
	CHECK(g_clicked == 1, "clicked callback fired once");

	//---- 抬起在按钮外不算点击 ----
	YMGUI_Inject_Pointer(110, 100, 1);//按下
	YMGUI_Inject_Pointer(10, 10, 0);  //移出后抬起
	CHECK(g_clicked == 1, "release off-button does NOT click");

	//---- Button 连发默认关闭；显式开启后按延迟/间隔触发，松开不补 Click ----
	YMGUI_Inject_Pointer(110, 100, 1);
	YMGUI_Inject_Tick(1000);
	CHECK(g_clicked == 1, "button repeat is disabled by default");
	YMGUI_Inject_Pointer(110, 100, 0);
	CHECK(g_clicked == 2, "disabled repeat keeps normal click");
	YMGUI_Button_SetRepeat(btn, 400, 80);
	YMGUI_Inject_Pointer(110, 100, 1);
	YMGUI_Inject_Tick(399);
	CHECK(g_clicked == 2, "button repeat waits for delay");
	YMGUI_Inject_Tick(1);
	CHECK(g_clicked == 3, "button repeat fires at delay");
	YMGUI_Inject_Tick(240);
	CHECK(g_clicked == 4, "one tick emits at most one repeat");
	YMGUI_Inject_Pointer(110, 100, 0);
	CHECK(g_clicked == 4, "release after repeat does not add click");
	YMGUI_Button_SetRepeat(btn, 0, 0);

	//---- Wheel:派给指针位置命中的对象，横纵增量彼此独立 ----
	GYOBJ wheel_target = YMGUI_Creat_Obj_Creat(ctx->root, 230, 10, 60, 40);
	wheel_target->event_cb = onEvent;
	YMGUI_Inject_Wheel(240, 20, -2, 3);
	CHECK(g_wheel_count == 1, "wheel dispatched once to hit object");
	CHECK(g_wheel_x == -2 && g_wheel_y == 3, "wheel preserves x/y signed deltas");
	CHECK(ctx->point_x == 240 && ctx->point_y == 20, "wheel stores pointer position");
	YMGUI_Inject_Wheel(240, 20, 0, 0);
	CHECK(g_wheel_count == 1, "zero wheel delta is ignored");

	//---- 上下文请求:只命中派发,不改变焦点/按下状态 ----
	GYOBJ target = YMGUI_Creat_Obj_Creat(ctx->root, 10, 10, 50, 40);
	target->event_cb = onEvent;
	GYOBJ focus_before = ctx->focus_obj;
	YMGUI_Inject_ContextRequest(20, 20);
	CHECK(g_context_requested == 1, "context request dispatched once");
	CHECK(g_context_x == 20 && g_context_y == 20, "context request stores coordinates");
	CHECK(ctx->focus_obj == focus_before, "context request does not change focus");
	CHECK(!(target->state & GY_STATE_Pressed), "context request does not press target");
	CHECK(g_event_clicked == 0, "context request does not click");

	//---- 指针取消:ReleasedOff 一次,清状态,后续物理抬起也不 Click ----
	YMGUI_Inject_Pointer(20, 20, 1);
	CHECK(target->state & GY_STATE_Pressed, "cancel target starts pressed");
	YMGUI_Inject_PointerCancel();
	CHECK(!(target->state & GY_STATE_Pressed), "cancel clears pressed state");
	CHECK(ctx->pressed_obj == NULL && !ctx->point_pressed, "cancel clears pointer capture");
	CHECK(g_released_off == 1, "cancel dispatches ReleasedOff once");
	YMGUI_Inject_PointerCancel();
	CHECK(g_released_off == 1, "repeated cancel is idempotent");
	YMGUI_Inject_Pointer(20, 20, 0);
	CHECK(g_event_clicked == 0, "physical up after cancel does not click");

	//---- 捕获式上下文拖动:跨出对象仍归起点对象,结束/取消先清捕获 ----
	g_capture_during_request = 0;
	YMGUI_Inject_ContextBegin(20, 20);
	CHECK(ctx->context_obj == target, "context begin captures hit target");
	CHECK(g_capture_during_request, "capture is visible during request callback");
	YMGUI_Inject_ContextMove(200, 200);
	CHECK(g_context_dragging == 1, "context move dispatched to captured target");
	CHECK(g_context_x == 200 && g_context_y == 200, "context move stores current coordinates");
	CHECK(ctx->pressed_obj == NULL && !ctx->point_pressed, "context drag is independent from pointer");
	g_capture_during_finish = 1;
	YMGUI_Inject_ContextEnd(210, 205);
	CHECK(g_context_released == 1, "context end dispatches released once");
	CHECK(ctx->context_obj == NULL && !g_capture_during_finish, "context end clears capture before callback");
	YMGUI_Inject_ContextEnd(210, 205);
	CHECK(g_context_released == 1, "repeated context end is idempotent");

	YMGUI_Inject_ContextBegin(20, 20);
	g_capture_during_finish = 1;
	YMGUI_Inject_ContextCancel();
	CHECK(g_context_cancelled == 1, "context cancel dispatched once");
	CHECK(ctx->context_obj == NULL && !g_capture_during_finish, "context cancel clears capture before callback");
	YMGUI_Inject_ContextCancel();
	CHECK(g_context_cancelled == 1, "repeated context cancel is idempotent");

	//捕获对象中途销毁后 Move/End 必须安全无操作,不得重新命中底层对象。
	GYOBJ doomed = YMGUI_Creat_Obj_Creat(ctx->root, 70, 10, 40, 40);
	doomed->event_cb = onEvent;
	YMGUI_Inject_ContextBegin(80, 20);
	CHECK(ctx->context_obj == doomed, "context captures object before free");
	int drag_before = g_context_dragging;
	int release_before = g_context_released;
	YMGUI_Free_ObjFree(doomed);
	CHECK(ctx->context_obj == NULL, "free clears context capture");
	YMGUI_Inject_ContextMove(90, 20);
	YMGUI_Inject_ContextEnd(90, 20);
	CHECK(g_context_dragging == drag_before && g_context_released == release_before,
	      "events after captured object free are ignored");

	//---- 释放:级联释放不崩,引用清理 ----
	YMGUI_Free_CtxFree(ctx);

	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_event: ALL PASS\n");
	else
		printf("test_event: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
