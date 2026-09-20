#include "YMGUI_DrawLine.h"
#include "YMGUI_DrawPx.h"
#include "YMGUI_Debug.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_DrawLine.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: Bresenham 整数画线,水平/垂直快速路径。逐点走 GY_PutPx(裁剪+band偏移)
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief Bresenham 整数画线(线宽 1px)
  */
void YMGUI_Draw_Line(GYSURFACE s, GYcoord x1, GYcoord y1, GYcoord x2, GYcoord y2, GYcolor color)
{
	gy_assert(s && s->buf);
	gy_log_explain((s == NULL) || (s->buf == NULL), GY_LOG_PtrI, "表面或缓冲区不存在");

	GYpx px = GY_ColorToPx(color);

	//水平线快速路径
	if (y1 == y2)
	{
		GYcoord xa = (x1 < x2) ? x1 : x2;
		GYcoord xb = (x1 < x2) ? x2 : x1;
		for (GYcoord x = xa; x <= xb; x++)
			GY_PutPx(s, x, y1, px);
		return;
	}
	//垂直线快速路径
	if (x1 == x2)
	{
		GYcoord ya = (y1 < y2) ? y1 : y2;
		GYcoord yb = (y1 < y2) ? y2 : y1;
		for (GYcoord y = ya; y <= yb; y++)
			GY_PutPx(s, x1, y, px);
		return;
	}

#if YMGUI_ANTIALIAS
	//---- Wu 反走样(斜线):沿主轴步进,副轴 16.16 定点,主/邻两像素按分数分配覆盖度 ----
	int32 adx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
	int32 ady = (y2 > y1) ? (y2 - y1) : (y1 - y2);
	int    steep = (ady > adx);        //陡线:以 y 为主轴,画时把 x/y 互换回来
	GYcoord ax1 = x1, ay1 = y1, ax2 = x2, ay2 = y2;
	if (steep)                          //互换坐标,统一按"主轴=x"处理
	{
		GYcoord t;
		t = ax1; ax1 = ay1; ay1 = t;
		t = ax2; ax2 = ay2; ay2 = t;
	}
	if (ax1 > ax2)                      //保证主轴递增
	{
		GYcoord t;
		t = ax1; ax1 = ax2; ax2 = t;
		t = ay1; ay1 = ay2; ay2 = t;
	}
	int32 ddx = ax2 - ax1;             //已排序 → >0;非 h/v 线故必 !=0
	int32 ddy = ay2 - ay1;
	int32 grad = (int32)(((int64)ddy * 65536) / ddx);//负斜率不能左移负数
	int64 fy = (int64)ay1 * 65536;    //副轴累加值(16.16)，支持负坐标
	for (GYcoord mx = ax1; mx <= ax2; mx++)
	{
		int32  yi = fy >> 16;                       //副轴整数
		int32  frac = fy & 0xFFFF;                  //副轴小数
		GYopa  opaNext = (GYopa)((frac * 255) >> 16);//邻像素覆盖度
		GYopa  opaMain = (GYopa)(255 - opaNext);     //主像素:互补(frac=0→255 保持锐利)
		if (steep)                                  //陡线:主轴是 y,画时 (yi, mx)
		{
			GY_BlendPx(s, (GYcoord)yi,       mx, color, opaMain);
			GY_BlendPx(s, (GYcoord)(yi + 1), mx, color, opaNext);
		}
		else
		{
			GY_BlendPx(s, mx, (GYcoord)yi,       color, opaMain);
			GY_BlendPx(s, mx, (GYcoord)(yi + 1), color, opaNext);
		}
		fy += grad;
	}
	(void)px;//AA 路径用 color 混合,不用打包值
#else
	//---- 一般 Bresenham(整数硬边) ----
	int32 dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
	int32 dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
	int32 sx = (x1 < x2) ? 1 : -1;
	int32 sy = (y1 < y2) ? 1 : -1;
	int32 err = dx - dy;
	GYcoord x = x1, y = y1;
	for (;;)
	{
		GY_PutPx(s, x, y, px);
		if (x == x2 && y == y2)
			break;
		int32 e2 = err * 2;
		if (e2 > -dy)
		{
			err -= dy;
			x = (GYcoord)(x + sx);
		}
		if (e2 < dx)
		{
			err += dx;
			y = (GYcoord)(y + sy);
		}
	}
#endif
}
