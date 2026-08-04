#include "YMGUI_BarChart.h"

#if YMGUI_BARCHART

#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_BarChart.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 通用柱状图。N 个竖直柱按值映射高度。两个正交维度:配色模式(BY_HEIGHT 随高度
  *	              渐变 / PER_BAR 每柱独立调色板)× 顶部回落模式(NONE 无 / BAR 高亮细条峰值保持
  *	              +回落)。只显示不交互。库不做数据分析,值由外部喂进来(app 侧做 DFT/统计)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息:
  * 1.每柱两个量:val(柱身当前高度值)、top(顶标值)。SetValue 抬 val,TopMode=BAR 时更高则顶 top。
  * 2.Tick 里 val 减 fall、top 减 top_fall(各自钳 0/钳到 val),实现回落动画。
  * 3.BY_HEIGHT:柱身**逐行竖直渐变** —— 每一行像素按其在控件内的竖直高度占比在 lo→hi 间插值
  *   (底部=lo,越往上越偏 hi;柱越高其顶端越接近 hi)。同一物理量不同强度的直观着色。
  *   PER_BAR:柱色 = 调色板 pal[i] 整柱一色,与高度无关。SetBarCount 时按 lo→hi 铺默认渐变调色板。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define BC_TOP_MARK_H 2 //顶标条厚度(像素)

typedef struct
{
	int32   n;              //柱数
	int32   max;            //值域上限
	int32   val[GY_BARCHART_MAX_BARS];  //柱身当前值
	int32   top[GY_BARCHART_MAX_BARS];  //顶标值
	GYcolor pal[GY_BARCHART_MAX_BARS];  //PER_BAR 调色板

	uint8   color_mode;     //GY_BARCHART_COLOR_*
	uint8   top_mode;       //GY_BARCHART_TOP_*
	GYcolor c_lo, c_hi;     //BY_HEIGHT 渐变两端
	GYcolor c_top;          //顶标条色
	GYcolor bg;
	int32   fall;           //柱身每帧回落量
	int32   top_fall;       //顶标每帧回落量
	GYcoord gap;            //柱间空隙
}GYbarchart_data;

//按比例(num/den)在 lo→hi 间线性插值一个通道
static uint8 lerpChan(uint8 lo, uint8 hi, int32 num, int32 den)
{
	if (den <= 0) return lo;
	int32 v = (int32)lo + ((int32)hi - (int32)lo) * num / den;
	return (uint8)GYLimitMaxMin(0, v, 255);
}

//按比例算插值色
static GYcolor lerpColor(GYcolor lo, GYcolor hi, int32 num, int32 den)
{
	uint8 r = lerpChan((uint8)GY_COLOR_R(lo), (uint8)GY_COLOR_R(hi), num, den);
	uint8 g = lerpChan((uint8)GY_COLOR_G(lo), (uint8)GY_COLOR_G(hi), num, den);
	uint8 b = lerpChan((uint8)GY_COLOR_B(lo), (uint8)GY_COLOR_B(hi), num, den);
	return GY_ARGB(0xFF, r, g, b);
}

//按 lo→hi 给整条调色板铺一版默认渐变(柱 i 占 i/(n-1))
static void seedPalette(GYbarchart_data* d)
{
	int32 i;
	int32 den = (d->n > 1) ? (d->n - 1) : 1;
	for (i = 0; i < GY_BARCHART_MAX_BARS; i++)
		d->pal[i] = lerpColor(d->c_lo, d->c_hi, (i < d->n) ? i : (d->n - 1), den);
}

//画第 i 根柱:PER_BAR 整柱一色(调色板);BY_HEIGHT 逐行竖直渐变(底=lo,越往上越趋 hi,
//以控件全高 full_h 为渐变标尺 → 柱越高其顶端越接近 hi)
static void drawBar(GYbarchart_data* d, GYSURFACE s, int32 i, GYcoord x, GYcoord bw,
                    GYcoord top_y, GYcoord bottom_y, GYcoord full_h)
{
	if (d->color_mode == GY_BARCHART_COLOR_PER_BAR)
	{
		GYrect bar = {x, top_y, bw, (GYcoord)(bottom_y - top_y)};
		YMGUI_Draw_Fill(s, &bar, d->pal[i], GY_OPA_COVER);
		return;
	}
	//BY_HEIGHT:逐行(1px 高横条)按"该行距柱底的像素数占控件全高比例"在 lo→hi 插值
	GYcoord yy;
	GYcoord den = (full_h > 0) ? full_h : 1;
	for (yy = top_y; yy < bottom_y; yy++)
	{
		int32 up = (int32)(bottom_y - yy);     //该行距柱底的像素数(1..)
		GYcolor c = lerpColor(d->c_lo, d->c_hi, up, den);
		GYrect row = {x, yy, bw, 1};
		YMGUI_Draw_Fill(s, &row, c, GY_OPA_COVER);
	}
}

/**
  * @brief 绘制:背景 → 每柱(柱身色 + 顶标条)
  */
static void barchartDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYbarchart_data* d = (GYbarchart_data*)obj->user_data;

	YMGUI_Draw_Fill(s, abs, d->bg, GY_OPA_COVER);

	if (d->n <= 0 || d->max <= 0)
		return;

	//每柱占宽(含空隙),末柱补余
	GYcoord total_gap = (GYcoord)(d->gap * (d->n - 1));
	GYcoord bars_w = (GYcoord)(abs->w - total_gap);
	if (bars_w < d->n) bars_w = d->n; //至少每柱 1px
	GYcoord bw = (GYcoord)(bars_w / d->n);
	if (bw < 1) bw = 1;

	int32 i;
	GYcoord x = abs->x;
	for (i = 0; i < d->n; i++)
	{
		//柱身高度 = h * val/max
		int32 v = GYLimitMaxMin(0, d->val[i], d->max);
		GYcoord bh = (GYcoord)((int64)v * abs->h / d->max);
		if (bh > 0)
		{
			GYcoord bottom_y = (GYcoord)(abs->y + abs->h);
			GYcoord top_y    = (GYcoord)(bottom_y - bh);
			drawBar(d, s, i, x, bw, top_y, bottom_y, abs->h);
		}
		//顶标条
		if (d->top_mode == GY_BARCHART_TOP_BAR)
		{
			int32 tv = GYLimitMaxMin(0, d->top[i], d->max);
			if (tv > 0)
			{
				GYcoord ty = (GYcoord)(abs->y + abs->h - (GYcoord)((int64)tv * abs->h / d->max));
				GYrect mark = {x, ty, bw, BC_TOP_MARK_H};
				YMGUI_Draw_Fill(s, &mark, d->c_top, GY_OPA_COVER);
			}
		}
		x = (GYcoord)(x + bw + d->gap);
	}
}

static void barchartFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建 BarChart
  */
GYOBJ YMGUI_Creat_BarChart_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ bc = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(bc);
	if (bc == NULL)
		return NULL;
	GYbarchart_data* d = (GYbarchart_data*)GY_malloc0(sizeof(GYbarchart_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "BarChart 数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(bc); return NULL; }

	d->n          = 16;
	d->max        = 100;
	d->color_mode = GY_BARCHART_COLOR_BY_HEIGHT;
	d->top_mode   = GY_BARCHART_TOP_BAR;
	d->c_lo       = GY_ARGB(0xFF, 0x30, 0x80, 0xE0);
	d->c_hi       = GY_ARGB(0xFF, 0xF0, 0x40, 0x40);
	d->c_top      = GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
	d->bg         = GY_ARGB(0xFF, 0x10, 0x10, 0x16);
	d->fall       = 6;
	d->top_fall   = 2;
	d->gap        = 2;
	seedPalette(d);

	bc->type = GY_OBJ_Base;
	bc->user_data = d;
	bc->draw_cb = barchartDrawCb;
	bc->event_cb = NULL; //只显示不交互
	bc->free_cb = barchartFreeCb;
	YMGUI_Obj_Invalidate(bc);
	return bc;
}

void YMGUI_BarChart_SetBarCount(GYOBJ bc, int32 n)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	GYbarchart_data* d = (GYbarchart_data*)bc->user_data;
	n = GYLimitMaxMin(1, n, GY_BARCHART_MAX_BARS);
	d->n = n;
	int32 i;
	for (i = 0; i < GY_BARCHART_MAX_BARS; i++) { d->val[i] = 0; d->top[i] = 0; }
	seedPalette(d); //柱数变了,默认调色板按新柱数重铺
	YMGUI_Obj_Invalidate(bc);
}

int32 YMGUI_BarChart_GetBarCount(GYOBJ bc)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return 0;
	return ((GYbarchart_data*)bc->user_data)->n;
}

void YMGUI_BarChart_SetRange(GYOBJ bc, int32 max)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	((GYbarchart_data*)bc->user_data)->max = (max > 0) ? max : 1;
	YMGUI_Obj_Invalidate(bc);
}

void YMGUI_BarChart_SetValue(GYOBJ bc, int32 i, int32 value)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	GYbarchart_data* d = (GYbarchart_data*)bc->user_data;
	if (i < 0 || i >= d->n) return;
	int32 v = GYLimitMaxMin(0, value, d->max);
	d->val[i] = v; //允许直接落(外部想瞬时压低);顶标由 top_mode 决定是否保持
	if (v > d->top[i]) d->top[i] = v;
	YMGUI_Obj_Invalidate(bc);
}

void YMGUI_BarChart_SetValues(GYOBJ bc, const int32* arr, int32 n)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL || arr == NULL) return;
	GYbarchart_data* d = (GYbarchart_data*)bc->user_data;
	int32 i;
	int32 lim = (n < d->n) ? n : d->n;
	for (i = 0; i < lim; i++)
	{
		int32 v = GYLimitMaxMin(0, arr[i], d->max);
		d->val[i] = v;
		if (v > d->top[i]) d->top[i] = v;
	}
	YMGUI_Obj_Invalidate(bc);
}

int32 YMGUI_BarChart_GetValue(GYOBJ bc, int32 i)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return 0;
	GYbarchart_data* d = (GYbarchart_data*)bc->user_data;
	if (i < 0 || i >= d->n) return 0;
	return d->val[i];
}

void YMGUI_BarChart_SetColorMode(GYOBJ bc, uint8 mode)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	((GYbarchart_data*)bc->user_data)->color_mode =
		(mode == GY_BARCHART_COLOR_PER_BAR) ? GY_BARCHART_COLOR_PER_BAR : GY_BARCHART_COLOR_BY_HEIGHT;
	YMGUI_Obj_Invalidate(bc);
}

int32 YMGUI_BarChart_GetColorMode(GYOBJ bc)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return 0;
	return ((GYbarchart_data*)bc->user_data)->color_mode;
}

void YMGUI_BarChart_SetGradient(GYOBJ bc, GYcolor lo, GYcolor hi)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	GYbarchart_data* d = (GYbarchart_data*)bc->user_data;
	d->c_lo = lo; d->c_hi = hi;
	seedPalette(d); //渐变端点变了,默认调色板跟着重铺(PER_BAR 未显式设色时也吃到新渐变)
	YMGUI_Obj_Invalidate(bc);
}

void YMGUI_BarChart_SetBarColor(GYOBJ bc, int32 i, GYcolor color)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	GYbarchart_data* d = (GYbarchart_data*)bc->user_data;
	if (i < 0 || i >= d->n) return;
	d->pal[i] = color;
	YMGUI_Obj_Invalidate(bc);
}

void YMGUI_BarChart_SetBarColors(GYOBJ bc, const GYcolor* arr, int32 n)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL || arr == NULL) return;
	GYbarchart_data* d = (GYbarchart_data*)bc->user_data;
	int32 i;
	int32 lim = (n < d->n) ? n : d->n;
	for (i = 0; i < lim; i++) d->pal[i] = arr[i];
	YMGUI_Obj_Invalidate(bc);
}

void YMGUI_BarChart_SetBgColor(GYOBJ bc, GYcolor bg)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	((GYbarchart_data*)bc->user_data)->bg = bg;
	YMGUI_Obj_Invalidate(bc);
}

void YMGUI_BarChart_SetTopMode(GYOBJ bc, uint8 mode)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	((GYbarchart_data*)bc->user_data)->top_mode =
		(mode == GY_BARCHART_TOP_BAR) ? GY_BARCHART_TOP_BAR : GY_BARCHART_TOP_NONE;
	YMGUI_Obj_Invalidate(bc);
}

void YMGUI_BarChart_SetTopColor(GYOBJ bc, GYcolor color)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	((GYbarchart_data*)bc->user_data)->c_top = color;
	YMGUI_Obj_Invalidate(bc);
}

void YMGUI_BarChart_SetDecay(GYOBJ bc, int32 fall, int32 top_fall)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	GYbarchart_data* d = (GYbarchart_data*)bc->user_data;
	if (fall >= 0) d->fall = fall;
	if (top_fall >= 0) d->top_fall = top_fall;
}

void YMGUI_BarChart_SetGap(GYOBJ bc, GYcoord gap)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return;
	if (gap >= 0)
	{
		((GYbarchart_data*)bc->user_data)->gap = gap;
		YMGUI_Obj_Invalidate(bc);
	}
}

uint8 YMGUI_BarChart_Tick(GYOBJ bc)
{
	gy_assert(bc && bc->user_data);
	if (bc == NULL || bc->user_data == NULL) return 0;
	GYbarchart_data* d = (GYbarchart_data*)bc->user_data;
	uint8 moved = 0;
	int32 i;
	for (i = 0; i < d->n; i++)
	{
		if (d->val[i] > 0)
		{
			d->val[i] -= d->fall;
			if (d->val[i] < 0) d->val[i] = 0;
			moved = 1;
		}
		if (d->top_mode == GY_BARCHART_TOP_BAR && d->top[i] > d->val[i])
		{
			d->top[i] -= d->top_fall;
			if (d->top[i] < d->val[i]) d->top[i] = d->val[i];
			moved = 1;
		}
	}
	if (moved)
		YMGUI_Obj_Invalidate(bc);
	return moved;
}

#endif // YMGUI_BARCHART
