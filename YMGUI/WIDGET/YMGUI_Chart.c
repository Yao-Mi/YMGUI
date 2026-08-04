#include "YMGUI_Chart.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Chart.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-03
  *	@Description: 折线图。网格 + 多序列多点折线。y 值按 [min,max] 映射到高度(翻转:大值在上)。
  *	              点沿 x 均布。支持流式 SetNext(左移末尾入新)。只显示不交互
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.点 i 的屏幕坐标:px = abs.x + w*i/(cnt-1);py = abs.y + h - h*(v-min)/(max-min)
  * 2.相邻点用 DrawLine 连接。序列数据数组按 point_cnt 动态申请
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	GYcolor color;
	int32*  pts;//point_cnt 个值
}GYchart_series;

typedef struct
{
	int32          min, max;
	uint16         point_cnt;
	uint8          hdiv, vdiv;//网格分格数(0=不画该向)
	GYcolor        bg, grid;
	uint8          series_cnt;
	GYchart_series series[GY_CHART_MAX_SERIES];
}GYchart_data;

/**
  * @brief 值映射到该点屏幕 y(翻转,大值在上)。空间不足时钳到 abs 内
  */
static GYcoord valueToY(GYchart_data* d, const GYrect* abs, int32 v)
{
	int32 span = d->max - d->min;
	if (span <= 0)
		return abs->y + abs->h - 1;
	v = GYLimitMaxMin(d->min, v, d->max);
	int32 off = (int32)((int64)(v - d->min) * (abs->h - 1) / span);
	return (GYcoord)(abs->y + abs->h - 1 - off);
}

/**
  * @brief 点索引映射到屏幕 x(沿宽度均布,钳到边界内)
  */
static GYcoord idxToX(GYchart_data* d, const GYrect* abs, uint16 i)
{
	if (d->point_cnt <= 1)
		return abs->x;
	GYcoord x = (GYcoord)(abs->x + (int32)(abs->w - 1) * i / (d->point_cnt - 1));
	return x;
}

/**
  * @brief 绘制:背景 + 网格 + 各序列折线
  */
static void chartDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYchart_data* d = (GYchart_data*)obj->user_data;
	//背景
	YMGUI_Draw_Fill(s, abs, d->bg, GY_OPA_COVER);
	//网格:vdiv 条竖线 + hdiv 条横线(等分,边界内)
	if (d->vdiv >= 1)
	{
		for (uint8 i = 0; i <= d->vdiv; i++)
		{
			GYcoord gx = (GYcoord)(abs->x + (int32)(abs->w - 1) * i / d->vdiv);
			YMGUI_Draw_Line(s, gx, abs->y, gx, abs->y + abs->h - 1, d->grid);
		}
	}
	if (d->hdiv >= 1)
	{
		for (uint8 i = 0; i <= d->hdiv; i++)
		{
			GYcoord gy = (GYcoord)(abs->y + (int32)(abs->h - 1) * i / d->hdiv);
			YMGUI_Draw_Line(s, abs->x, gy, abs->x + abs->w - 1, gy, d->grid);
		}
	}
	//各序列折线
	for (uint8 sidx = 0; sidx < d->series_cnt; sidx++)
	{
		GYchart_series* se = &d->series[sidx];
		if (se->pts == NULL || d->point_cnt < 2)
			continue;
		GYcoord px0 = idxToX(d, abs, 0);
		GYcoord py0 = valueToY(d, abs, se->pts[0]);
		for (uint16 i = 1; i < d->point_cnt; i++)
		{
			GYcoord px1 = idxToX(d, abs, i);
			GYcoord py1 = valueToY(d, abs, se->pts[i]);
			YMGUI_Draw_Line(s, px0, py0, px1, py1, se->color);
			px0 = px1;
			py0 = py1;
		}
	}
}

static void chartFreeCb(GYOBJ obj)
{
	GYchart_data* d = (GYchart_data*)obj->user_data;
	if (d != NULL)
	{
		for (uint8 i = 0; i < d->series_cnt; i++)
		{
			if (d->series[i].pts != NULL)
				GY_free0(d->series[i].pts);
		}
		GY_free0(d);
		obj->user_data = NULL;
	}
}

/**
  * @brief (重新)申请某序列点数组,填 min。失败置 NULL
  */
static void allocSeriesPts(GYchart_data* d, GYchart_series* se)
{
	if (se->pts != NULL)
		GY_free0(se->pts);
	se->pts = NULL;
	if (d->point_cnt == 0)
		return;
	int32* p = (int32*)GY_malloc0((uint32)d->point_cnt * sizeof(int32));
	gy_log_explain(p == NULL, GY_LOG_Mem0, "折线图序列点数组申请失败");
	if (p == NULL)
		return;
	for (uint16 i = 0; i < d->point_cnt; i++)
		p[i] = d->min;
	se->pts = p;
}

/**
  * @brief 创建折线图
  */
GYOBJ YMGUI_Creat_Chart_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ chart = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(chart);
	if (chart == NULL)
		return NULL;
	GYchart_data* d = (GYchart_data*)GY_malloc0(sizeof(GYchart_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "折线图数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(chart); return NULL; }
	d->min = 0;
	d->max = 100;
	d->point_cnt = 10;
	d->hdiv = 4;
	d->vdiv = 4;
	d->bg = GY_ARGB(0xFF, 0x18, 0x18, 0x20);
	d->grid = GY_ARGB(0xFF, 0x40, 0x40, 0x48);
	d->series_cnt = 0;

	chart->type = GY_OBJ_Base;
	chart->user_data = d;
	chart->draw_cb = chartDrawCb;
	chart->event_cb = NULL;//只显示不交互
	chart->free_cb = chartFreeCb;
	YMGUI_Obj_Invalidate(chart);
	return chart;
}

/**
  * @brief 设 y 值域(钳制既有点)
  */
void YMGUI_Chart_SetRange(GYOBJ chart, int32 min, int32 max)
{
	gy_assert(chart && chart->user_data);
	gy_log_explain((chart == NULL) || (chart->user_data == NULL), GY_LOG_PtrI, "折线图或数据不存在");
	GYchart_data* d = (GYchart_data*)chart->user_data;
	d->min = min;
	d->max = (max > min) ? max : min + 1;
	for (uint8 sidx = 0; sidx < d->series_cnt; sidx++)
	{
		if (d->series[sidx].pts == NULL)
			continue;
		for (uint16 i = 0; i < d->point_cnt; i++)
			d->series[sidx].pts[i] = GYLimitMaxMin(d->min, d->series[sidx].pts[i], d->max);
	}
	YMGUI_Obj_Invalidate(chart);
}

/**
  * @brief 设每序列点数(所有序列重置为 min)
  */
void YMGUI_Chart_SetPointCount(GYOBJ chart, uint16 count)
{
	gy_assert(chart && chart->user_data);
	gy_log_explain((chart == NULL) || (chart->user_data == NULL), GY_LOG_PtrI, "折线图或数据不存在");
	GYchart_data* d = (GYchart_data*)chart->user_data;
	d->point_cnt = count;
	for (uint8 sidx = 0; sidx < d->series_cnt; sidx++)
		allocSeriesPts(d, &d->series[sidx]);
	YMGUI_Obj_Invalidate(chart);
}

/**
  * @brief 设网格分格数(0=不画该向)
  */
void YMGUI_Chart_SetGrid(GYOBJ chart, uint8 hdiv, uint8 vdiv)
{
	gy_assert(chart && chart->user_data);
	gy_log_explain((chart == NULL) || (chart->user_data == NULL), GY_LOG_PtrI, "折线图或数据不存在");
	GYchart_data* d = (GYchart_data*)chart->user_data;
	d->hdiv = hdiv;
	d->vdiv = vdiv;
	YMGUI_Obj_Invalidate(chart);
}

/**
  * @brief 设背景/网格色
  */
void YMGUI_Chart_SetColors(GYOBJ chart, GYcolor bg, GYcolor grid)
{
	gy_assert(chart && chart->user_data);
	gy_log_explain((chart == NULL) || (chart->user_data == NULL), GY_LOG_PtrI, "折线图或数据不存在");
	GYchart_data* d = (GYchart_data*)chart->user_data;
	d->bg = bg;
	d->grid = grid;
	YMGUI_Obj_Invalidate(chart);
}

/**
  * @brief 加序列,返回索引;满或申请失败返回 -1
  */
int32 YMGUI_Chart_AddSeries(GYOBJ chart, GYcolor color)
{
	gy_assert(chart && chart->user_data);
	gy_log_explain((chart == NULL) || (chart->user_data == NULL), GY_LOG_PtrI, "折线图或数据不存在");
	if (chart == NULL || chart->user_data == NULL)
		return -1;
	GYchart_data* d = (GYchart_data*)chart->user_data;
	if (d->series_cnt >= GY_CHART_MAX_SERIES)
		return -1;
	GYchart_series* se = &d->series[d->series_cnt];
	se->color = color;
	se->pts = NULL;
	allocSeriesPts(d, se);
	if (se->pts == NULL && d->point_cnt > 0)
		return -1;//申请失败,不占用序列位
	int32 idx = d->series_cnt;
	d->series_cnt++;
	YMGUI_Obj_Invalidate(chart);
	return idx;
}

/**
  * @brief 设某序列某点(钳制)
  */
void YMGUI_Chart_SetValue(GYOBJ chart, int32 series, uint16 idx, int32 value)
{
	gy_assert(chart && chart->user_data);
	gy_log_explain((chart == NULL) || (chart->user_data == NULL), GY_LOG_PtrI, "折线图或数据不存在");
	if (chart == NULL || chart->user_data == NULL)
		return;
	GYchart_data* d = (GYchart_data*)chart->user_data;
	if (series < 0 || series >= d->series_cnt || idx >= d->point_cnt)
		return;
	if (d->series[series].pts == NULL)
		return;
	d->series[series].pts[idx] = GYLimitMaxMin(d->min, value, d->max);
	YMGUI_Obj_Invalidate(chart);
}

/**
  * @brief 整体左移,末尾入新值(流式滚动)
  */
void YMGUI_Chart_SetNext(GYOBJ chart, int32 series, int32 value)
{
	gy_assert(chart && chart->user_data);
	gy_log_explain((chart == NULL) || (chart->user_data == NULL), GY_LOG_PtrI, "折线图或数据不存在");
	if (chart == NULL || chart->user_data == NULL)
		return;
	GYchart_data* d = (GYchart_data*)chart->user_data;
	if (series < 0 || series >= d->series_cnt || d->point_cnt == 0)
		return;
	int32* p = d->series[series].pts;
	if (p == NULL)
		return;
	for (uint16 i = 1; i < d->point_cnt; i++)
		p[i - 1] = p[i];
	p[d->point_cnt - 1] = GYLimitMaxMin(d->min, value, d->max);
	YMGUI_Obj_Invalidate(chart);
}

/**
  * @brief 读某点(越界返回 min)
  */
int32 YMGUI_Chart_GetValue(GYOBJ chart, int32 series, uint16 idx)
{
	gy_assert(chart && chart->user_data);
	gy_log_explain((chart == NULL) || (chart->user_data == NULL), GY_LOG_PtrI, "折线图或数据不存在");
	if (chart == NULL || chart->user_data == NULL)
		return 0;
	GYchart_data* d = (GYchart_data*)chart->user_data;
	if (series < 0 || series >= d->series_cnt || idx >= d->point_cnt || d->series[series].pts == NULL)
		return d->min;
	return d->series[series].pts[idx];
}
