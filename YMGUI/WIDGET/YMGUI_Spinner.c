#include "YMGUI_Spinner.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawArc.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Spinner.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 加载转圈。固定跨度圆弧,每次 Tick 起始角前进 step 度并标脏。无内部定时器
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	int32   angle;    //当前起始角
	int32   span;     //弧跨度(度)
	int32   step;     //每步前进(度)
	GYcoord thickness;
	GYcolor color;
}GYsp_data;

/**
  * @brief 绘制:从 angle 起画 span 度的粗弧
  */
static void spDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYsp_data* d = (GYsp_data*)obj->user_data;
	GYcoord cx = abs->x + abs->w / 2;
	GYcoord cy = abs->y + abs->h / 2;
	GYcoord rad = (GYMin(abs->w, abs->h) / 2) - d->thickness;
	if (rad < 1)
		rad = 1;
	GYcoord half = d->thickness / 2;
	GYcoord r_in = rad - half;
	GYcoord r_out = rad + (d->thickness - half - 1);
	if (r_in < 1)
		r_in = 1;
	YMGUI_Draw_ArcThick(s, cx, cy, r_in, r_out, d->angle, d->angle + d->span, d->color);
}

static void spFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建加载转圈
  */
GYOBJ YMGUI_Creat_Spinner_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ sp = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(sp);
	if (sp == NULL)
		return NULL;
	GYsp_data* d = (GYsp_data*)GY_malloc0(sizeof(GYsp_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "转圈控件数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(sp); return NULL; }
	GY_memset(d, 0, sizeof(GYsp_data));
	d->angle = 0;
	d->span = 90;
	d->step = 30;
	d->thickness = 5;
	d->color = GY_ARGB(0xFF, 0x30, 0x90, 0xE0);

	sp->type = GY_OBJ_Base;
	sp->user_data = d;
	sp->draw_cb = spDrawCb;
	sp->event_cb = NULL;
	sp->free_cb = spFreeCb;
	YMGUI_Obj_Invalidate(sp);
	return sp;
}

/**
  * @brief 旋转一步
  */
void YMGUI_Spinner_Tick(GYOBJ sp)
{
	gy_assert(sp && sp->user_data);
	gy_log_explain((sp == NULL) || (sp->user_data == NULL), GY_LOG_PtrI, "转圈控件或数据不存在");
	GYsp_data* d = (GYsp_data*)sp->user_data;
	d->angle = (d->angle + d->step) % 360;
	YMGUI_Obj_Invalidate(sp);
}

void YMGUI_Spinner_SetSpan(GYOBJ sp, int32 span_deg, int32 step_deg)
{
	gy_assert(sp && sp->user_data);
	gy_log_explain((sp == NULL) || (sp->user_data == NULL), GY_LOG_PtrI, "转圈控件或数据不存在");
	GYsp_data* d = (GYsp_data*)sp->user_data;
	d->span = span_deg;
	d->step = step_deg;
	YMGUI_Obj_Invalidate(sp);
}

void YMGUI_Spinner_SetColor(GYOBJ sp, GYcolor color)
{
	gy_assert(sp && sp->user_data);
	gy_log_explain((sp == NULL) || (sp->user_data == NULL), GY_LOG_PtrI, "转圈控件或数据不存在");
	((GYsp_data*)sp->user_data)->color = color;
	YMGUI_Obj_Invalidate(sp);
}

void YMGUI_Spinner_SetWidth(GYOBJ sp, GYcoord thickness)
{
	gy_assert(sp && sp->user_data);
	gy_log_explain((sp == NULL) || (sp->user_data == NULL), GY_LOG_PtrI, "转圈控件或数据不存在");
	((GYsp_data*)sp->user_data)->thickness = (thickness > 0) ? thickness : 1;
	YMGUI_Obj_Invalidate(sp);
}
