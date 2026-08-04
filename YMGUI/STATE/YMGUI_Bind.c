#include "YMGUI_Bind.h"
#include "YMGUI_Label.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Switch.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_Bar.h"
#include "YMGUI_ArcWidget.h"
#include "YMGUI_Meter.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Bind.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 控件级数据绑定适配器。把 subject 的值 apply 进具体控件(label/slider),
  *	              可写控件(slider)另接写回:交互→SetXxx 写 subject→通知其余观察者。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.断环:slider 写回走 State_SetInt(compare-and-skip);其 apply 走 Slider_SetValue(不触发 changed)
  * 2.无 FPU:整数→十进制串用本地 fmtInt,不引 sprintf 的浮点负担
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief int32→十进制字符串(含负号),写入 buf。裸机友好,不依赖 sprintf。
  */
static void fmtInt(int32 v, char* buf, int cap)
{
	if (cap <= 0) return;
	char tmp[12];
	int n = 0;
	uint32 u = (v < 0) ? (uint32)(-(int64)v) : (uint32)v;
	do {
		tmp[n++] = (char)('0' + (u % 10));
		u /= 10;
	} while (u != 0 && n < (int)sizeof(tmp));
	int i = 0;
	if (v < 0 && i < cap - 1)
		buf[i++] = '-';
	while (n > 0 && i < cap - 1)
		buf[i++] = tmp[--n];
	buf[i] = '\0';
}

//---- 标签(单向 subject→文本) ----

static void labelApply(GYOBJ label, const GYval* v)
{
	char buf[16];
	switch (v->type)
	{
	case GY_VAL_Str:
		YMGUI_Label_SetText(label, (v->u.s != NULL) ? v->u.s : "");
		break;
	case GY_VAL_Bool:
		YMGUI_Label_SetText(label, v->u.i ? "1" : "0");
		break;
	case GY_VAL_Fixed:
		fmtInt(GY_INT(v->u.i), buf, sizeof(buf));//定点→整数部分(标量优先,先不显小数)
		YMGUI_Label_SetText(label, buf);
		break;
	case GY_VAL_Int:
	default:
		fmtInt(v->u.i, buf, sizeof(buf));
		YMGUI_Label_SetText(label, buf);
		break;
	}
}

void YMGUI_Label_Bind(GYOBJ label, GYSUBJECT s)
{
	gy_assert(label && s);
	gy_log_explain((label == NULL) || (s == NULL), GY_LOG_PtrI, "标签绑定参数不全");
	YMGUI_Bind_Attach(label, s, labelApply);
}

//---- 滑块(双向 subject↔value) ----

static void sliderApply(GYOBJ slider, const GYval* v)
{
	//SetValue 只标脏、不触发 changed,故不会反向再写 subject → 天然断环
	YMGUI_Slider_SetValue(slider, v->u.i);
}

/**
  * @brief 滑块交互写回:值变时写进绑定的 subject(compare-and-skip 拦重复)
  */
static void sliderWriteback(GYOBJ slider, int32 value)
{
	if (slider == NULL || slider->bind_data == NULL)
		return;
	GYobserver* o = (GYobserver*)slider->bind_data;
	if (o->subject != NULL)
		YMGUI_State_SetInt(o->subject, value);
}

void YMGUI_Slider_Bind(GYOBJ slider, GYSUBJECT s)
{
	gy_assert(slider && s);
	gy_log_explain((slider == NULL) || (s == NULL), GY_LOG_PtrI, "滑块绑定参数不全");
	YMGUI_Bind_Attach(slider, s, sliderApply);
	YMGUI_Slider_SetChanged(slider, sliderWriteback);//交互→写回 subject
}

//---- 开关(双向 subject↔on) ----

static void switchApply(GYOBJ sw, const GYval* v)
{
	YMGUI_Switch_SetOn(sw, v->u.i ? 1 : 0);//SetOn 不触发 changed → 断环
}

static void switchWriteback(GYOBJ sw, uint8 on)
{
	if (sw == NULL || sw->bind_data == NULL)
		return;
	GYobserver* o = (GYobserver*)sw->bind_data;
	if (o->subject != NULL)
		YMGUI_State_SetBool(o->subject, on);
}

void YMGUI_Switch_Bind(GYOBJ sw, GYSUBJECT s)
{
	gy_assert(sw && s);
	gy_log_explain((sw == NULL) || (s == NULL), GY_LOG_PtrI, "开关绑定参数不全");
	YMGUI_Bind_Attach(sw, s, switchApply);
	YMGUI_Switch_SetChanged(sw, switchWriteback);
}

//---- 复选框(双向 subject↔checked) ----

static void checkboxApply(GYOBJ cb, const GYval* v)
{
	YMGUI_Checkbox_SetChecked(cb, v->u.i ? 1 : 0);//SetChecked 不触发 changed → 断环
}

static void checkboxWriteback(GYOBJ cb, uint8 checked)
{
	if (cb == NULL || cb->bind_data == NULL)
		return;
	GYobserver* o = (GYobserver*)cb->bind_data;
	if (o->subject != NULL)
		YMGUI_State_SetBool(o->subject, checked);
}

void YMGUI_Checkbox_Bind(GYOBJ cb, GYSUBJECT s)
{
	gy_assert(cb && s);
	gy_log_explain((cb == NULL) || (s == NULL), GY_LOG_PtrI, "复选框绑定参数不全");
	YMGUI_Bind_Attach(cb, s, checkboxApply);
	YMGUI_Checkbox_SetChanged(cb, checkboxWriteback);
}

//---- 进度条(单向 subject→value) ----

static void barApply(GYOBJ bar, const GYval* v)
{
	YMGUI_Bar_SetValue(bar, v->u.i);
}

void YMGUI_Bar_Bind(GYOBJ bar, GYSUBJECT s)
{
	gy_assert(bar && s);
	gy_log_explain((bar == NULL) || (s == NULL), GY_LOG_PtrI, "进度条绑定参数不全");
	YMGUI_Bind_Attach(bar, s, barApply);//只显示,无写回
}

//---- 环形进度(单向 subject→value) ----

static void arcApply(GYOBJ arc, const GYval* v)
{
	YMGUI_Arc_SetValue(arc, v->u.i);//SetValue 钳制+标脏,无写回
}

void YMGUI_Arc_Bind(GYOBJ arc, GYSUBJECT s)
{
	gy_assert(arc && s);
	gy_log_explain((arc == NULL) || (s == NULL), GY_LOG_PtrI, "环形进度绑定参数不全");
	YMGUI_Bind_Attach(arc, s, arcApply);//只显示,无写回
}

//---- 仪表盘(单向 subject→value) ----

static void meterApply(GYOBJ meter, const GYval* v)
{
	YMGUI_Meter_SetValue(meter, v->u.i);//SetValue 钳制+标脏,无写回
}

void YMGUI_Meter_Bind(GYOBJ meter, GYSUBJECT s)
{
	gy_assert(meter && s);
	gy_log_explain((meter == NULL) || (s == NULL), GY_LOG_PtrI, "仪表盘绑定参数不全");
	YMGUI_Bind_Attach(meter, s, meterApply);//只显示,无写回
}

//---- 文本框(双向 subject↔text) ----

static void textinputApply(GYOBJ ti, const GYval* v)
{
	const char* s = (v->type == GY_VAL_Str && v->u.s != NULL) ? v->u.s : "";
	//自回声保护:subject 已指向本框内部缓冲(写回造成)时,SetText 会自拷+光标跳末尾。
	//此时内容本就同步,直接跳过,保住编辑中的光标位置。
	if (s == YMGUI_TextInput_GetText(ti))
		return;
	YMGUI_TextInput_SetText(ti, s);
}

static void textinputWriteback(GYOBJ ti, const char* text)
{
	if (ti == NULL || ti->bind_data == NULL)
		return;
	GYobserver* o = (GYobserver*)ti->bind_data;
	if (o->subject == NULL)
		return;
	//文本框缓冲地址稳定:首次写回换指向触发通知,之后地址不变,SetStr 会 compare-and-skip
	//跳过——故内容原地变时用 Touch 强制通知其余观察者(如绑同 subject 的 label)。
	if (o->subject->val.u.s == text)
		YMGUI_State_Touch(o->subject);
	else
		YMGUI_State_SetStr(o->subject, text);
}

void YMGUI_TextInput_Bind(GYOBJ ti, GYSUBJECT s)
{
	gy_assert(ti && s);
	gy_log_explain((ti == NULL) || (s == NULL), GY_LOG_PtrI, "文本框绑定参数不全");
	YMGUI_Bind_Attach(ti, s, textinputApply);
	YMGUI_TextInput_SetChanged(ti, textinputWriteback);
}
