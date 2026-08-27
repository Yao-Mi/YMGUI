#include "YMGUI_Slider.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Slider.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 水平滑块。按下/拖动(GY_EVENT_Pressing)时按 ctx->point_x 换算值,更新填充段与拖柄
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.整数运算(无 FPU):value = min + (px - x) * (max-min) / w
  * 2.拖动用 core 的 Pressing 事件 + ctx->point_x(见 GYctx 交互状态)
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	int32            min, max, value;
	GYsld_changed_cb changed;
}GYsld_data;

/**
  * @brief 值→拖柄左边缘 x 偏移(相对控件左边,整数运算)
  */
static GYcoord valueToKnobDx(GYsld_data* d, GYcoord w, GYcoord knob_w)
{
	int32 span = d->max - d->min;
	if (span <= 0)
		return 0;
	GYcoord travel = w - knob_w;//拖柄可移动范围
	if (travel < 0)
		travel = 0;
	return (GYcoord)((int64)(d->value - d->min) * travel / span);
}

/**
  * @brief 绘制:轨道 + 已填充段(左) + 拖柄
  */
static void sldDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYsld_data* d = (GYsld_data*)obj->user_data;
	GYcoord knob_w = abs->h;//方形拖柄
	GYcoord dx = valueToKnobDx(d, abs->w, knob_w);

	//轨道底(纵向居中,高度 = 控件高的一半)
	GYcoord th = abs->h / 2;
	GYcoord ty = abs->y + (abs->h - th) / 2;
	GYrect track = {abs->x, ty, abs->w, th};
	YMGUI_Draw_Fill(s, &track, GY_ARGB(0xFF, 0x50, 0x50, 0x58), GY_OPA_COVER);
	//已填充段(到拖柄中心)
	GYrect fill = {abs->x, ty, dx + knob_w / 2, th};
	YMGUI_Draw_Fill(s, &fill, GY_ARGB(0xFF, 0x30, 0x90, 0xE0), GY_OPA_COVER);
	//拖柄
	GYrect knob = {abs->x + dx, abs->y, knob_w, abs->h};
	GYcolor kc = (obj->state & GY_STATE_Pressed) ? GY_ARGB(0xFF, 0xD0, 0xD0, 0xD0) : GY_ARGB(0xFF, 0xF8, 0xF8, 0xF8);
	YMGUI_Draw_Fill(s, &knob, kc, GY_OPA_COVER);
}

/**
  * @brief 按指针屏幕 x 更新值(钳制),变化则标脏+回调
  */
static void updateFromPointer(GYOBJ obj, GYcoord px)
{
	GYsld_data* d = (GYsld_data*)obj->user_data;
	GYrect abs;
	YMGUI_Obj_GetAbsArea(obj, &abs);
	GYcoord knob_w = abs.h;
	GYcoord travel = abs.w - knob_w;
	if (travel <= 0)
		return;
	//指针相对拖柄可移动区的位置(以拖柄中心对齐指针)
	int32 rel = (int32)px - abs.x - knob_w / 2;
	rel = GYLimitMaxMin(0, rel, travel);
	int32 span = d->max - d->min;
	int32 nv = d->min + (int32)((int64)rel * span / travel);
	nv = GYLimitMaxMin(d->min, nv, d->max);
	if (nv != d->value)
	{
		d->value = nv;
		YMGUI_Obj_Invalidate(obj);
		if (d->changed != NULL)
			d->changed(obj, nv);
	}
}

/**
  * @brief 事件:按下 & 拖动都按指针 x 改值
  */
static void sldEventCb(GYOBJ obj, GYEvent e)
{
	if (e == GY_EVENT_Pressed || e == GY_EVENT_Pressing)
	{
		//从上下文取当前指针位置
		GYcoord px = obj->ctx->point_x;
		updateFromPointer(obj, px);
		if (e == GY_EVENT_Pressed)
			YMGUI_Obj_Invalidate(obj);//按下换拖柄色
	}
	else if (e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff)
	{
		YMGUI_Obj_Invalidate(obj);//抬起恢复拖柄色
	}
}

static void sldFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建滑块
  */
GYOBJ YMGUI_Creat_Slider_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ sld = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(sld);
	if (sld == NULL)
		return NULL;
	GYsld_data* d = (GYsld_data*)GY_malloc0(sizeof(GYsld_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "滑块数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(sld); return NULL; }
	GY_memset(d, 0, sizeof(GYsld_data));
	d->min = 0;
	d->max = 100;
	d->value = 0;
	d->changed = NULL;

	sld->type = GY_OBJ_Base;
	sld->user_data = d;
	sld->draw_cb = sldDrawCb;
	sld->event_cb = sldEventCb;
	sld->free_cb = sldFreeCb;
	YMGUI_Obj_Invalidate(sld);
	return sld;
}

/**
  * @brief 设范围
  */
void YMGUI_Slider_SetRange(GYOBJ sld, int32 min, int32 max)
{
	gy_assert(sld && sld->user_data);
	gy_log_explain((sld == NULL) || (sld->user_data == NULL), GY_LOG_PtrI, "滑块或数据不存在");
	GYsld_data* d = (GYsld_data*)sld->user_data;
	d->min = min;
	d->max = (max > min) ? max : min + 1;
	d->value = GYLimitMaxMin(d->min, d->value, d->max);
	YMGUI_Obj_Invalidate(sld);
}

/**
  * @brief 设值(钳制)
  */
void YMGUI_Slider_SetValue(GYOBJ sld, int32 value)
{
	gy_assert(sld && sld->user_data);
	gy_log_explain((sld == NULL) || (sld->user_data == NULL), GY_LOG_PtrI, "滑块或数据不存在");
	GYsld_data* d = (GYsld_data*)sld->user_data;
	d->value = GYLimitMaxMin(d->min, value, d->max);
	YMGUI_Obj_Invalidate(sld);
}

/**
  * @brief 读值
  */
int32 YMGUI_Slider_GetValue(GYOBJ sld)
{
	gy_assert(sld && sld->user_data);
	gy_log_explain((sld == NULL) || (sld->user_data == NULL), GY_LOG_PtrI, "滑块或数据不存在");
	return ((GYsld_data*)sld->user_data)->value;
}

/**
  * @brief 设值变回调
  */
void YMGUI_Slider_SetChanged(GYOBJ sld, GYsld_changed_cb cb)
{
	gy_assert(sld && sld->user_data);
	gy_log_explain((sld == NULL) || (sld->user_data == NULL), GY_LOG_PtrI, "滑块或数据不存在");
	((GYsld_data*)sld->user_data)->changed = cb;
}
