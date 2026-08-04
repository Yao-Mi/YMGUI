#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Obj.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 对象树生命周期。Creat/Free 成对,Free 容器递归级联释放子节点(区别于 YMCV 扁平模型)
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.对象头走 GY_malloc0(小/快);无大数据,故不涉及 malloc1
  * 2.坐标 area 相对父;屏幕绝对坐标由 GetAbsArea 累加父原点得到
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief 默认绘制回调:填背景色(不透明)
  */
static void baseDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	extern void YMGUI_Draw_Fill(GYSURFACE, const GYrect*, GYcolor, GYopa);
	YMGUI_Draw_Fill(s, abs, obj->bg_color, GY_OPA_COVER);
}

/**
  * @brief 把子对象挂到父的子链表尾
  */
static void appendChild(GYOBJ parent, GYOBJ child)
{
	if (parent->child_head == NULL)
	{
		parent->child_head = child;
		return;
	}
	GYOBJ p = parent->child_head;
	while (p->sibling != NULL)
		p = p->sibling;
	p->sibling = child;
}

/**
  * @brief 从父的子链表里摘除某子对象
  */
static void detachChild(GYOBJ parent, GYOBJ child)
{
	if (parent == NULL || parent->child_head == NULL)
		return;
	if (parent->child_head == child)
	{
		parent->child_head = child->sibling;
		return;
	}
	GYOBJ p = parent->child_head;
	while (p->sibling != NULL && p->sibling != child)
		p = p->sibling;
	if (p->sibling == child)
		p->sibling = child->sibling;
}

/**
  * @brief 创建上下文 + 根对象(根铺满全屏)
  */
GYCTX YMGUI_Creat_Ctx_Creat(void* disp, GYcoord w, GYcoord h)
{
	GYCTX ctx = (GYCTX)GY_malloc0(sizeof(GYctx));
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_Mem0, "上下文内存申请失败");
	if (ctx == NULL)
		return NULL;//gy_assert 不中止,须显式返回

	ctx->disp = disp;
	ctx->pressed_obj = NULL;
	ctx->focus_obj = NULL;
	ctx->point_x = 0;
	ctx->point_y = 0;
	ctx->point_pressed = 0;
	ctx->last_key = 0;
	ctx->inv_cnt = 0;

	//根对象
	GYOBJ root = (GYOBJ)GY_malloc0(sizeof(GYobj));
	gy_assert(root);
	gy_log_explain(root == NULL, GY_LOG_Mem0, "根对象内存申请失败");
	if (root == NULL)
	{
		GY_free0(ctx);//根申请失败,回收已申请的 ctx
		return NULL;
	}
	GY_memset(root, 0, sizeof(GYobj));
	root->type = GY_OBJ_Base;
	root->area = (GYrect){0, 0, w, h};
	root->state = GY_STATE_Default;
	root->bg_color = GY_ARGB(0xFF, 0x00, 0x00, 0x00);
	root->parent = NULL;
	root->child_head = NULL;
	root->sibling = NULL;
	root->ctx = ctx;
	root->draw_cb = baseDrawCb;
	root->event_cb = NULL;

	ctx->root = root;

	//顶层:全屏透明容器。无 draw_cb(不填任何像素,故 root 内容透过它显示),
	//parent=NULL(与 root 同级,GetAbsArea 从 (0,0) 起算全屏)。弹出层挂它。
	GYOBJ top = (GYOBJ)GY_malloc0(sizeof(GYobj));
	gy_assert(top);
	gy_log_explain(top == NULL, GY_LOG_Mem0, "顶层对象内存申请失败");
	if (top == NULL)
	{
		GY_free0(root);//顶层申请失败,回收 root 与 ctx
		GY_free0(ctx);
		return NULL;
	}
	GY_memset(top, 0, sizeof(GYobj));
	top->type = GY_OBJ_Base;
	top->area = (GYrect){0, 0, w, h};
	top->state = GY_STATE_Default;
	top->parent = NULL;
	top->child_head = NULL;
	top->sibling = NULL;
	top->ctx = ctx;
	top->draw_cb = NULL;//透明:不画自己
	top->event_cb = NULL;

	ctx->top_layer = top;
	return ctx;
}

/**
  * @brief 创建基础对象,挂到 parent 子链表尾
  */
GYOBJ YMGUI_Creat_Obj_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	gy_assert(parent);
	gy_log_explain(parent == NULL, GY_LOG_PtrI, "父对象不存在");

	GYOBJ obj = (GYOBJ)GY_malloc0(sizeof(GYobj));
	gy_assert(obj);
	gy_log_explain(obj == NULL, GY_LOG_Mem0, "对象内存申请失败");
	if (obj == NULL)
		return NULL;//gy_assert 不中止,须显式返回
	GY_memset(obj, 0, sizeof(GYobj));

	obj->type = GY_OBJ_Base;
	obj->area = (GYrect){x, y, w, h};
	obj->state = GY_STATE_Default;
	obj->bg_color = GY_ARGB(0xFF, 0x80, 0x80, 0x80);
	obj->parent = parent;
	obj->child_head = NULL;
	obj->sibling = NULL;
	obj->ctx = parent->ctx;
	obj->draw_cb = baseDrawCb;
	obj->event_cb = NULL;

	appendChild(parent, obj);
	YMGUI_Obj_Invalidate(obj);//新对象需要绘制
	return obj;
}

/**
  * @brief 释放对象:先递归级联释放所有子节点,再从父链摘除,最后释放自己
  */
void YMGUI_Free_ObjFree(GYOBJ obj)
{
	if (obj == NULL)
		return;

	//1.递归级联释放子树(边遍历边释放,先存 sibling)
	GYOBJ c = obj->child_head;
	while (c != NULL)
	{
		GYOBJ next = c->sibling;
		YMGUI_Free_ObjFree(c);
		c = next;
	}
	obj->child_head = NULL;

	//2.释放前把占用区域标脏(需要重绘露出的底层)
	YMGUI_Obj_Invalidate(obj);

	//3.清理上下文里对本对象的引用
	if (obj->ctx != NULL)
	{
		if (obj->ctx->pressed_obj == obj) obj->ctx->pressed_obj = NULL;
		if (obj->ctx->focus_obj == obj)   obj->ctx->focus_obj = NULL;
	}

	//4.从父链摘除
	detachChild(obj->parent, obj);

	//4.5 若绑定了状态,从 subject 观察者链摘掉本控件(防野指针)。
	//    前置声明避免 OPOBJ 头依赖 STATE;实现见 STATE/YMGUI_State.c,对未绑定对象幂等。
	if (obj->bind_data != NULL)
	{
		extern void YMGUI_Bind_Unlink(GYOBJ widget);
		YMGUI_Bind_Unlink(obj);
	}

	//5.让控件清理自己的 user_data
	if (obj->free_cb != NULL)
		obj->free_cb(obj);

	//6.释放自己
	GY_free0(obj);
}

/**
  * @brief 释放上下文:级联释放整棵树 + 根 + ctx
  */
void YMGUI_Free_CtxFree(GYCTX ctx)
{
	if (ctx == NULL)
		return;
	//释放顺序关键:先释放根子树,再释放顶层子树。
	//  弹出层(Dropdown 菜单/遮罩等)挂在 top_layer,但其"所有者"控件在根子树里,
	//  控件的 free_cb(如 ddFreeCb→teardownPopup)负责释放并从 top_layer 摘除自己的弹出层。
	//  若先释放 top_layer 子树,弹出层被释放后所有者控件仍持有其指针,teardown 时二次释放(double free)。
    //  故先放根:所有者随根子树释放时顺带拆掉各自弹出层并从 top_layer 摘链;剩余无主的再由下方兜底释放。
	if (ctx->root != NULL)
	{
		//先释放根的所有子(根本身单独释放,因它 parent=NULL 不走 detach)
		GYOBJ c = ctx->root->child_head;
		while (c != NULL)
		{
			GYOBJ next = c->sibling;
			YMGUI_Free_ObjFree(c);
			c = next;
		}
		ctx->root->child_head = NULL;
	}
	//再释放顶层子树(此时多已被各所有者 teardown 摘除;剩余无主的兜底释放),及顶层自身
	if (ctx->top_layer != NULL)
	{
		GYOBJ c = ctx->top_layer->child_head;
		while (c != NULL)
		{
			GYOBJ next = c->sibling;
			YMGUI_Free_ObjFree(c);
			c = next;
		}
		GY_free0(ctx->top_layer);
		ctx->top_layer = NULL;
	}
	if (ctx->root != NULL)
		GY_free0(ctx->root);
	GY_free0(ctx);
}

/**
  * @brief 算对象屏幕绝对矩形:沿 parent 链累加原点偏移
  */
void YMGUI_Obj_GetAbsArea(GYOBJ obj, GYRECT abs)
{
	GYcoord ax = 0, ay = 0;
	GYOBJ p = obj;
	while (p != NULL)
	{
		ax += p->area.x;
		ay += p->area.y;
		//父的滚动偏移作用于其所有子孙(含 p),故减去父的 scroll
		if (p->parent != NULL)
		{
			ax -= p->parent->scroll_x;
			ay -= p->parent->scroll_y;
		}
		p = p->parent;
	}
	abs->x = ax;
	abs->y = ay;
	abs->w = obj->area.w;
	abs->h = obj->area.h;
}

/**
  * @brief 设背景色并标脏
  */
void YMGUI_Obj_SetBgColor(GYOBJ obj, GYcolor color)
{
	gy_assert(obj);
	gy_log_explain(obj == NULL, GY_LOG_PtrI, "对象不存在");
	obj->bg_color = color;
	YMGUI_Obj_Invalidate(obj);
}

/**
  * @brief 取顶层容器(弹出层挂它;跨子树置顶、不被父 ClipChildren 裁剪)
  */
GYOBJ YMGUI_Ctx_GetTopLayer(GYCTX ctx)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	return (ctx != NULL) ? ctx->top_layer : NULL;
}

/**
  * @brief 显示/隐藏对象:切换 Hidden 位并标脏
  *        注意:标脏须在改位前后正确排序——Invalidate 对 Hidden 对象直接返回,
  *        故隐藏时先标脏(此时还可见)再置位;显示时先清位再标脏。
  *        本函数只标对象自身矩形;若对象含超出自身范围的子(菜单一般不会),调用方另行标脏。
  */
void YMGUI_Obj_SetHidden(GYOBJ obj, uint8 hidden)
{
	gy_assert(obj);
	gy_log_explain(obj == NULL, GY_LOG_PtrI, "对象不存在");
	if (obj == NULL)
		return;
	uint8 now_hidden = (obj->state & GY_STATE_Hidden) ? 1 : 0;
	if (now_hidden == (hidden ? 1 : 0))
		return;//状态未变
	if (hidden)
	{
		YMGUI_Obj_Invalidate(obj);//隐藏前标脏(重绘露出的底层)——此刻仍可见
		obj->state |= GY_STATE_Hidden;
	}
	else
	{
		obj->state &= (uint8)~GY_STATE_Hidden;
		YMGUI_Obj_Invalidate(obj);//显示后标脏——已可见方能生效
	}
}
