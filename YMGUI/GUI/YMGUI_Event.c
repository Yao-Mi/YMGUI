#include "YMGUI_Event.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Event.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 事件分发 + 命中测试。指针注入 → 命中 → 派发 → 控件改状态标脏
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief 递归命中:后序遍历(子在父上层),返回最深命中对象
  */
static GYOBJ hitRec(GYOBJ obj, GYcoord x, GYcoord y)
{
	GYrect abs;
	if (obj->state & GY_STATE_Hidden)
		return NULL;
	YMGUI_Obj_GetAbsArea(obj, &abs);
	if (!GY_Rect_Contains(&abs, x, y))
		return NULL;

	//先看子(后添加的兄弟在上层,取最后一个命中的子)
	GYOBJ hit = NULL;
	GYOBJ c = obj->child_head;
	while (c != NULL)
	{
		GYOBJ h = hitRec(c, x, y);
		if (h != NULL)
			hit = h;//覆盖:靠后的兄弟层级更高
		c = c->sibling;
	}
	if (hit != NULL)
		return hit;
	return obj;//子都没命中,命中自己
}

/**
  * @brief 命中测试入口
  */
GYOBJ YMGUI_HitTest(GYCTX ctx, GYcoord x, GYcoord y)
{
	gy_assert(ctx && ctx->root);
	gy_log_explain((ctx == NULL) || (ctx->root == NULL), GY_LOG_PtrI, "上下文或根不存在");
	//先测顶层(弹出层):它渲染在最上,命中也应优先。只有当顶层有子且命中到
	//某个子(而非顶层这个透明容器本身)时才采信 —— 否则落到 root。
	//顶层透明无 draw_cb,全屏铺开,若直接返回它本身会吞掉所有点击。
	if (ctx->top_layer != NULL && ctx->top_layer->child_head != NULL)
	{
		GYOBJ h = hitRec(ctx->top_layer, x, y);
		if (h != NULL && h != ctx->top_layer)
			return h;
	}
	GYOBJ h = hitRec(ctx->root, x, y);
	return (h != NULL) ? h : ctx->root;
}

/**
  * @brief 派发事件到对象的 event_cb(空回调则忽略)
  */
static void sendEvent(GYOBJ obj, GYEvent e)
{
	if (obj != NULL && obj->event_cb != NULL)
		obj->event_cb(obj, e);
}

/**
  * @brief 设置焦点对象。切换时旧焦点收 FocusLost,新焦点收 FocusGot,均标状态位
  */
void YMGUI_SetFocus(GYCTX ctx, GYOBJ obj)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	if (ctx->focus_obj == obj)
		return;
	if (ctx->focus_obj != NULL)
	{
		//失焦即退出编辑态(焦点两级:失焦一定不在编辑)
		ctx->focus_obj->state &= (uint8)~(GY_STATE_Focused | GY_STATE_Editing);
		sendEvent(ctx->focus_obj, GY_EVENT_FocusLost);
	}
	ctx->focus_obj = obj;
	if (obj != NULL)
	{
		//新焦点默认落"选择态"(Focused 不含 Editing);点击路径会再补 Editing
		obj->state |= GY_STATE_Focused;
		obj->state &= (uint8)~GY_STATE_Editing;
		sendEvent(obj, GY_EVENT_FocusGot);
	}
}

/**
  * @brief 焦点轮转的前序遍历状态。一趟遍历里收集:第一个可聚焦对象、
  *        当前焦点之后紧跟的那个可聚焦对象(即目标)。零堆分配。
  */
typedef struct
{
	GYOBJ cur;    //当前焦点(基准)
	GYOBJ first;  //遍历到的第一个可聚焦对象(回卷用)
	GYOBJ next;   //cur 之后紧跟的可聚焦对象(命中即目标)
	uint8 seen;   //是否已越过 cur
} GYfocusWalk;

//前序遍历对象树,填 GYfocusWalk。跳过 Hidden 子树(隐藏对象及其子孙不参与轮转)。
static void focusWalk(GYOBJ obj, GYfocusWalk* w)
{
	if (obj == NULL || (obj->state & GY_STATE_Hidden))
		return;
	if (obj->state & GY_STATE_Focusable)
	{
		if (w->first == NULL)
			w->first = obj;
		if (w->seen && w->next == NULL)
			w->next = obj;      //越过 cur 后遇到的第一个即目标
		if (obj == w->cur)
			w->seen = 1;        //置于赋 next 之后:cur 不会成为自己的 next
	}
	for (GYOBJ c = obj->child_head; c != NULL; c = c->sibling)
		focusWalk(c, w);
}

/**
  * @brief 焦点轮转到"下一个"可聚焦对象(前序序,回卷)。见头文件说明。
  */
void YMGUI_FocusNext(GYCTX ctx)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");

	GYfocusWalk w = { ctx->focus_obj, NULL, NULL, 0 };
	//当前焦点若不在树里(已被摘除/隐藏),当作无焦点从头选
	if (w.cur == NULL || (w.cur->state & GY_STATE_Hidden))
		w.cur = NULL, w.seen = 1;
	focusWalk(ctx->root, &w);
	if (ctx->top_layer != NULL)
		focusWalk(ctx->top_layer, &w);

	GYOBJ target = (w.next != NULL) ? w.next : w.first;  //到末尾回卷到第一个
	if (target != NULL)
		YMGUI_SetFocus(ctx, target);
}

/**
  * @brief 处理一次按键:派发 GY_EVENT_Key 给焦点对象(键值存 ctx->last_key)
  */
void YMGUI_Event_Key(GYCTX ctx, uint32 key)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");

	//先把键交给焦点控件。控件若"消费"了就置 ctx->key_handled=1(如 EditView 把 Tab 变空格)。
	//这样 Tab 是否轮转焦点取决于焦点控件——可编辑多行控件吃掉它做缩进,其余控件放行才切焦点。
	ctx->key_handled = 0;
	if (ctx->focus_obj != NULL)
	{
		ctx->last_key = key;
		sendEvent(ctx->focus_obj, GY_EVENT_Key);
	}

	//Tab 未被焦点控件消费(或无焦点)→ 轮转焦点到下一个可聚焦控件(标准表单行为)
	if (key == GY_KEY_TAB && !ctx->key_handled)
		YMGUI_FocusNext(ctx);
}

/**
  * @brief 处理一次滚轮:记录位置和增量，派给指针所在对象
  */
void YMGUI_Event_Wheel(GYCTX ctx, GYcoord x, GYcoord y, int32 delta_x, int32 delta_y)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	if (ctx == NULL || (delta_x == 0 && delta_y == 0))
		return;
	ctx->point_x = x;
	ctx->point_y = y;
	ctx->wheel_x = delta_x;
	ctx->wheel_y = delta_y;
	sendEvent(YMGUI_HitTest(ctx, x, y), GY_EVENT_Wheel);
}

void YMGUI_Event_Tick(GYCTX ctx, uint32 elapsed_ms)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	if (ctx == NULL || elapsed_ms == 0)
		return;
	ctx->tick_elapsed = elapsed_ms;
	if (ctx->point_pressed && ctx->pressed_obj != NULL)
		sendEvent(ctx->pressed_obj, GY_EVENT_Tick);
}

/**
  * @brief 处理一次双击:命中对象派 GY_EVENT_DoubleClicked。
  *        双击前 SDL 已注入过一次 press+release(第一击),对象已聚焦/进编辑,
  *        故这里只在命中对象上补派双击事件(编辑器据此选词)。
  */
void YMGUI_Event_DoubleClick(GYCTX ctx, GYcoord x, GYcoord y)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	ctx->point_x = x;
	ctx->point_y = y;
	GYOBJ hit = YMGUI_HitTest(ctx, x, y);
	sendEvent(hit, GY_EVENT_DoubleClicked);
}

/**
  * @brief 取消当前指针捕获:恢复对象状态并派 ReleasedOff,不产生 Clicked
  */
void YMGUI_Event_PointerCancel(GYCTX ctx)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	ctx->point_pressed = 0;
	GYOBJ pobj = ctx->pressed_obj;
	if (pobj == NULL)
		return;
	pobj->state &= (uint8)~GY_STATE_Pressed;
	ctx->pressed_obj = NULL;
	sendEvent(pobj, GY_EVENT_ReleasedOff);
}

/**
  * @brief 上下文请求:命中对象派 ContextRequested,不改变焦点/普通指针状态
  */
void YMGUI_Event_ContextRequest(GYCTX ctx, GYcoord x, GYcoord y)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	ctx->point_x = x;
	ctx->point_y = y;
	GYOBJ hit = YMGUI_HitTest(ctx, x, y);
	sendEvent(hit, GY_EVENT_ContextRequested);
}

/**
  * @brief 开始捕获式上下文手势:命中起点对象并派 ContextRequested
  */
void YMGUI_Event_ContextBegin(GYCTX ctx, GYcoord x, GYcoord y)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	if (ctx->context_obj != NULL)
		YMGUI_Event_ContextCancel(ctx);
	ctx->point_x = x;
	ctx->point_y = y;
	GYOBJ hit = YMGUI_HitTest(ctx, x, y);
	ctx->context_obj = hit;
	sendEvent(hit, GY_EVENT_ContextRequested);
}

/**
  * @brief 移动捕获式上下文手势:始终派给起点对象
  */
void YMGUI_Event_ContextMove(GYCTX ctx, GYcoord x, GYcoord y)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	ctx->point_x = x;
	ctx->point_y = y;
	if (ctx->context_obj != NULL)
		sendEvent(ctx->context_obj, GY_EVENT_ContextDragging);
}

/**
  * @brief 正常结束上下文手势:回调前先清捕获,保证回调可安全释放对象
  */
void YMGUI_Event_ContextEnd(GYCTX ctx, GYcoord x, GYcoord y)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	ctx->point_x = x;
	ctx->point_y = y;
	GYOBJ obj = ctx->context_obj;
	ctx->context_obj = NULL;
	sendEvent(obj, GY_EVENT_ContextReleased);
}

/**
  * @brief 异常取消上下文手势:回调前先清捕获,重复调用幂等
  */
void YMGUI_Event_ContextCancel(GYCTX ctx)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	GYOBJ obj = ctx->context_obj;
	ctx->context_obj = NULL;
	sendEvent(obj, GY_EVENT_ContextCancelled);
}

/**
  * @brief 处理一次指针状态,维护 pressed/pressing/clicked 语义
  *        同一位置连续调用即"移动"(SDL 每次鼠标事件都调一次)
  */
void YMGUI_Event_Pointer(GYCTX ctx, GYcoord x, GYcoord y, uint8 pressed)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");

	//记录当前指针位置(控件事件回调里可读 ctx->point_x/y)
	ctx->point_x = x;
	ctx->point_y = y;
	uint8 was_pressed = ctx->point_pressed;
	ctx->point_pressed = pressed;

	GYOBJ hit = YMGUI_HitTest(ctx, x, y);

	if (pressed)
	{
		if (ctx->pressed_obj == NULL)
		{
			//新按下:捕获对象,置状态位,派发 Pressed
			ctx->pressed_obj = hit;
			hit->state |= GY_STATE_Pressed;
			//焦点切换:命中可聚焦对象则获焦,否则清焦点
			if (hit->state & GY_STATE_Focusable)
			{
				YMGUI_SetFocus(ctx, hit);
				//点击=直接进编辑态(指针用户意图就是编辑,光标即现)
				hit->state |= GY_STATE_Editing;
			}
			else
				YMGUI_SetFocus(ctx, NULL);
			sendEvent(hit, GY_EVENT_Pressed);
		}
		else if (was_pressed)
		{
			//按住移动:派发 Pressing 给已捕获对象(即使指针移出其范围,拖动仍归它)
			sendEvent(ctx->pressed_obj, GY_EVENT_Pressing);
		}
	}
	else
	{
		//抬起:清 pressed 状态
		GYOBJ pobj = ctx->pressed_obj;
		if (pobj != NULL)
		{
			pobj->state &= (uint8)~GY_STATE_Pressed;
			if (pobj == hit)
			{
				//按下和抬起在同一对象 → 点击
				sendEvent(pobj, GY_EVENT_Released);
				sendEvent(pobj, GY_EVENT_Clicked);
			}
			else
			{
				sendEvent(pobj, GY_EVENT_ReleasedOff);
			}
			ctx->pressed_obj = NULL;
		}
	}
}
