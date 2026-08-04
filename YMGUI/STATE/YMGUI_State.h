#ifndef YMGUI_STATE_H
#define YMGUI_STATE_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 状态/数据绑定核心(数据驱动:UI = f(state))
//   Subject —— 带类型标量的"可观察值",是唯一真相。可静态声明(零堆)。
//   Observer —— 挂在 Subject 链上,把值 apply 到某控件。控件被 free 时自动摘链。
//   路径不进运行时:绑定用符号直连(见 YMGUI_Bind.h 的 *_Bind),运行时全是指针。
//
//   写回规则:改状态必须走 SetXxx(触发 compare-and-skip → 通知 → 标脏 → 重绘)。
//   裸 union 原地改内容(如字符串 buffer)不会通知,需显式 YMGUI_State_Touch。
//===========================================================================

//值类型标签(标量优先)
typedef enum
{
	GY_VAL_None = 0,
	GY_VAL_Int,   //int32
	GY_VAL_Bool,  //0/1(存整数槽)
	GY_VAL_Fixed, //GYvalue 16.16(存整数槽)
	GY_VAL_Str,   //const char*(存地址,不拷贝;指向须比绑定活得久)
}GYvalType;

//带标签标量值:整数类共用 i 槽,字符串存地址
typedef struct
{
	GYvalType type;
	union
	{
		int32       i;
		const char* s;
	}u;
}GYval;

struct GYsubject;
typedef struct GYsubject GYsubject;
typedef struct GYsubject* GYSUBJECT;

//subject→控件 的应用回调(把当前值画进控件)
typedef void (*GYobserver_apply_cb)(GYOBJ target, const GYval* v);

//subject→app逻辑 的通知回调(状态变了,业务侧响应;不绑定任何控件)
typedef void (*GYsubject_observer_cb)(GYSUBJECT s, const GYval* v, void* user_data);

//观察者节点:挂在 subject 链上。
//两类:控件观察者(target!=NULL,走 apply);app观察者(target==NULL,走 notify)。
//控件的 bind_data 指向自己这个节点;app观察者的句柄由调用方自己持有。
typedef struct GYobserver
{
	struct GYsubject*     subject;  //所属 subject(摘链用)
	GYOBJ                 target;   //目标控件(app观察者为 NULL)
	GYobserver_apply_cb   apply;    //subject→控件
	GYsubject_observer_cb notify;   //subject→app逻辑
	void*                 user_data;//app观察者的透传数据
	struct GYobserver*    next;     //subject 链下一个
}GYobserver;

//可观察值:当前权威值 + 观察者链 + 重入保护
struct GYsubject
{
	GYval      val;      //当前权威值
	GYobserver* obs_head;//观察者链头
	uint8      notifying;//通知重入保护
};

//---- 静态声明(零堆,可放全局/静态) ----
#define GY_SUBJECT_INT(name, v)   GYsubject name = { { GY_VAL_Int,   { .i = (int32)(v) } }, NULL, 0 }
#define GY_SUBJECT_BOOL(name, v)  GYsubject name = { { GY_VAL_Bool,  { .i = (v) ? 1 : 0 } }, NULL, 0 }
#define GY_SUBJECT_FIXED(name, v) GYsubject name = { { GY_VAL_Fixed, { .i = (int32)(v) } }, NULL, 0 }
#define GY_SUBJECT_STR(name, v)   GYsubject name = { { GY_VAL_Str,   { .s = (v) } }, NULL, 0 }

//---- 运行时初始化(动态申请的 subject 用) ----
void YMGUI_State_InitInt(GYSUBJECT s, int32 v);
void YMGUI_State_InitBool(GYSUBJECT s, uint8 v);
void YMGUI_State_InitStr(GYSUBJECT s, const char* v);

//---- 写(compare-and-skip + 通知观察者) ----
void YMGUI_State_SetInt(GYSUBJECT s, int32 v);
void YMGUI_State_SetBool(GYSUBJECT s, uint8 v);
void YMGUI_State_SetStr(GYSUBJECT s, const char* v);//只换指向,不拷贝
void YMGUI_State_Touch(GYSUBJECT s);                //原地改内容后强制通知

//---- 读 ----
int32       YMGUI_State_GetInt(GYSUBJECT s);
uint8       YMGUI_State_GetBool(GYSUBJECT s);
const char* YMGUI_State_GetStr(GYSUBJECT s);

//---- 绑定核心(供各控件 *_Bind 复用;一般不直接调) ----
//把控件挂到 subject:分配 observer、存进 widget->bind_data、立即 apply 一次当前值。
GYobserver* YMGUI_Bind_Attach(GYOBJ widget, GYSUBJECT s, GYobserver_apply_cb apply);
//从 subject 链摘掉控件的 observer 并释放(YMGUI_Free_ObjFree 自动调,幂等)。
void        YMGUI_Bind_Unlink(GYOBJ widget);

//---- app 观察者:业务逻辑直接订阅状态(不绑控件,与控件绑定并存) ----
//状态每次变化(不论来自用户交互 writeback 还是后端 SetXxx)都会回调 cb。
//返回的句柄由调用方持有,用于 RemoveObserver;不会立即触发一次(只在后续变化时回调)。
GYobserver* YMGUI_State_AddObserver(GYSUBJECT s, GYsubject_observer_cb cb, void* user_data);
//摘掉一个 app 观察者并释放(须传 AddObserver 返回的句柄;幂等)。
void        YMGUI_State_RemoveObserver(GYobserver* o);

#endif // !YMGUI_STATE_H
