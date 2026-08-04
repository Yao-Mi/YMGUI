#include "YMGUI_Geom.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Geom.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 平台无关纯几何运算(矩形求交/包含),不碰 GYsurface
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief 两矩形求交,结果写入 res。返回 1=有交集,0=无交集
  */
int GY_Rect_Intersect(GYRECT res, const GYrect* a, const GYrect* b)
{
	//用 int32 中间量避免 x+w 超 int16(32767)截断成负导致误判
	int32 x1 = GYMax((int32)a->x, (int32)b->x);
	int32 y1 = GYMax((int32)a->y, (int32)b->y);
	int32 x2 = GYMin((int32)a->x + a->w, (int32)b->x + b->w);
	int32 y2 = GYMin((int32)a->y + a->h, (int32)b->y + b->h);

	if (x2 <= x1 || y2 <= y1)
		return 0;//无交集

	res->x = (GYcoord)x1;
	res->y = (GYcoord)y1;
	res->w = (GYcoord)(x2 - x1);
	res->h = (GYcoord)(y2 - y1);
	return 1;
}

/**
  * @brief 点是否在矩形内
  */
int GY_Rect_Contains(const GYrect* r, GYcoord x, GYcoord y)
{
	return (x >= r->x) && (x < r->x + r->w) && (y >= r->y) && (y < r->y + r->h);
}
