#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawPx.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Debug.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_DrawFill.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 矩形填充图元(软件光栅化)。收屏幕坐标,裁剪到 surface.clip 后写入 draw buffer
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.opa==255 走直写(向量化友好);opa<255 走读-混合-写回(RGB565 无 alpha,alpha 只在此刻存在)
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief 在表面填充矩形区域(带裁剪与混合)
  *        像素混合内核已提升为 GY_MixPx(见 DrawPx.h),与抗锯齿图元共用
  */
void YMGUI_Draw_Fill(GYSURFACE s, const GYrect* area, GYcolor color, GYopa opa)
{
	GYrect draw_area;
	GYpx*  bufp;
	GYpx   fillpx;
	GYcoord x, y;
	//输入指针判断
	gy_assert(s && area);
	gy_log_explain((s == NULL) || (area == NULL), GY_LOG_PtrI, "表面或区域不存在");
	gy_assert(s->buf);
	gy_log_explain(s->buf == NULL, GY_LOG_PtrI, "表面缓冲区不存在");
	//覆盖度为0直接返回
	if (opa == GY_OPA_TRANSP)
		return;

	//区域裁剪:填充区 ∩ 表面裁剪区(屏幕坐标)
	if (GY_Rect_Intersect(&draw_area, area, &s->clip) == 0)
		return;//完全在裁剪区外
	//再 ∩ buf_area,防 clip 非 buf_area 子集时越界写
	if (GY_Rect_Intersect(&draw_area, &draw_area, &s->buf_area) == 0)
		return;

	bufp = (GYpx*)s->buf;
	fillpx = GY_ColorToPx(color);

	for (y = draw_area.y; y < draw_area.y + draw_area.h; y++)
	{
		//屏幕行 → buffer 行:减去 buf_area 原点
		GYcoord by = y - s->buf_area.y;
		GYpx*   row = bufp + (int32)by * s->stride;
		for (x = draw_area.x; x < draw_area.x + draw_area.w; x++)
		{
			GYcoord bx = x - s->buf_area.x;
			if (opa == GY_OPA_COVER)
				row[bx] = fillpx;                        //不透明:直写
			else
				row[bx] = GY_MixPx(row[bx], color, opa); //半透明:读-混合-写回
		}
	}
}
