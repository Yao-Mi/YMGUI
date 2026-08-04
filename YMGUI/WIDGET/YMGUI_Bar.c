#include "YMGUI_Bar.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Bar.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 进度条。背景槽满铺,前景按 value/(max-min) 比例填充左段。只显示不交互
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	int32   min, max, value;
	GYcolor bg, fg;
}GYbar_data;

/**
  * @brief 绘制:背景槽 + 前景填充段(整数比例)
  */
static void barDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYbar_data* d = (GYbar_data*)obj->user_data;
	//背景槽
	YMGUI_Draw_Fill(s, abs, d->bg, GY_OPA_COVER);
	//前景填充宽度 = w * (value-min)/(max-min)
	int32 span = d->max - d->min;
	if (span > 0)
	{
		GYcoord fw = (GYcoord)((int64)(d->value - d->min) * abs->w / span);
		if (fw > 0)
		{
			GYrect fill = {abs->x, abs->y, fw, abs->h};
			YMGUI_Draw_Fill(s, &fill, d->fg, GY_OPA_COVER);
		}
	}
}

static void barFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建进度条
  */
GYOBJ YMGUI_Creat_Bar_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ bar = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(bar);
	if (bar == NULL)
		return NULL;
	GYbar_data* d = (GYbar_data*)GY_malloc0(sizeof(GYbar_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "进度条数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(bar); return NULL; }
	d->min = 0;
	d->max = 100;
	d->value = 0;
	d->bg = GY_ARGB(0xFF, 0x50, 0x50, 0x58);
	d->fg = GY_ARGB(0xFF, 0x40, 0xC0, 0x50);

	bar->type = GY_OBJ_Base;
	bar->user_data = d;
	bar->draw_cb = barDrawCb;
	bar->event_cb = NULL;//只显示不交互
	bar->free_cb = barFreeCb;
	YMGUI_Obj_Invalidate(bar);
	return bar;
}

/**
  * @brief 设范围
  */
void YMGUI_Bar_SetRange(GYOBJ bar, int32 min, int32 max)
{
	gy_assert(bar && bar->user_data);
	gy_log_explain((bar == NULL) || (bar->user_data == NULL), GY_LOG_PtrI, "进度条或数据不存在");
	GYbar_data* d = (GYbar_data*)bar->user_data;
	d->min = min;
	d->max = (max > min) ? max : min + 1;
	d->value = GYLimitMaxMin(d->min, d->value, d->max);
	YMGUI_Obj_Invalidate(bar);
}

/**
  * @brief 设值(钳制)
  */
void YMGUI_Bar_SetValue(GYOBJ bar, int32 value)
{
	gy_assert(bar && bar->user_data);
	gy_log_explain((bar == NULL) || (bar->user_data == NULL), GY_LOG_PtrI, "进度条或数据不存在");
	GYbar_data* d = (GYbar_data*)bar->user_data;
	d->value = GYLimitMaxMin(d->min, value, d->max);
	YMGUI_Obj_Invalidate(bar);
}

/**
  * @brief 读值
  */
int32 YMGUI_Bar_GetValue(GYOBJ bar)
{
	gy_assert(bar && bar->user_data);
	gy_log_explain((bar == NULL) || (bar->user_data == NULL), GY_LOG_PtrI, "进度条或数据不存在");
	return ((GYbar_data*)bar->user_data)->value;
}

/**
  * @brief 设槽/前景色
  */
void YMGUI_Bar_SetColors(GYOBJ bar, GYcolor bg, GYcolor fg)
{
	gy_assert(bar && bar->user_data);
	gy_log_explain((bar == NULL) || (bar->user_data == NULL), GY_LOG_PtrI, "进度条或数据不存在");
	GYbar_data* d = (GYbar_data*)bar->user_data;
	d->bg = bg;
	d->fg = fg;
	YMGUI_Obj_Invalidate(bar);
}
