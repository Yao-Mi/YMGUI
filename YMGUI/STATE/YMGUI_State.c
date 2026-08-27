#include "YMGUI_State.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_State.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 状态/数据绑定核心。Subject 持权威标量值 + 观察者链;写走 compare-and-skip 后通知,
  *	              apply 回调把值刷进控件。控件 free 时经 YMGUI_Bind_Unlink 自动摘链,杜绝野指针。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.断环:apply 里调控件 SetXxx 若又写回 subject,SetXxx 的 compare-and-skip + 这里的值比较双重拦截,不会无限递归
  * 2.notifying 位防重入(一次通知过程中某观察者又触发写)
  * 3.observer 走 GY_malloc0(小/快);subject 本体一般由调用方静态声明,零堆
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief 通知所有观察者:把当前值 apply 进各控件
  */
static void notifyAll(GYSUBJECT s)
{
	if (s->notifying)
		return;//重入保护:通知中触发的再次通知直接跳过(值已是最新)
	s->notifying = 1;
	GYobserver* o = s->obs_head;
	while (o != NULL)
	{
		GYobserver* next = o->next;//回调里可能自摘,先存 next
		if (o->target != NULL)
		{
			if (o->apply != NULL)
				o->apply(o->target, &s->val);//控件观察者
		}
		else if (o->notify != NULL)
		{
			o->notify(s, &s->val, o->user_data);//app 观察者
		}
		o = next;
	}
	s->notifying = 0;
}

//---- 运行时初始化 ----

void YMGUI_State_InitInt(GYSUBJECT s, int32 v)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	if (s == NULL) return;
	s->val.type = GY_VAL_Int;
	s->val.u.i = v;
	s->obs_head = NULL;
	s->notifying = 0;
}

void YMGUI_State_InitBool(GYSUBJECT s, uint8 v)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	if (s == NULL) return;
	s->val.type = GY_VAL_Bool;
	s->val.u.i = v ? 1 : 0;
	s->obs_head = NULL;
	s->notifying = 0;
}

void YMGUI_State_InitStr(GYSUBJECT s, const char* v)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	if (s == NULL) return;
	s->val.type = GY_VAL_Str;
	s->val.u.s = v;
	s->obs_head = NULL;
	s->notifying = 0;
}

//---- 写(compare-and-skip + 通知) ----

void YMGUI_State_SetInt(GYSUBJECT s, int32 v)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	if (s == NULL) return;
	if (s->val.type == GY_VAL_Int && s->val.u.i == v)
		return;//值未变,断环
	s->val.type = GY_VAL_Int;
	s->val.u.i = v;
	notifyAll(s);
}

void YMGUI_State_SetBool(GYSUBJECT s, uint8 v)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	if (s == NULL) return;
	int32 nv = v ? 1 : 0;
	if (s->val.type == GY_VAL_Bool && s->val.u.i == nv)
		return;
	s->val.type = GY_VAL_Bool;
	s->val.u.i = nv;
	notifyAll(s);
}

void YMGUI_State_SetStr(GYSUBJECT s, const char* v)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	if (s == NULL) return;
	if (s->val.type == GY_VAL_Str && s->val.u.s == v)
		return;//同一指针,断环(原地改内容请用 Touch)
	s->val.type = GY_VAL_Str;
	s->val.u.s = v;
	notifyAll(s);
}

void YMGUI_State_Touch(GYSUBJECT s)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	if (s == NULL) return;
	notifyAll(s);//不比较,强制刷(字符串 buffer 原地改后用)
}

//---- 读 ----

int32 YMGUI_State_GetInt(GYSUBJECT s)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	return (s != NULL) ? s->val.u.i : 0;
}

uint8 YMGUI_State_GetBool(GYSUBJECT s)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	return (s != NULL && s->val.u.i) ? 1 : 0;
}

const char* YMGUI_State_GetStr(GYSUBJECT s)
{
	gy_assert(s);
	gy_log_explain(s == NULL, GY_LOG_PtrI, "subject 不存在");
	return (s != NULL) ? s->val.u.s : NULL;
}

//---- 绑定核心 ----

/**
  * @brief 把控件挂到 subject:分配 observer、存 bind_data、立即 apply 当前值一次
  */
GYobserver* YMGUI_Bind_Attach(GYOBJ widget, GYSUBJECT s, GYobserver_apply_cb apply)
{
	gy_assert(widget && s && apply);
	gy_log_explain((widget == NULL) || (s == NULL) || (apply == NULL), GY_LOG_PtrI, "绑定参数不全");
	if (widget == NULL || s == NULL || apply == NULL)
		return NULL;

	//一个控件重复绑定:先摘旧的,避免泄漏与双挂
	if (widget->bind_data != NULL)
		YMGUI_Bind_Unlink(widget);

	GYobserver* o = (GYobserver*)GY_malloc0(sizeof(GYobserver));
	gy_assert(o);
	gy_log_explain(o == NULL, GY_LOG_Mem0, "观察者内存申请失败");
	if (o == NULL)
		return NULL;
	GY_memset(o, 0, sizeof(GYobserver));
	o->subject = s;
	o->target = widget;
	o->apply = apply;
	o->next = s->obs_head;//头插
	s->obs_head = o;

	widget->bind_data = o;
	o->apply(widget, &s->val);//绑定即同步一次当前状态
	return o;
}

/**
  * @brief 从 subject 链摘掉控件的 observer 并释放(幂等)
  */
void YMGUI_Bind_Unlink(GYOBJ widget)
{
	if (widget == NULL || widget->bind_data == NULL)
		return;
	GYobserver* self = (GYobserver*)widget->bind_data;
	GYSUBJECT s = self->subject;
	if (s != NULL)
	{
		if (s->obs_head == self)
		{
			s->obs_head = self->next;
		}
		else
		{
			GYobserver* p = s->obs_head;
			while (p != NULL && p->next != self)
				p = p->next;
			if (p != NULL)
				p->next = self->next;
		}
	}
	widget->bind_data = NULL;
	GY_free0(self);
}

//---- app 观察者 ----

/**
  * @brief 让业务逻辑直接订阅 subject:分配 observer(target=NULL,走 notify 分支)、头插入链。
  *        不立即触发——只在后续状态变化时回调。句柄由调用方持有用于摘链。
  */
GYobserver* YMGUI_State_AddObserver(GYSUBJECT s, GYsubject_observer_cb cb, void* user_data)
{
	gy_assert(s && cb);
	gy_log_explain((s == NULL) || (cb == NULL), GY_LOG_PtrI, "观察参数不全");
	if (s == NULL || cb == NULL)
		return NULL;

	GYobserver* o = (GYobserver*)GY_malloc0(sizeof(GYobserver));
	gy_assert(o);
	gy_log_explain(o == NULL, GY_LOG_Mem0, "观察者内存申请失败");
	if (o == NULL)
		return NULL;
	GY_memset(o, 0, sizeof(GYobserver));
	o->subject = s;
	o->target = NULL;      //app 观察者标志
	o->apply = NULL;
	o->notify = cb;
	o->user_data = user_data;
	o->next = s->obs_head; //头插
	s->obs_head = o;
	return o;
}

/**
  * @brief 摘掉一个 app 观察者并释放(须传 AddObserver 返回句柄;幂等)
  */
void YMGUI_State_RemoveObserver(GYobserver* o)
{
	if (o == NULL)
		return;
	GYSUBJECT s = o->subject;
	if (s != NULL)
	{
		if (s->obs_head == o)
		{
			s->obs_head = o->next;
		}
		else
		{
			GYobserver* p = s->obs_head;
			while (p != NULL && p->next != o)
				p = p->next;
			if (p != NULL)
				p->next = o->next;
		}
	}
	GY_free0(o);
}
