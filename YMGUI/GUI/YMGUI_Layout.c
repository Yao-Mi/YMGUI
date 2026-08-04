#include "YMGUI_Layout.h"

#if YMGUI_LAYOUT

#include "YMGUI_Invalidate.h"
#include "YMGUI_Debug.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Layout.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 轻量一次性布局助手。遍历对象树写 area,再标脏一次,无持久约束、无 reflow。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.无状态:坐标算完写进现成的 area 字段,不在对象上存任何布局约束(零每对象 RAM)。
  * 2.一次性:布局变动时调用者重跑,区别于 flex/grid 的每帧 measure→arrange→reflow。
  * 3.只定位不缩放:Stack/Align 都不改子的 w/h(需要 grow/stretch 请显式设尺寸后再布局)。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief 交叉轴坐标:在 [pad, pad+avail] 区间里按 cross 摆一个 size 长的子。
  *        avail=父交叉轴内容长度(已减两侧 pad),start 贴起点、center 居中、end 贴终点。
  */
static GYcoord crossPos(GYLayoutCross cross, GYcoord pad, GYcoord avail, GYcoord size)
{
	switch (cross)
	{
	case GY_CROSS_CENTER:
		return pad + (GYcoord)((avail - size) / 2);
	case GY_CROSS_END:
		return pad + (avail - size);
	case GY_CROSS_START:
	default:
		return pad;
	}
}

void YMGUI_Layout_Stack(GYOBJ parent, GYLayoutDir dir, GYcoord gap, GYcoord pad, GYLayoutCross cross)
{
	gy_assert(parent);
	gy_log_explain(parent == NULL, GY_LOG_PtrI, "布局 Stack 父对象为空");
	if (parent == NULL)
		return;

	//交叉轴可用长度 = 父内容边(减两侧 pad);主轴游标从 pad 起
	GYcoord ver = (dir == GY_LAYOUT_VER);
	GYcoord cross_avail = (ver ? parent->area.w : parent->area.h) - (GYcoord)(2 * pad);
	GYcoord cur = pad;

	for (GYOBJ ch = parent->child_head; ch != NULL; ch = ch->sibling)
	{
		if (ch->state & GY_STATE_Hidden)
			continue;//隐藏子不占位

		if (ver)
		{
			ch->area.y = cur;
			ch->area.x = crossPos(cross, pad, cross_avail, ch->area.w);
			cur += ch->area.h + gap;
		}
		else
		{
			ch->area.x = cur;
			ch->area.y = crossPos(cross, pad, cross_avail, ch->area.h);
			cur += ch->area.w + gap;
		}
	}

	//标脏父一次:其绝对区覆盖所有子的旧+新位置,省得逐子 old/new 两次
	YMGUI_Obj_Invalidate(parent);
}

void YMGUI_Layout_Align(GYOBJ obj, GYAlign align, GYcoord pad)
{
	gy_assert(obj);
	gy_log_explain(obj == NULL, GY_LOG_PtrI, "布局 Align 对象为空");
	if (obj == NULL || obj->parent == NULL)
		return;

	GYcoord bw = obj->parent->area.w;
	GYcoord bh = obj->parent->area.h;
	GYcoord ow = obj->area.w;
	GYcoord oh = obj->area.h;

	//水平:左 = pad;中 = (bw-ow)/2;右 = bw-ow-pad
	GYcoord xl = pad, xm = (GYcoord)((bw - ow) / 2), xr = bw - ow - pad;
	//垂直:上 = pad;中 = (bh-oh)/2;下 = bh-oh-pad
	GYcoord yt = pad, ym = (GYcoord)((bh - oh) / 2), yb = bh - oh - pad;

	switch (align)
	{
	case GY_ALIGN_TL: obj->area.x = xl; obj->area.y = yt; break;
	case GY_ALIGN_TM: obj->area.x = xm; obj->area.y = yt; break;
	case GY_ALIGN_TR: obj->area.x = xr; obj->area.y = yt; break;
	case GY_ALIGN_ML: obj->area.x = xl; obj->area.y = ym; break;
	case GY_ALIGN_MR: obj->area.x = xr; obj->area.y = ym; break;
	case GY_ALIGN_BL: obj->area.x = xl; obj->area.y = yb; break;
	case GY_ALIGN_BM: obj->area.x = xm; obj->area.y = yb; break;
	case GY_ALIGN_BR: obj->area.x = xr; obj->area.y = yb; break;
	case GY_ALIGN_CENTER:
	default:          obj->area.x = xm; obj->area.y = ym; break;
	}

	//标脏父(覆盖 obj 的旧+新位置)
	YMGUI_Obj_Invalidate(obj->parent);
}

#endif // YMGUI_LAYOUT
