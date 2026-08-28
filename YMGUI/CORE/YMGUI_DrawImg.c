#include "YMGUI_DrawImg.h"
#include "YMGUI_DrawPx.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Debug.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_DrawImg.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 图片 blit。native GYpx 数据直拷,可选 colorkey 透明。裁剪+band偏移
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief 把图片 blit 到 surface(左上角屏幕 x,y)
  */
void YMGUI_Draw_Img(GYSURFACE s, GYIMG img, GYcoord x, GYcoord y)
{
	gy_assert(s && s->buf && img && img->data);
	gy_log_explain((s == NULL) || (img == NULL), GY_LOG_PtrI, "表面或图片不存在");

	//图片占用屏幕矩形,先与裁剪区求交(只遍历可见部分)
	GYrect imgr = {x, y, img->w, img->h};
	GYrect vis;
	if (!GY_Rect_Intersect(&vis, &imgr, &s->clip))
		return;

	for (GYcoord sy = vis.y; sy < vis.y + vis.h; sy++)
	{
		GYcoord iy = sy - y;//图片内行
		const GYpx* srow = img->data + (int32)iy * img->w;
		for (GYcoord sx = vis.x; sx < vis.x + vis.w; sx++)
		{
			GYcoord ix = sx - x;//图片内列
			GYpx p = srow[ix];
			if (img->use_key && GY_PxEqual(p, img->key))
				continue;//透明色跳过
			GY_PutPx(s, sx, sy, p);
		}
	}
}

/**
  * @brief 把图片缩放 blit 到目标屏幕矩形 dst(定点最近邻)
  *   遍历 dst∩clip 的每个屏幕像素,反算源像素:
  *     ix = (sx - dst.x) * src_w / dst.w  (dst.h 同理)
  *   用 16.16 定点步进(step = (src<<16)/dst),避免每像素乘除;累加取整。
  */
void YMGUI_Draw_ImgScaled(GYSURFACE s, GYIMG img, GYrect dst)
{
	gy_assert(s && s->buf && img && img->data);
	gy_log_explain((s == NULL) || (img == NULL), GY_LOG_PtrI, "表面或图片不存在");
	if (s == NULL || img == NULL || img->data == NULL)
		return;
	if (dst.w <= 0 || dst.h <= 0 || img->w <= 0 || img->h <= 0)
		return;

	//目标矩形先与裁剪区求交(只遍历可见部分)
	GYrect vis;
	if (!GY_Rect_Intersect(&vis, &dst, &s->clip))
		return;

	//16.16 定点采样步长:每前进一个目标像素,源坐标推进 step
	int32 stepx = (int32)(((int64)img->w << 16) / dst.w);
	int32 stepy = (int32)(((int64)img->h << 16) / dst.h);

	for (GYcoord sy = vis.y; sy < vis.y + vis.h; sy++)
	{
		//源行:(sy - dst.y) 个目标像素对应的源行(定点后取整),+半步做四舍五入近似
		int32 syf = ((int32)(sy - dst.y) * (int64)stepy + (stepy >> 1)) >> 16;
		if (syf < 0) syf = 0;
		if (syf >= img->h) syf = img->h - 1;
		const GYpx* srow = img->data + (int32)syf * img->w;
		for (GYcoord sx = vis.x; sx < vis.x + vis.w; sx++)
		{
			int32 sxf = ((int32)(sx - dst.x) * (int64)stepx + (stepx >> 1)) >> 16;
			if (sxf < 0) sxf = 0;
			if (sxf >= img->w) sxf = img->w - 1;
			GYpx p = srow[sxf];
			if (img->use_key && GY_PxEqual(p, img->key))
				continue;//透明色跳过
			GY_PutPx(s, sx, sy, p);
		}
	}
}
