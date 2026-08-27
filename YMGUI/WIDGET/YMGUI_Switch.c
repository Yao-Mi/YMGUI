#include "YMGUI_Switch.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Switch.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 开关控件。轨道按 on/off 换色,滑钮居左(off)或居右(on)。点击切换并回调
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	uint8           on;
	GYsw_changed_cb changed;
}GYsw_data;

/**
  * @brief 绘制:轨道(on绿/off灰) + 滑钮(白方块,居左或居右)
  */
static void swDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYsw_data* d = (GYsw_data*)obj->user_data;
	//轨道
	GYcolor track = d->on ? GY_ARGB(0xFF, 0x40, 0xC0, 0x50) : GY_ARGB(0xFF, 0x60, 0x60, 0x68);
	YMGUI_Draw_Fill(s, abs, track, GY_OPA_COVER);
	//滑钮:边长 = 高-4,留 2px 边距;on 靠右,off 靠左
	GYcoord kn = abs->h - 4;
	GYcoord kx = d->on ? (abs->x + abs->w - kn - 2) : (abs->x + 2);
	GYrect knob = {kx, abs->y + 2, kn, kn};
	YMGUI_Draw_Fill(s, &knob, GY_ARGB(0xFF, 0xF8, 0xF8, 0xF8), GY_OPA_COVER);
}

/**
  * @brief 事件:点击切换 on
  */
static void swEventCb(GYOBJ obj, GYEvent e)
{
	GYsw_data* d = (GYsw_data*)obj->user_data;
	if (e == GY_EVENT_Clicked)
	{
		d->on = !d->on;
		YMGUI_Obj_Invalidate(obj);
		if (d->changed != NULL)
			d->changed(obj, d->on);
	}
}

static void swFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建开关
  */
GYOBJ YMGUI_Creat_Switch_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ sw = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(sw);
	if (sw == NULL)
		return NULL;
	GYsw_data* d = (GYsw_data*)GY_malloc0(sizeof(GYsw_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "开关数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(sw); return NULL; }
	GY_memset(d, 0, sizeof(GYsw_data));
	d->on = 0;
	d->changed = NULL;

	sw->type = GY_OBJ_Base;
	sw->user_data = d;
	sw->draw_cb = swDrawCb;
	sw->event_cb = swEventCb;
	sw->free_cb = swFreeCb;
	YMGUI_Obj_Invalidate(sw);
	return sw;
}

/**
  * @brief 设开关态
  */
void YMGUI_Switch_SetOn(GYOBJ sw, uint8 on)
{
	gy_assert(sw && sw->user_data);
	gy_log_explain((sw == NULL) || (sw->user_data == NULL), GY_LOG_PtrI, "开关或数据不存在");
	((GYsw_data*)sw->user_data)->on = on ? 1 : 0;
	YMGUI_Obj_Invalidate(sw);
}

/**
  * @brief 读开关态
  */
uint8 YMGUI_Switch_GetOn(GYOBJ sw)
{
	gy_assert(sw && sw->user_data);
	gy_log_explain((sw == NULL) || (sw->user_data == NULL), GY_LOG_PtrI, "开关或数据不存在");
	return ((GYsw_data*)sw->user_data)->on;
}

/**
  * @brief 设值变回调
  */
void YMGUI_Switch_SetChanged(GYOBJ sw, GYsw_changed_cb cb)
{
	gy_assert(sw && sw->user_data);
	gy_log_explain((sw == NULL) || (sw->user_data == NULL), GY_LOG_PtrI, "开关或数据不存在");
	((GYsw_data*)sw->user_data)->changed = cb;
}
