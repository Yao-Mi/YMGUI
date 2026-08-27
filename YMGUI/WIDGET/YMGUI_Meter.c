#include "YMGUI_Meter.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawArc.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_Font.h"
#include "YMGUI_Trig.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Meter.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 仪表盘。外圈弧 + 均布刻度线 + 指针(按值映射到角度)。全整数定点三角
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.值→角度:ang = start + (end-start)*(value-min)/(max-min)
  * 2.指针/刻度端点用定点三角表:pt = center + r*(cos,sin)>>Q15
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	int32   min, max, value;
	int32   start_deg, end_deg;
	uint8   ticks;
	uint8   show_labels;   //刻度旁是否标数值(默认 0)
	GYcolor label_color;   //刻度值文字色
}GYmeter_data;

/**
  * @brief 整数→十进制串(无 FPU,不引 sprintf 浮点;同 Bind.c 的 fmtInt)
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

/**
  * @brief 极坐标点:center + r*(cos ang, sin ang)
  */
static void polar(GYcoord cx, GYcoord cy, GYcoord r, int32 ang, GYcoord* ox, GYcoord* oy)
{
	*ox = (GYcoord)(cx + (((int32)r * GY_Cos(ang)) >> GY_TRIG_SHIFT));
	*oy = (GYcoord)(cy + (((int32)r * GY_Sin(ang)) >> GY_TRIG_SHIFT));
}

/**
  * @brief 值映射到角度
  */
static int32 valueToAngle(GYmeter_data* d)
{
	int32 span = d->max - d->min;
	if (span <= 0)
		return d->start_deg;
	return d->start_deg + (int32)((int64)(d->end_deg - d->start_deg) * (d->value - d->min) / span);
}

/**
  * @brief 绘制:外圈弧 + 刻度 + 指针
  */
static void meterDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYmeter_data* d = (GYmeter_data*)obj->user_data;
	GYcoord cx = abs->x + abs->w / 2;
	GYcoord cy = abs->y + abs->h / 2;
	GYcoord r = GYMin(abs->w, abs->h) / 2 - 2;
	if (r < 4)
		r = 4;

	//外圈弧
	YMGUI_Draw_Arc(s, cx, cy, r, d->start_deg, d->end_deg, GY_ARGB(0xFF, 0x80, 0x80, 0x88));
	//刻度线(从外圈向内 6px)
	if (d->ticks >= 2)
	{
		int32 arc = d->end_deg - d->start_deg;
		for (uint8 i = 0; i < d->ticks; i++)
		{
			int32 a = d->start_deg + arc * i / (d->ticks - 1);
			GYcoord x0, y0, x1, y1;
			polar(cx, cy, r, a, &x0, &y0);
			polar(cx, cy, r - 6, a, &x1, &y1);
			YMGUI_Draw_Line(s, x0, y0, x1, y1, GY_ARGB(0xFF, 0xB0, 0xB0, 0xB8));

			//刻度值:该刻度对应的量程值,文字定位到刻度更内侧并按文字宽高居中
			if (d->show_labels)
			{
				int32 tv = d->min + (int32)((int64)(d->max - d->min) * i / (d->ticks - 1));
				char buf[12];
				fmtInt(tv, buf, sizeof(buf));
				GYcoord lx, ly;
				polar(cx, cy, r - 16, a, &lx, &ly);
				GYcoord tw = YMGUI_Font_TextWidth(&YMGUI_Font_Default, buf);
				GYcoord th = YMGUI_Font_Default.cell_h;
				YMGUI_Draw_Text(s, &YMGUI_Font_Default, lx - tw / 2, ly - th / 2, buf, d->label_color);
			}
		}
	}
	//指针(中心到值角度,长度 r-8)
	int32 va = valueToAngle(d);
	GYcoord nx, ny;
	polar(cx, cy, r - 8, va, &nx, &ny);
	YMGUI_Draw_Line(s, cx, cy, nx, ny, GY_ARGB(0xFF, 0xE0, 0x40, 0x40));
	//中心枢轴
	YMGUI_Draw_CircleFill(s, cx, cy, 3, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0));
}

static void meterFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建仪表盘
  */
GYOBJ YMGUI_Creat_Meter_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ m = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(m);
	if (m == NULL)
		return NULL;
	GYmeter_data* d = (GYmeter_data*)GY_malloc0(sizeof(GYmeter_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "仪表盘数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(m); return NULL; }
	GY_memset(d, 0, sizeof(GYmeter_data));
	d->min = 0;
	d->max = 100;
	d->value = 0;
	d->start_deg = 135;//左下
	d->end_deg = 45 + 360;//右下,顺时针扫 270°
	d->ticks = 6;
	d->show_labels = 0;//默认关(小表盘挤字;需要时 SetShowLabels 打开)
	d->label_color = GY_ARGB(0xFF, 0xC0, 0xC0, 0xC8);

	m->type = GY_OBJ_Base;
	m->user_data = d;
	m->draw_cb = meterDrawCb;
	m->event_cb = NULL;
	m->free_cb = meterFreeCb;
	YMGUI_Obj_Invalidate(m);
	return m;
}

void YMGUI_Meter_SetRange(GYOBJ m, int32 min, int32 max)
{
	gy_assert(m && m->user_data);
	gy_log_explain((m == NULL) || (m->user_data == NULL), GY_LOG_PtrI, "仪表盘或数据不存在");
	GYmeter_data* d = (GYmeter_data*)m->user_data;
	d->min = min;
	d->max = (max > min) ? max : min + 1;
	d->value = GYLimitMaxMin(d->min, d->value, d->max);
	YMGUI_Obj_Invalidate(m);
}

void YMGUI_Meter_SetValue(GYOBJ m, int32 value)
{
	gy_assert(m && m->user_data);
	gy_log_explain((m == NULL) || (m->user_data == NULL), GY_LOG_PtrI, "仪表盘或数据不存在");
	GYmeter_data* d = (GYmeter_data*)m->user_data;
	d->value = GYLimitMaxMin(d->min, value, d->max);
	YMGUI_Obj_Invalidate(m);
}

int32 YMGUI_Meter_GetValue(GYOBJ m)
{
	gy_assert(m && m->user_data);
	gy_log_explain((m == NULL) || (m->user_data == NULL), GY_LOG_PtrI, "仪表盘或数据不存在");
	return ((GYmeter_data*)m->user_data)->value;
}

void YMGUI_Meter_SetAngles(GYOBJ m, int32 start_deg, int32 end_deg)
{
	gy_assert(m && m->user_data);
	gy_log_explain((m == NULL) || (m->user_data == NULL), GY_LOG_PtrI, "仪表盘或数据不存在");
	GYmeter_data* d = (GYmeter_data*)m->user_data;
	d->start_deg = start_deg;
	d->end_deg = (end_deg > start_deg) ? end_deg : end_deg + 360;
	YMGUI_Obj_Invalidate(m);
}

void YMGUI_Meter_SetTicks(GYOBJ m, uint8 count)
{
	gy_assert(m && m->user_data);
	gy_log_explain((m == NULL) || (m->user_data == NULL), GY_LOG_PtrI, "仪表盘或数据不存在");
	((GYmeter_data*)m->user_data)->ticks = count;
	YMGUI_Obj_Invalidate(m);
}

void YMGUI_Meter_SetShowLabels(GYOBJ m, uint8 on)
{
	gy_assert(m && m->user_data);
	gy_log_explain((m == NULL) || (m->user_data == NULL), GY_LOG_PtrI, "仪表盘或数据不存在");
	((GYmeter_data*)m->user_data)->show_labels = on ? 1 : 0;
	YMGUI_Obj_Invalidate(m);
}

void YMGUI_Meter_SetLabelColor(GYOBJ m, GYcolor color)
{
	gy_assert(m && m->user_data);
	gy_log_explain((m == NULL) || (m->user_data == NULL), GY_LOG_PtrI, "仪表盘或数据不存在");
	((GYmeter_data*)m->user_data)->label_color = color;
	YMGUI_Obj_Invalidate(m);
}
