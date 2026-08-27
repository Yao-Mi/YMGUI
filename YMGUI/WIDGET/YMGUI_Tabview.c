#include "YMGUI_Tabview.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Tabview.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-31
  *	@Description: 标签页容器。顶部 tab bar(等分分段+当前页高亮下划线)+ 下方内容区。
  *	              每 tab 一个页容器(内容区子,开 ClipChildren);切页用 SetHidden 显隐(隐藏页被 draw/hit 跳过)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.页对象是 tabview 的子(挂在内容区),AddTab 返回它给用户往里加控件。
  * 2.tab bar 点击落在 tabview 自身(页在 bar 之下),draw_cb 只画 bar,内容由各页自绘。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

//标签页私有数据
typedef struct
{
	char    titles[GY_TABVIEW_MAX_TABS][GY_TABVIEW_TAB_LEN];
	GYOBJ   pages[GY_TABVIEW_MAX_TABS];//各页内容容器
	uint16  tab_count;
	uint16  active;
	GYcoord bar_h;
	GYtabview_changed_cb changed_cb;
	GYcolor bar_bg, tab_active, tab_idle, underline;
}GYtv_data;

/**
  * @brief 单个 tab 段宽(bar 等分)
  */
static GYcoord segW(GYOBJ tv, GYtv_data* d)
{
	if (d->tab_count == 0)
		return tv->area.w;
	return (GYcoord)(tv->area.w / d->tab_count);
}

/**
  * @brief 把各页内容容器摆到内容区([bar_h, 底]),开 ClipChildren
  */
static void layoutPages(GYOBJ tv, GYtv_data* d)
{
	GYcoord ch = tv->area.h - d->bar_h;
	if (ch < 0)
		ch = 0;
	for (uint16 i = 0; i < d->tab_count; i++)
	{
		if (d->pages[i] == NULL)
			continue;
		d->pages[i]->area = (GYrect){0, d->bar_h, tv->area.w, ch};
	}
}

/**
  * @brief 绘制:只画 tab bar(内容由各页自绘)。当前页段高亮 + 底部下划线
  */
static void tvDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYtv_data* d = (GYtv_data*)obj->user_data;
	GYFONT font = &YMGUI_Font_Default;

	//bar 背景
	GYrect bar = {abs->x, abs->y, abs->w, d->bar_h};
	YMGUI_Draw_Fill(s, &bar, d->bar_bg, GY_OPA_COVER);

	GYcoord sw = segW(obj, d);
	for (uint16 i = 0; i < d->tab_count; i++)
	{
		GYcoord sx = abs->x + (GYcoord)(i * sw);
		//末段补足余宽(整除余数),避免右侧留缝
		GYcoord w = (i == d->tab_count - 1) ? (abs->w - (GYcoord)(i * sw)) : sw;
		GYrect seg = {sx, abs->y, w, d->bar_h};
		//当前页段用高亮底色
		if (i == d->active)
			YMGUI_Draw_Fill(s, &seg, d->tab_active, GY_OPA_COVER);
		else
			YMGUI_Draw_Fill(s, &seg, d->tab_idle, GY_OPA_COVER);
		//标题居中
		GYcoord tw = YMGUI_Font_TextWidth(font, d->titles[i]);
		GYcoord tx = sx + ((w > tw) ? (w - tw) / 2 : 0);
		GYcoord ty = abs->y + ((d->bar_h > font->cell_h) ? (d->bar_h - font->cell_h) / 2 : 0);
		GYcolor tc = (i == d->active) ? GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF) : GY_ARGB(0xFF, 0xB0, 0xB0, 0xB8);
		YMGUI_Draw_Text(s, font, tx, ty, d->titles[i], tc);
		//段右分隔线
		YMGUI_Draw_Line(s, sx + w - 1, abs->y, sx + w - 1, abs->y + d->bar_h - 1, GY_ARGB(0xFF, 0x40, 0x40, 0x48));
		//当前页下划线(bar 底 2px)
		if (i == d->active)
		{
			GYrect ul = {sx, abs->y + d->bar_h - 2, w, 2};
			YMGUI_Draw_Fill(s, &ul, d->underline, GY_OPA_COVER);
		}
	}
}

/**
  * @brief 事件:点击落在 tab bar 内则按 point_x 算命中 tab,切页 + 触发回调
  */
static void tvEventCb(GYOBJ obj, GYEvent e)
{
	if (e != GY_EVENT_Clicked)
		return;
	GYtv_data* d = (GYtv_data*)obj->user_data;
	if (d->tab_count == 0)
		return;
	GYrect abs;
	YMGUI_Obj_GetAbsArea(obj, &abs);
	GYcoord py = obj->ctx->point_y;
	//只有点在 bar 区(内容区点击留给页内控件)
	if (py < abs.y || py >= abs.y + d->bar_h)
		return;
	GYcoord sw = segW(obj, d);
	if (sw < 1)
		return;
	GYcoord rel = obj->ctx->point_x - abs.x;
	uint16 idx = (uint16)(rel / sw);
	if (idx >= d->tab_count)
		idx = (uint16)(d->tab_count - 1);
	if (idx == d->active)
		return;//点当前页,无变化
	YMGUI_Tabview_SetActive(obj, idx);
	if (d->changed_cb != NULL)
		d->changed_cb(obj, idx);
}

static void tvFreeCb(GYOBJ obj)
{
	//各页是 tabview 的子对象,随对象树级联释放(此处不重复释放)
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

//---- 公共 API ----
/**
  * @brief 创建标签页容器
  */
GYOBJ YMGUI_Creat_Tabview_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ tv = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(tv);
	if (tv == NULL)
		return NULL;
	GYtv_data* d = (GYtv_data*)GY_malloc0(sizeof(GYtv_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "标签页数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(tv); return NULL; }
	GY_memset(d, 0, sizeof(GYtv_data));
	d->tab_count = 0;
	d->active = 0;
	d->bar_h = 24;
	d->changed_cb = NULL;
	d->bar_bg     = GY_ARGB(0xFF, 0x1C, 0x1C, 0x24);
	d->tab_active = GY_ARGB(0xFF, 0x2E, 0x3A, 0x50);
	d->tab_idle   = GY_ARGB(0xFF, 0x22, 0x22, 0x2A);
	d->underline  = GY_ARGB(0xFF, 0x40, 0x80, 0xC0);

	tv->type = GY_OBJ_Tabview;
	tv->user_data = d;
	tv->draw_cb = tvDrawCb;
	tv->event_cb = tvEventCb;
	tv->free_cb = tvFreeCb;
	tv->bg_color = d->bar_bg;
	YMGUI_Obj_Invalidate(tv);
	return tv;
}

/**
  * @brief 加一个标签页,返回其内容容器(内容区里开 ClipChildren 的子)
  */
GYOBJ YMGUI_Tabview_AddTab(GYOBJ tabview, const char* title)
{
	gy_assert(tabview && tabview->user_data && title);
	gy_log_explain((tabview == NULL) || (tabview->user_data == NULL) || (title == NULL), GY_LOG_PtrI, "标签页/数据/标题不存在");
	GYtv_data* d = (GYtv_data*)tabview->user_data;
	if (d->tab_count >= GY_TABVIEW_MAX_TABS)
		return NULL;
	uint16 slot = d->tab_count;

	//内容区页容器:相对 tabview (0, bar_h),铺满内容区
	GYcoord ch = tabview->area.h - d->bar_h;
	if (ch < 0)
		ch = 0;
	GYOBJ page = YMGUI_Creat_Obj_Creat(tabview, 0, d->bar_h, tabview->area.w, ch);
	if (page == NULL)
		return NULL;
	page->state |= GY_STATE_ClipChildren;//页内控件裁到页区
	page->bg_color = GY_ARGB(0xFF, 0x18, 0x18, 0x20);
	//非首页初始隐藏(直接置位,尚未显示无需标脏)
	if (slot != 0)
		page->state |= GY_STATE_Hidden;

	uint16 i = 0;
	while (title[i] != '\0' && i < GY_TABVIEW_TAB_LEN - 1) { d->titles[slot][i] = title[i]; i++; }
	d->titles[slot][i] = '\0';
	d->pages[slot] = page;
	d->tab_count++;
	YMGUI_Obj_Invalidate(tabview);//bar 多一段
	return page;
}

/**
  * @brief 切到第 idx 页(越界忽略):隐藏旧页、显示新页
  */
void YMGUI_Tabview_SetActive(GYOBJ tabview, uint16 idx)
{
	gy_assert(tabview && tabview->user_data);
	gy_log_explain((tabview == NULL) || (tabview->user_data == NULL), GY_LOG_PtrI, "标签页或数据不存在");
	GYtv_data* d = (GYtv_data*)tabview->user_data;
	if (idx >= d->tab_count || idx == d->active)
	{
		if (idx < d->tab_count && idx == d->active)
			YMGUI_Obj_Invalidate(tabview);//bar 仍需重绘(可能外部调用要求刷新)
		return;
	}
	if (d->pages[d->active] != NULL)
		YMGUI_Obj_SetHidden(d->pages[d->active], 1);//隐藏旧页(标脏露出的底层)
	d->active = idx;
	if (d->pages[idx] != NULL)
		YMGUI_Obj_SetHidden(d->pages[idx], 0);//显示新页
	YMGUI_Obj_Invalidate(tabview);//bar 高亮变了
}

uint16 YMGUI_Tabview_GetActive(GYOBJ tabview)
{
	gy_assert(tabview && tabview->user_data);
	gy_log_explain((tabview == NULL) || (tabview->user_data == NULL), GY_LOG_PtrI, "标签页或数据不存在");
	return ((GYtv_data*)tabview->user_data)->active;
}

uint16 YMGUI_Tabview_GetTabCount(GYOBJ tabview)
{
	gy_assert(tabview && tabview->user_data);
	gy_log_explain((tabview == NULL) || (tabview->user_data == NULL), GY_LOG_PtrI, "标签页或数据不存在");
	return ((GYtv_data*)tabview->user_data)->tab_count;
}

GYOBJ YMGUI_Tabview_GetPage(GYOBJ tabview, uint16 idx)
{
	gy_assert(tabview && tabview->user_data);
	gy_log_explain((tabview == NULL) || (tabview->user_data == NULL), GY_LOG_PtrI, "标签页或数据不存在");
	GYtv_data* d = (GYtv_data*)tabview->user_data;
	return (idx < d->tab_count) ? d->pages[idx] : NULL;
}

/**
  * @brief 设 tab bar 高,重排各页内容区
  */
void YMGUI_Tabview_SetBarHeight(GYOBJ tabview, GYcoord bar_h)
{
	gy_assert(tabview && tabview->user_data);
	gy_log_explain((tabview == NULL) || (tabview->user_data == NULL), GY_LOG_PtrI, "标签页或数据不存在");
	GYtv_data* d = (GYtv_data*)tabview->user_data;
	if (bar_h < 0)
		bar_h = 0;
	d->bar_h = bar_h;
	layoutPages(tabview, d);
	YMGUI_Obj_Invalidate(tabview);
}

/**
  * @brief 设切页回调
  */
void YMGUI_Tabview_SetChangedCb(GYOBJ tabview, GYtabview_changed_cb cb)
{
	gy_assert(tabview && tabview->user_data);
	gy_log_explain((tabview == NULL) || (tabview->user_data == NULL), GY_LOG_PtrI, "标签页或数据不存在");
	((GYtv_data*)tabview->user_data)->changed_cb = cb;
}
