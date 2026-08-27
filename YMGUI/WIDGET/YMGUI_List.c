#include "YMGUI_List.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_List.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 可滚动列表。容器开 ClipChildren,条目纵向堆叠;拖动(条目或空白)改容器 scroll_y 并钳制
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.拖动滚动:按下记录锚点(指针y+当时scroll),移动时 scroll = 锚点scroll +(锚点y - 当前y)
  * 2.条目 event_cb 把 Pressed/Pressing 转发给父列表(child 捕获拖动仍能滚动)
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

//列表容器私有数据
typedef struct
{
	GYcoord content_h;      //已堆叠内容总高
	GYcoord drag_start_y;   //按下时指针 y
	GYcoord drag_start_scr; //按下时 scroll_y
}GYlist_data;

//条目私有数据
typedef struct
{
	char text[48];
}GYitem_data;

/**
  * @brief scroll 钳到 [0, max(0, content_h - 视口高)]
  */
static void clampScroll(GYOBJ list)
{
	GYlist_data* d = (GYlist_data*)list->user_data;
	GYcoord maxs = d->content_h - list->area.h;
	if (maxs < 0)
		maxs = 0;
	list->scroll_y = GYLimitMaxMin(0, list->scroll_y, maxs);
}

/**
  * @brief 拖动处理(list 自身或其条目触发都调它):按锚点更新 scroll
  */
static void listDrag(GYOBJ list, GYEvent e)
{
	GYlist_data* d = (GYlist_data*)list->user_data;
	GYcoord py = list->ctx->point_y;
	if (e == GY_EVENT_Pressed)
	{
		d->drag_start_y = py;
		d->drag_start_scr = list->scroll_y;
	}
	else if (e == GY_EVENT_Pressing)
	{
		list->scroll_y = d->drag_start_scr + (d->drag_start_y - py);
		clampScroll(list);
		YMGUI_Obj_Invalidate(list);//滚动 → 整个视口重绘
	}
}

/**
  * @brief 容器绘制:填背景(条目由子对象各自画)
  */
static void listDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	YMGUI_Draw_Fill(s, abs, obj->bg_color, GY_OPA_COVER);
}

/**
  * @brief 容器自身事件:空白处拖动也能滚
  */
static void listEventCb(GYOBJ obj, GYEvent e)
{
	if (e == GY_EVENT_Pressed || e == GY_EVENT_Pressing)
		listDrag(obj, e);
}

static void listFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL) { GY_free0(obj->user_data); obj->user_data = NULL; }
}

/**
  * @brief 条目绘制:背景条 + 左对齐文字 + 底部分隔线
  */
static void itemDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYitem_data* d = (GYitem_data*)obj->user_data;
	GYrect saved_clip = s->clip;
	GYrect item_clip;
	if (!GY_Rect_Intersect(&item_clip, abs, &saved_clip))
		return;
	s->clip = item_clip;
	YMGUI_Draw_Fill(s, abs, obj->bg_color, GY_OPA_COVER);
	GYFONT font = &YMGUI_Font_Default;
	GYcoord ty = abs->y + ((abs->h > font->cell_h) ? (abs->h - font->cell_h) / 2 : 0);
	YMGUI_Draw_Text(s, font, abs->x + 6, ty, d->text, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0));
	//分隔线(底部 1px)
	GYrect sep = {abs->x, abs->y + abs->h - 1, abs->w, 1};
	YMGUI_Draw_Fill(s, &sep, GY_ARGB(0xFF, 0x40, 0x40, 0x48), GY_OPA_COVER);
	s->clip = saved_clip;
}

/**
  * @brief 条目事件:把拖动转发给父列表(实现 child 捕获也能滚动)
  */
static void itemEventCb(GYOBJ obj, GYEvent e)
{
	if ((e == GY_EVENT_Pressed || e == GY_EVENT_Pressing) && obj->parent != NULL)
		listDrag(obj->parent, e);
}

static void itemFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL) { GY_free0(obj->user_data); obj->user_data = NULL; }
}

/**
  * @brief 创建可滚动列表容器
  */
GYOBJ YMGUI_Creat_List_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ list = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(list);
	if (list == NULL)
		return NULL;
	GYlist_data* d = (GYlist_data*)GY_malloc0(sizeof(GYlist_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "列表数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(list); return NULL; }
	GY_memset(d, 0, sizeof(GYlist_data));
	d->content_h = 0;
	d->drag_start_y = 0;
	d->drag_start_scr = 0;

	list->type = GY_OBJ_List;
	list->state |= GY_STATE_ClipChildren;//子项裁剪到视口
	list->user_data = d;
	list->draw_cb = listDrawCb;
	list->event_cb = listEventCb;
	list->free_cb = listFreeCb;
	list->bg_color = GY_ARGB(0xFF, 0x1C, 0x1C, 0x24);
	YMGUI_Obj_Invalidate(list);
	return list;
}

/**
  * @brief 加条目(纵向紧接上一条)
  */
GYOBJ YMGUI_List_AddItem(GYOBJ list, const char* text, GYcoord item_h)
{
	gy_assert(list && list->user_data && text);
	gy_log_explain((list == NULL) || (list->user_data == NULL) || (text == NULL), GY_LOG_PtrI, "列表/数据/文本不存在");
	GYlist_data* ld = (GYlist_data*)list->user_data;

	//条目对象:相对列表 (0, content_h),宽=列表宽,高=item_h
	GYOBJ item = YMGUI_Creat_Obj_Creat(list, 0, ld->content_h, list->area.w, item_h);
	if (item == NULL)
		return NULL;
	GYitem_data* id = (GYitem_data*)GY_malloc0(sizeof(GYitem_data));
	gy_assert(id);
	gy_log_explain(id == NULL, GY_LOG_Mem0, "条目数据内存申请失败");
	if (id == NULL) { YMGUI_Free_ObjFree(item); return NULL; }
	GY_memset(id, 0, sizeof(GYitem_data));
	uint16 i = 0;
	while (text[i] != '\0' && i < 47) { id->text[i] = text[i]; i++; }
	id->text[i] = '\0';

	item->user_data = id;
	item->draw_cb = itemDrawCb;
	item->event_cb = itemEventCb;
	item->free_cb = itemFreeCb;
	item->bg_color = GY_ARGB(0xFF, 0x28, 0x28, 0x30);

	ld->content_h += item_h;
	YMGUI_Obj_Invalidate(list);
	return item;
}

/**
  * @brief 设滚动位置(钳制)
  */
void YMGUI_List_SetScroll(GYOBJ list, GYcoord scroll_y)
{
	gy_assert(list && list->user_data);
	gy_log_explain((list == NULL) || (list->user_data == NULL), GY_LOG_PtrI, "列表或数据不存在");
	list->scroll_y = scroll_y;
	clampScroll(list);
	YMGUI_Obj_Invalidate(list);
}

/**
  * @brief 读滚动位置
  */
GYcoord YMGUI_List_GetScroll(GYOBJ list)
{
	gy_assert(list);
	gy_log_explain(list == NULL, GY_LOG_PtrI, "列表不存在");
	return list->scroll_y;
}
