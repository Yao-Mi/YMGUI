#include "YMGUI_ArcWidget.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawArc.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_ArcWidget.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 环形进度控件。背景弧整段 + 前景弧按值占比,多层半径描粗
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.前景终止角 = start + (end-start)*(value-min)/(max-min),整数运算
  * 2.粗环用 [r-thick/2, r+thick/2] 多层 1px 圆弧叠出来
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	int32   min, max, value;
	int32   start_deg, end_deg;
	GYcoord thickness;
	GYcolor bg, fg;
}GYarc_data;

/**
  * @brief 粗弧:用图元的无缝扇环填充(r 附近 thickness 宽)
  */
static void thickArc(GYSURFACE s, GYcoord cx, GYcoord cy, GYcoord r, GYcoord thick, int32 a0, int32 a1, GYcolor c)
{
	GYcoord half = thick / 2;
	GYcoord r_in = r - half;
	GYcoord r_out = r + (thick - half - 1);//thickness 层
	if (r_in < 1)
		r_in = 1;
	YMGUI_Draw_ArcThick(s, cx, cy, r_in, r_out, a0, a1, c);
}

/**
  * @brief 绘制:背景整段弧 + 前景按值弧
  */
static void arcDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYarc_data* d = (GYarc_data*)obj->user_data;
	GYcoord cx = abs->x + abs->w / 2;
	GYcoord cy = abs->y + abs->h / 2;
	GYcoord rad = (GYMin(abs->w, abs->h) / 2) - d->thickness;
	if (rad < 1)
		rad = 1;

	//背景弧(整段)
	thickArc(s, cx, cy, rad, d->thickness, d->start_deg, d->end_deg, d->bg);
	//前景弧(按值)
	int32 span = d->max - d->min;
	if (span > 0)
	{
		int32 arc_span = d->end_deg - d->start_deg;
		int32 fg_end = d->start_deg + (int32)((int64)arc_span * (d->value - d->min) / span);
		if (fg_end > d->start_deg)
			thickArc(s, cx, cy, rad, d->thickness, d->start_deg, fg_end, d->fg);
	}
}

static void arcFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建环形进度控件
  */
GYOBJ YMGUI_Creat_Arc_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ arc = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(arc);
	if (arc == NULL)
		return NULL;
	GYarc_data* d = (GYarc_data*)GY_malloc0(sizeof(GYarc_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "环形控件数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(arc); return NULL; }
	GY_memset(d, 0, sizeof(GYarc_data));
	d->min = 0;
	d->max = 100;
	d->value = 0;
	d->start_deg = 135;//默认下方开口的 270° 弧(135→45 顺时针)
	d->end_deg = 45 + 360;//跨 270°
	d->thickness = 6;
	d->bg = GY_ARGB(0xFF, 0x40, 0x40, 0x48);
	d->fg = GY_ARGB(0xFF, 0x30, 0x90, 0xE0);

	arc->type = GY_OBJ_Base;
	arc->user_data = d;
	arc->draw_cb = arcDrawCb;
	arc->event_cb = NULL;
	arc->free_cb = arcFreeCb;
	YMGUI_Obj_Invalidate(arc);
	return arc;
}

void YMGUI_Arc_SetRange(GYOBJ arc, int32 min, int32 max)
{
	gy_assert(arc && arc->user_data);
	gy_log_explain((arc == NULL) || (arc->user_data == NULL), GY_LOG_PtrI, "环形控件或数据不存在");
	GYarc_data* d = (GYarc_data*)arc->user_data;
	d->min = min;
	d->max = (max > min) ? max : min + 1;
	d->value = GYLimitMaxMin(d->min, d->value, d->max);
	YMGUI_Obj_Invalidate(arc);
}

void YMGUI_Arc_SetValue(GYOBJ arc, int32 value)
{
	gy_assert(arc && arc->user_data);
	gy_log_explain((arc == NULL) || (arc->user_data == NULL), GY_LOG_PtrI, "环形控件或数据不存在");
	GYarc_data* d = (GYarc_data*)arc->user_data;
	d->value = GYLimitMaxMin(d->min, value, d->max);
	YMGUI_Obj_Invalidate(arc);
}

int32 YMGUI_Arc_GetValue(GYOBJ arc)
{
	gy_assert(arc && arc->user_data);
	gy_log_explain((arc == NULL) || (arc->user_data == NULL), GY_LOG_PtrI, "环形控件或数据不存在");
	return ((GYarc_data*)arc->user_data)->value;
}

void YMGUI_Arc_SetAngles(GYOBJ arc, int32 start_deg, int32 end_deg)
{
	gy_assert(arc && arc->user_data);
	gy_log_explain((arc == NULL) || (arc->user_data == NULL), GY_LOG_PtrI, "环形控件或数据不存在");
	GYarc_data* d = (GYarc_data*)arc->user_data;
	d->start_deg = start_deg;
	d->end_deg = (end_deg > start_deg) ? end_deg : end_deg + 360;
	YMGUI_Obj_Invalidate(arc);
}

void YMGUI_Arc_SetWidth(GYOBJ arc, GYcoord thickness)
{
	gy_assert(arc && arc->user_data);
	gy_log_explain((arc == NULL) || (arc->user_data == NULL), GY_LOG_PtrI, "环形控件或数据不存在");
	((GYarc_data*)arc->user_data)->thickness = (thickness > 0) ? thickness : 1;
	YMGUI_Obj_Invalidate(arc);
}

void YMGUI_Arc_SetColors(GYOBJ arc, GYcolor bg, GYcolor fg)
{
	gy_assert(arc && arc->user_data);
	gy_log_explain((arc == NULL) || (arc->user_data == NULL), GY_LOG_PtrI, "环形控件或数据不存在");
	GYarc_data* d = (GYarc_data*)arc->user_data;
	d->bg = bg;
	d->fg = fg;
	YMGUI_Obj_Invalidate(arc);
}
