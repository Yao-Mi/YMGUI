#include "YMGUI_Table.h"
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
  *	@FileName:    YMGUI_Table.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-31
  *	@Description: 表格。固定列 + 可滚动行体,自绘型(单 draw_cb 画全部单元格,行数据内部链表存)。
  *	              复用 Chart 网格 / List 拖动滚动 / ClipChildren 裁剪(表体裁到表头之下)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.表头固定(sticky)在顶部;行体在表头之下、按 scroll_y 上下滚,裁剪到表体矩形。
  * 2.拖动滚动带阈值:指针移动超阈值判为滚动 → 抬起不算行点击(区分选中与滚动)。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define DRAG_THRESH 4 //拖动超过此像素则本次不算行点击(判为滚动)

//列定义
typedef struct
{
	char    title[GY_TABLE_HEAD_LEN];
	GYcoord width;
}GYtbl_col;

//行节点(链表;各格文字)
typedef struct GYtbl_row
{
	struct GYtbl_row* next;
	char cells[GY_TABLE_MAX_COLS][GY_TABLE_CELL_LEN];
}GYtbl_row;

//表格私有数据
typedef struct
{
	GYtbl_col  cols[GY_TABLE_MAX_COLS];
	uint16     col_count;
	GYtbl_row* row_head;   //行链表头
	GYtbl_row* row_tail;   //尾(AddRow O(1) 追加)
	uint16     row_count;
	GYcoord    row_h, head_h;
	GYcoord    scroll_y;   //行体滚动偏移
	int32      selected;   //选中行(-1 无)
	GYtable_row_cb row_cb;
	//拖动状态
	GYcoord    drag_start_y, drag_start_scr;
	GYcoord    drag_moved;  //本次按下累计位移(判滚动/点击)
	GYcolor    bg, grid, head_bg, sel_bg;
}GYtbl_data;

/**
  * @brief 取第 idx 行节点(越界返回 NULL)
  */
static GYtbl_row* rowAt(GYtbl_data* d, uint16 idx)
{
	if (idx >= d->row_count)
		return NULL;
	GYtbl_row* r = d->row_head;
	for (uint16 i = 0; i < idx && r != NULL; i++)
		r = r->next;
	return r;
}

/**
  * @brief 行体内容总高 = 行数 * 行高
  */
static GYcoord contentH(GYtbl_data* d)
{
	return (GYcoord)((int32)d->row_count * d->row_h);
}

/**
  * @brief 行体视口高 = 控件高 - 表头高(下限 0)
  */
static GYcoord bodyH(GYOBJ table, GYtbl_data* d)
{
	GYcoord h = table->area.h - d->head_h;
	return (h > 0) ? h : 0;
}

/**
  * @brief scroll_y 钳到 [0, max(0, 内容高 - 表体视口高)]
  */
static void clampScroll(GYOBJ table)
{
	GYtbl_data* d = (GYtbl_data*)table->user_data;
	GYcoord maxs = contentH(d) - bodyH(table, d);
	if (maxs < 0)
		maxs = 0;
	d->scroll_y = GYLimitMaxMin(0, d->scroll_y, maxs);
}

/**
  * @brief 单元格文字裁剪绘制:临时把 clip 收窄到本格,防止长文串到邻格
  */
static void drawCellText(GYSURFACE s, GYFONT font, const GYrect* cell, const char* txt, GYcolor color)
{
	if (txt[0] == '\0')
		return;
	GYrect saved = s->clip;
	GYrect cclip;
	if (GY_Rect_Intersect(&cclip, cell, &saved))
	{
		s->clip = cclip;
		GYcoord ty = cell->y + ((cell->h > font->cell_h) ? (cell->h - font->cell_h) / 2 : 0);
		YMGUI_Draw_Text(s, font, cell->x + 4, ty, txt, color);
		s->clip = saved;
	}
}

/**
  * @brief 表格绘制:背景 → 表体(裁到表头之下,画选中高亮/单元格/行线/列线) → 表头(sticky,盖在最上)
  */
static void tableDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYtbl_data* d = (GYtbl_data*)obj->user_data;
	GYFONT font = &YMGUI_Font_Default;

	//整体裁到自身区域(自绘型控件手动做 ClipChildren 的事)
	GYrect self_clip;
	if (!GY_Rect_Intersect(&self_clip, abs, &s->clip))
		return;
	GYrect saved_clip = s->clip;
	s->clip = self_clip;

	//背景
	YMGUI_Draw_Fill(s, abs, d->bg, GY_OPA_COVER);

	GYcoord body_top = abs->y + d->head_h;
	GYcoord body_h = bodyH(obj, d);
	GYcolor txt_col = GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0);

	//---- 表体:裁剪到 [body_top, body_top+body_h) ----
	GYrect body_rect = {abs->x, body_top, abs->w, body_h};
	GYrect body_clip;
	if (body_h > 0 && GY_Rect_Intersect(&body_clip, &body_rect, &self_clip))
	{
		s->clip = body_clip;
		//只画可见行:首个可见行下标 = scroll_y / row_h
		uint16 first = (d->row_h > 0) ? (uint16)(d->scroll_y / d->row_h) : 0;
		for (uint16 ri = first; ri < d->row_count; ri++)
		{
			GYcoord ry = body_top + (GYcoord)((int32)ri * d->row_h - d->scroll_y);
			if (ry >= body_top + body_h)
				break;//已滚出下沿
			GYtbl_row* row = rowAt(d, ri);
			if (row == NULL)
				break;
			//选中行高亮
			if ((int32)ri == d->selected)
			{
				GYrect selr = {abs->x, ry, abs->w, d->row_h};
				YMGUI_Draw_Fill(s, &selr, d->sel_bg, GY_OPA_COVER);
			}
			//各列单元格文字
			GYcoord cx = abs->x;
			for (uint16 ci = 0; ci < d->col_count; ci++)
			{
				GYrect cell = {cx, ry, d->cols[ci].width, d->row_h};
				drawCellText(s, font, &cell, row->cells[ci], txt_col);
				cx += d->cols[ci].width;
			}
			//行底分隔线
			YMGUI_Draw_Line(s, abs->x, ry + d->row_h - 1, abs->x + abs->w - 1, ry + d->row_h - 1, d->grid);
		}
		//列竖线(贯穿表体)
		GYcoord cx = abs->x;
		for (uint16 ci = 0; ci < d->col_count; ci++)
		{
			cx += d->cols[ci].width;
			YMGUI_Draw_Line(s, cx, body_top, cx, body_top + body_h - 1, d->grid);
		}
		s->clip = self_clip;//恢复到自身裁剪(表头用)
	}

	//---- 表头:sticky,画在 [abs->y, abs->y+head_h),盖过滚上来的行 ----
	if (d->head_h > 0)
	{
		GYrect head_rect = {abs->x, abs->y, abs->w, d->head_h};
		YMGUI_Draw_Fill(s, &head_rect, d->head_bg, GY_OPA_COVER);
		GYcoord cx = abs->x;
		for (uint16 ci = 0; ci < d->col_count; ci++)
		{
			GYrect hcell = {cx, abs->y, d->cols[ci].width, d->head_h};
			drawCellText(s, font, &hcell, d->cols[ci].title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));
			cx += d->cols[ci].width;
			YMGUI_Draw_Line(s, cx, abs->y, cx, abs->y + d->head_h - 1, d->grid);
		}
		//表头底线
		YMGUI_Draw_Line(s, abs->x, abs->y + d->head_h - 1, abs->x + abs->w - 1, abs->y + d->head_h - 1, d->grid);
	}

	s->clip = saved_clip;//全部恢复
}

static void tableFreeCb(GYOBJ obj)
{
	GYtbl_data* d = (GYtbl_data*)obj->user_data;
	if (d != NULL)
	{
		GYtbl_row* r = d->row_head;
		while (r != NULL)
		{
			GYtbl_row* next = r->next;
			GY_free0(r);
			r = next;
		}
		GY_free0(d);
		obj->user_data = NULL;
	}
}

/**
  * @brief 指针 y(屏幕)落在表体内的哪一行;不在表体/越界返回 -1
  */
static int32 rowAtPointer(GYOBJ table, GYtbl_data* d, GYcoord py)
{
	GYrect abs;
	YMGUI_Obj_GetAbsArea(table, &abs);
	GYcoord body_top = abs.y + d->head_h;
	if (py < body_top || py >= abs.y + abs.h)
		return -1;//在表头或控件外
	int32 ri = (int32)(((int32)(py - body_top) + d->scroll_y) / d->row_h);
	if (ri < 0 || ri >= (int32)d->row_count)
		return -1;
	return ri;
}

/**
  * @brief 表格事件:按下记拖动锚点;按住移动改 scroll(累计位移);
  *        抬起(Clicked)时若未超拖动阈值则选中命中行并触发回调
  */
static void tableEventCb(GYOBJ obj, GYEvent e)
{
	GYtbl_data* d = (GYtbl_data*)obj->user_data;
	GYcoord py = obj->ctx->point_y;
	switch (e)
	{
	case GY_EVENT_Pressed:
		d->drag_start_y = py;
		d->drag_start_scr = d->scroll_y;
		d->drag_moved = 0;
		break;
	case GY_EVENT_Pressing:
	{
		GYcoord delta = (GYcoord)(d->drag_start_y - py);
		GYcoord ad = (delta < 0) ? (GYcoord)(-delta) : delta;
		if (ad > d->drag_moved)
			d->drag_moved = ad;
		d->scroll_y = d->drag_start_scr + delta;
		clampScroll(obj);
		YMGUI_Obj_Invalidate(obj);
		break;
	}
	case GY_EVENT_Clicked:
		if (d->drag_moved <= DRAG_THRESH)
		{
			int32 ri = rowAtPointer(obj, d, py);
			if (ri >= 0)
			{
				d->selected = ri;
				YMGUI_Obj_Invalidate(obj);
				if (d->row_cb != NULL)
					d->row_cb(obj, ri);
			}
		}
		break;
	default:
		break;
	}
}

//---- 公共 API ----
/**
  * @brief 创建表格
  */
GYOBJ YMGUI_Creat_Table_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ tbl = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(tbl);
	if (tbl == NULL)
		return NULL;
	GYtbl_data* d = (GYtbl_data*)GY_malloc0(sizeof(GYtbl_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "表格数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(tbl); return NULL; }
	GY_memset(d, 0, sizeof(GYtbl_data));
	d->col_count = 0;
	d->row_head = NULL;
	d->row_tail = NULL;
	d->row_count = 0;
	d->row_h = 22;
	d->head_h = 22;
	d->scroll_y = 0;
	d->selected = -1;
	d->row_cb = NULL;
	d->drag_moved = 0;
	d->bg      = GY_ARGB(0xFF, 0x1C, 0x1C, 0x24);
	d->grid    = GY_ARGB(0xFF, 0x40, 0x40, 0x48);
	d->head_bg = GY_ARGB(0xFF, 0x2A, 0x2A, 0x34);
	d->sel_bg  = GY_ARGB(0xFF, 0x35, 0x5A, 0x8A);

	tbl->type = GY_OBJ_Base;
	tbl->user_data = d;
	tbl->draw_cb = tableDrawCb;
	tbl->event_cb = tableEventCb;
	tbl->free_cb = tableFreeCb;
	tbl->bg_color = d->bg;
	YMGUI_Obj_Invalidate(tbl);
	return tbl;
}

/**
  * @brief 加一列(标题 + 列宽)
  */
int YMGUI_Table_AddColumn(GYOBJ table, const char* title, GYcoord width)
{
	gy_assert(table && table->user_data && title);
	gy_log_explain((table == NULL) || (table->user_data == NULL) || (title == NULL), GY_LOG_PtrI, "表格/数据/标题不存在");
	GYtbl_data* d = (GYtbl_data*)table->user_data;
	if (d->col_count >= GY_TABLE_MAX_COLS)
		return -1;
	uint16 slot = d->col_count;
	uint16 i = 0;
	while (title[i] != '\0' && i < GY_TABLE_HEAD_LEN - 1) { d->cols[slot].title[i] = title[i]; i++; }
	d->cols[slot].title[i] = '\0';
	d->cols[slot].width = (width > 0) ? width : 1;
	d->col_count++;
	YMGUI_Obj_Invalidate(table);
	return (int)slot;
}

/**
  * @brief 加一行(尾插;各格空串)
  */
int YMGUI_Table_AddRow(GYOBJ table)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	GYtbl_data* d = (GYtbl_data*)table->user_data;
	GYtbl_row* r = (GYtbl_row*)GY_malloc0(sizeof(GYtbl_row));
	gy_assert(r);
	gy_log_explain(r == NULL, GY_LOG_Mem0, "表格行内存申请失败");
	if (r == NULL)
		return -1;
	GY_memset(r, 0, sizeof(GYtbl_row));
	r->next = NULL;
	for (uint16 c = 0; c < GY_TABLE_MAX_COLS; c++)
		r->cells[c][0] = '\0';
	if (d->row_tail == NULL)
		d->row_head = r;
	else
		d->row_tail->next = r;
	d->row_tail = r;
	int idx = (int)d->row_count;
	d->row_count++;
	YMGUI_Obj_Invalidate(table);
	return idx;
}

/**
  * @brief 设某格文字(越界忽略)
  */
void YMGUI_Table_SetCell(GYOBJ table, uint16 row, uint16 col, const char* text)
{
	gy_assert(table && table->user_data && text);
	gy_log_explain((table == NULL) || (table->user_data == NULL) || (text == NULL), GY_LOG_PtrI, "表格/数据/文本不存在");
	GYtbl_data* d = (GYtbl_data*)table->user_data;
	if (col >= d->col_count)
		return;
	GYtbl_row* r = rowAt(d, row);
	if (r == NULL)
		return;
	uint16 i = 0;
	while (text[i] != '\0' && i < GY_TABLE_CELL_LEN - 1) { r->cells[col][i] = text[i]; i++; }
	r->cells[col][i] = '\0';
	YMGUI_Obj_Invalidate(table);
}

/**
  * @brief 读某格文字(越界返回 "")
  */
const char* YMGUI_Table_GetCell(GYOBJ table, uint16 row, uint16 col)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	GYtbl_data* d = (GYtbl_data*)table->user_data;
	if (col >= d->col_count)
		return "";
	GYtbl_row* r = rowAt(d, row);
	return (r != NULL) ? r->cells[col] : "";
}

uint16 YMGUI_Table_GetRowCount(GYOBJ table)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	return ((GYtbl_data*)table->user_data)->row_count;
}

uint16 YMGUI_Table_GetColCount(GYOBJ table)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	return ((GYtbl_data*)table->user_data)->col_count;
}

/**
  * @brief 设行高/表头高(<=0 保持原值)
  */
void YMGUI_Table_SetRowHeight(GYOBJ table, GYcoord row_h, GYcoord head_h)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	GYtbl_data* d = (GYtbl_data*)table->user_data;
	if (row_h > 0)  d->row_h = row_h;
	if (head_h >= 0) d->head_h = head_h;
	clampScroll(table);
	YMGUI_Obj_Invalidate(table);
}

/**
  * @brief 设滚动位置(钳制)
  */
void YMGUI_Table_SetScroll(GYOBJ table, GYcoord scroll_y)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	((GYtbl_data*)table->user_data)->scroll_y = scroll_y;
	clampScroll(table);
	YMGUI_Obj_Invalidate(table);
}

GYcoord YMGUI_Table_GetScroll(GYOBJ table)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	return ((GYtbl_data*)table->user_data)->scroll_y;
}

/**
  * @brief 设选中行(-1 取消,越界忽略),不触发回调
  */
void YMGUI_Table_SetSelectedRow(GYOBJ table, int32 row)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	GYtbl_data* d = (GYtbl_data*)table->user_data;
	if (row >= (int32)d->row_count)
		return;
	d->selected = (row < 0) ? -1 : row;
	YMGUI_Obj_Invalidate(table);
}

int32 YMGUI_Table_GetSelectedRow(GYOBJ table)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	return ((GYtbl_data*)table->user_data)->selected;
}

/**
  * @brief 设行选中回调
  */
void YMGUI_Table_SetRowCb(GYOBJ table, GYtable_row_cb cb)
{
	gy_assert(table && table->user_data);
	gy_log_explain((table == NULL) || (table->user_data == NULL), GY_LOG_PtrI, "表格或数据不存在");
	((GYtbl_data*)table->user_data)->row_cb = cb;
}
