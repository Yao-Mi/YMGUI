#include "YMGUI_Grid.h"

#if YMGUI_GRID

#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Grid.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 通用可编辑单元格网格。自绘型(单 draw_cb 画全部单元格,type 保持 GY_OBJ_Base)。
  *	              单元格级选中/编辑意图 + 二维滚动 + sticky 行列表头。复用 Table 的 sticky+clip 思路。
  *	              分层:只管显示串/选中/滚动/编辑意图,不认识公式(电子表格语义在 app 侧 excel_edit)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * 备注信息:
  * 1.表头 sticky:顶部列名行随 scroll_x 横移不随 scroll_y 纵移;左侧行号列反之;左上角块全固定。
  * 2.拖动超阈值判为二维滚动 → 抬起不算单元格点击(区分选中与滚动,同 Table)。
  * 3.单元格存扁平数组 cells[(row*col_cap+col)*CELL_LEN],容量创建期传参(不用大默认绑架 RAM)。
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define DRAG_THRESH 4 //拖动超过此像素则本次不算单元格点击(判为选区/滚动)
#define CELL_PAD    4 //单元格文字左右内边距(像素)
#define EDGE_SCROLL 12//框选拖到距视口边 <此值时自动滚屏的步进

//一个合并区(归一:r0<=r1、c0<=c1;in_use=0 表示空槽)
typedef struct { int32 r0, c0, r1, c1; uint8 in_use; } GYgrid_merge;

//网格私有数据
typedef struct
{
	uint16   row_cap, col_cap;              //实际行列数(<= MAX)
	GYcoord  col_w[GY_GRID_MAX_COLS];       //各列宽
	GYcoord  row_hs[GY_GRID_MAX_ROWS];      //各行高(每行独立)
	GYcoord  row_h, head_h, head_col_w;     //默认行高(SetRowHeight 全局设)/ 列名行高 / 行号列宽
	char*    cells;                         //扁平单元格文字:row_cap*col_cap*CELL_LEN
	uint8*   align;                         //扁平每格对齐(GY_ALIGN_*):row_cap*col_cap
	int32    sel_row, sel_col;              //活动格(选区移动端;-1 无)
	int32    anchor_row, anchor_col;        //选区锚点(-1 无);矩形选区 = [anchor..sel]
	int32    scroll_x, scroll_y;            //二维滚动(像素)
	GYgrid_merge merges[GY_GRID_MAX_MERGES];//合并区(定长)
	GYgrid_sel_cb  sel_cb;
	GYgrid_edit_cb edit_cb;
	//拖动状态
	int32    drag_start_x, drag_start_y;
	int32    drag_start_sx, drag_start_sy;
	GYcoord  drag_moved;
	uint8    drag_select;                   //本次拖动是否为"单元格区框选"(否则为表头平移)
	GYcolor  bg, grid_c, head_bg, head_fg, sel_bg, cell_fg, sel_head_bg;
}GYgrid_data;
//---- 几何助手 ----
/**
  * @brief 取 (row,col) 格的文字缓冲指针(越界返回 NULL)
  */
static char* cellAt(GYgrid_data* d, uint16 row, uint16 col)
{
	if (row >= d->row_cap || col >= d->col_cap)
		return NULL;
	return d->cells + ((size_t)row * d->col_cap + col) * GY_GRID_CELL_LEN;
}

/**
  * @brief 取 (row,col) 格的对齐字节指针(越界返回 NULL)
  */
static uint8* alignAt(GYgrid_data* d, uint16 row, uint16 col)
{
	if (row >= d->row_cap || col >= d->col_cap || d->align == NULL)
		return NULL;
	return d->align + ((size_t)row * d->col_cap + col);
}

/**
  * @brief 找到覆盖 (row,col) 的合并区槽位;无则返回 NULL
  */
static GYgrid_merge* mergeCovering(GYgrid_data* d, int32 row, int32 col)
{
	for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
	{
		GYgrid_merge* m = &d->merges[i];
		if (m->in_use && row >= m->r0 && row <= m->r1 && col >= m->c0 && col <= m->c1)
			return m;
	}
	return NULL;
}

/**
  * @brief 若 (row,col) 落在某合并区的被覆盖子格,把 row/col 吸附到该片锚点(r0,c0)。
  *        单击/双击命中合并区时用:避免选到隐藏子格,导致输入写进不可见格、且高亮判定落空。
  */
static void snapToAnchor(GYgrid_data* d, uint16* row, uint16* col)
{
	GYgrid_merge* m = mergeCovering(d, (int32)*row, (int32)*col);
	if (m != NULL) { *row = (uint16)m->r0; *col = (uint16)m->c0; }
}

/**
  * @brief 归一 (r0,c0,r1,c1) 使 r0<=r1、c0<=c1(就地)
  */
static void normRange(int32* r0, int32* c0, int32* r1, int32* c1)
{
	if (*r0 > *r1) { int32 t = *r0; *r0 = *r1; *r1 = t; }
	if (*c0 > *c1) { int32 t = *c0; *c0 = *c1; *c1 = t; }
}

/**
  * @brief 第 col 列左沿相对"单元格区左沿(不含行号列)"的像素偏移(列宽前缀和)
  */
static int32 colLeft(GYgrid_data* d, uint16 col)
{
	int32 x = 0;
	for (uint16 c = 0; c < col && c < d->col_cap; c++)
		x += d->col_w[c];
	return x;
}

/**
  * @brief 第 row 行的高(越界回退默认 row_h)
  */
static GYcoord rowH(GYgrid_data* d, uint16 row)
{
	return (row < d->row_cap) ? d->row_hs[row] : d->row_h;
}

/**
  * @brief 第 row 行顶沿相对"单元格区上沿(不含列名行)"的像素偏移(行高前缀和)
  */
static int32 rowTop(GYgrid_data* d, uint16 row)
{
	int32 y = 0;
	for (uint16 r = 0; r < row && r < d->row_cap; r++)
		y += d->row_hs[r];
	return y;
}

/**
  * @brief 内容 y 偏移 cy(已含 scroll)命中哪一行,越界钳到 [0,row_cap-1]。cy<0 → 0
  */
static int32 rowAtY(GYgrid_data* d, int32 cy)
{
	if (cy < 0) return 0;
	int32 acc = 0;
	for (uint16 r = 0; r < d->row_cap; r++)
	{
		if (cy < acc + d->row_hs[r]) return (int32)r;
		acc += d->row_hs[r];
	}
	return (int32)d->row_cap - 1;
}

/**
  * @brief 所有列总宽 / 所有行总高(内容尺寸,不含表头)
  */
static int32 contentW(GYgrid_data* d) { return colLeft(d, d->col_cap); }
static int32 contentH(GYgrid_data* d) { return rowTop(d, d->row_cap); }

/**
  * @brief 单元格视口宽/高(控件尺寸减去行号列/列名行;下限 0)
  */
static GYcoord bodyW(GYOBJ g, GYgrid_data* d)
{
	GYcoord w = g->area.w - d->head_col_w;
	return (w > 0) ? w : 0;
}
static GYcoord bodyH(GYOBJ g, GYgrid_data* d)
{
	GYcoord h = g->area.h - d->head_h;
	return (h > 0) ? h : 0;
}

/**
  * @brief scroll_x/y 钳到 [0, max(0, 内容 - 视口)]
  */
static void clampScroll(GYOBJ g)
{
	GYgrid_data* d = (GYgrid_data*)g->user_data;
	int32 maxx = contentW(d) - bodyW(g, d);
	int32 maxy = contentH(d) - bodyH(g, d);
	if (maxx < 0) maxx = 0;
	if (maxy < 0) maxy = 0;
	d->scroll_x = GYLimitMaxMin(0, d->scroll_x, maxx);
	d->scroll_y = GYLimitMaxMin(0, d->scroll_y, maxy);
}

/**
  * @brief 列名 A..Z(超过 26 列用 AA.. 但默认上限 26,单字母够);写进 out(cap>=3)
  */
static void colName(uint16 col, char* out, size_t cap)
{
	if (cap == 0) return;
	if (col < 26)
	{
		if (cap >= 2) { out[0] = (char)('A' + col); out[1] = '\0'; }
		else out[0] = '\0';
	}
	else
	{
		//双字母(AA..),留作扩展
		uint16 hi = col / 26 - 1, lo = col % 26;
		size_t i = 0;
		if (i + 1 < cap) out[i++] = (char)('A' + hi);
		if (i + 1 < cap) out[i++] = (char)('A' + lo);
		out[i] = '\0';
	}
}

/**
  * @brief 无符号整数 → 十进制串(行号,不用 sprintf 避免浮点),写进 out
  */
static void uintStr(uint32 v, char* out, size_t cap)
{
	char tmp[12];
	int n = 0;
	if (v == 0) tmp[n++] = '0';
	while (v > 0 && n < (int)sizeof(tmp)) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
	size_t i = 0;
	while (n > 0 && i + 1 < cap) out[i++] = tmp[--n];
	out[i] = '\0';
}

/**
  * @brief 在 clip 收窄到 cell 后画文字(垂直居中;水平按 align 左/中/右,内边距 CELL_PAD;防串格)
  */
static void drawCellText(GYSURFACE s, GYFONT font, const GYrect* cell, const char* txt, GYcolor color, uint8 align)
{
	if (txt == NULL || txt[0] == '\0')
		return;
	GYrect saved = s->clip;
	GYrect cclip;
	if (GY_Rect_Intersect(&cclip, cell, &saved))
	{
		s->clip = cclip;
		GYcoord ty = cell->y + ((cell->h > font->cell_h) ? (cell->h - font->cell_h) / 2 : 0);
		GYcoord tx;
		if (align == GY_ALIGN_CENTER || align == GY_ALIGN_RIGHT)
		{
			GYcoord tw = YMGUI_Font_TextWidth(font, txt);
			GYcoord avail = cell->w - 2 * CELL_PAD;
			if (align == GY_ALIGN_CENTER)
				tx = cell->x + ((avail > tw) ? CELL_PAD + (avail - tw) / 2 : CELL_PAD);
			else //RIGHT
				tx = cell->x + ((avail > tw) ? cell->w - CELL_PAD - tw : CELL_PAD);
		}
		else
			tx = cell->x + CELL_PAD;//LEFT
		YMGUI_Draw_Text(s, font, tx, ty, txt, color);
		s->clip = saved;
	}
}
//---- 绘制 ----
/**
  * @brief 网格绘制:背景 → 单元格区(裁到表头之下右) → 列名行(sticky,随 scroll_x)
  *        → 行号列(sticky,随 scroll_y) → 左上角块(全固定)。选中格高亮 + 网格线。
  */
static void gridDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYgrid_data* d = (GYgrid_data*)obj->user_data;
	GYFONT font = &YMGUI_Font_Default;

	//整体裁到自身
	GYrect self_clip;
	if (!GY_Rect_Intersect(&self_clip, abs, &s->clip))
		return;
	GYrect saved_clip = s->clip;
	s->clip = self_clip;

	YMGUI_Draw_Fill(s, abs, d->bg, GY_OPA_COVER);

	GYcoord body_x = abs->x + d->head_col_w;   //单元格区左沿(行号列之右)
	GYcoord body_y = abs->y + d->head_h;        //单元格区上沿(列名行之下)
	GYcoord bw = bodyW(obj, d);
	GYcoord bh = bodyH(obj, d);

	//===== 单元格区:裁到 [body_x,body_x+bw) x [body_y,body_y+bh) =====
	GYrect body_rect = { body_x, body_y, bw, bh };
	GYrect body_clip;
	if (bw > 0 && bh > 0 && GY_Rect_Intersect(&body_clip, &body_rect, &self_clip))
	{
		s->clip = body_clip;
		//归一选区(用于矩形高亮)
		int32 sr0 = d->anchor_row, sc0 = d->anchor_col, sr1 = d->sel_row, sc1 = d->sel_col;
		int32 has_sel = (d->sel_row >= 0 && d->sel_col >= 0);
		if (has_sel) normRange(&sr0, &sc0, &sr1, &sc1);
		//----- 第 1 趟:非合并覆盖格的高亮底 + 文字(被合并覆盖的格跳过,留给第 3 趟)-----
		for (uint16 r = 0; r < d->row_cap; r++)
		{
			GYcoord rh = d->row_hs[r];
			GYcoord ry = body_y + (GYcoord)(rowTop(d, r) - d->scroll_y);
			if (ry + rh <= body_y) continue;//滚出上沿
			if (ry >= body_y + bh)
				break;//滚出下沿
			//行内各列
			for (uint16 c = 0; c < d->col_cap; c++)
			{
				GYcoord cx = body_x + (GYcoord)(colLeft(d, c) - d->scroll_x);
				GYcoord cw = d->col_w[c];
				if (cx + cw <= body_x) continue;   //滚出左沿
				if (cx >= body_x + bw) break;       //滚出右沿(后续列更靠右)
				if (mergeCovering(d, r, c) != NULL) continue;//合并覆盖格:第 3 趟统一画
				//选区矩形高亮
				if (has_sel && (int32)r >= sr0 && (int32)r <= sr1 && (int32)c >= sc0 && (int32)c <= sc1)
				{
					GYrect selr = { cx, ry, cw, rh };
					YMGUI_Draw_Fill(s, &selr, d->sel_bg, GY_OPA_COVER);
				}
				GYrect cell = { cx, ry, cw, rh };
				uint8* ap = alignAt(d, r, c);
				drawCellText(s, font, &cell, cellAt(d, r, c), d->cell_fg, ap ? *ap : GY_ALIGN_LEFT);
			}
			//行底分隔线(贯穿可见宽)
			YMGUI_Draw_Line(s, body_x, ry + rh - 1, body_x + bw - 1, ry + rh - 1, d->grid_c);
		}
		//----- 第 2 趟:列竖线(贯穿单元格区高)-----
		for (uint16 c = 0; c <= d->col_cap; c++)
		{
			GYcoord cx = body_x + (GYcoord)(colLeft(d, c) - d->scroll_x);
			if (cx < body_x) continue;
			if (cx > body_x + bw) break;
			YMGUI_Draw_Line(s, cx, body_y, cx, body_y + bh - 1, d->grid_c);
		}
		//----- 第 3 趟:合并区(锚点格横跨整片,盖住内部网格线,画外框 + 居中文字)-----
		for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
		{
			GYgrid_merge* m = &d->merges[i];
			if (!m->in_use) continue;
			if (m->r0 >= (int32)d->row_cap || m->c0 >= (int32)d->col_cap) continue;
			GYcoord mx = body_x + (GYcoord)(colLeft(d, (uint16)m->c0) - d->scroll_x);
			GYcoord my = body_y + (GYcoord)(rowTop(d, (uint16)m->r0) - d->scroll_y);
			GYcoord mw = 0;
			for (int32 c = m->c0; c <= m->c1 && c < (int32)d->col_cap; c++) mw += d->col_w[c];
			GYcoord mh = 0;
			for (int32 r = m->r0; r <= m->r1 && r < (int32)d->row_cap; r++) mh += d->row_hs[r];
			GYrect mr = { mx, my, mw, mh };
			//选区与合并区矩形相交则整片高亮(点合并任一格都算选中),否则填背景盖内部线
			GYcolor fill = (has_sel && m->r0 <= sr1 && m->r1 >= sr0 && m->c0 <= sc1 && m->c1 >= sc0)
			               ? d->sel_bg : d->bg;
			YMGUI_Draw_Fill(s, &mr, fill, GY_OPA_COVER);
			//外框
			YMGUI_Draw_Line(s, mx, my, mx + mw - 1, my, d->grid_c);
			YMGUI_Draw_Line(s, mx, my + mh - 1, mx + mw - 1, my + mh - 1, d->grid_c);
			YMGUI_Draw_Line(s, mx, my, mx, my + mh - 1, d->grid_c);
			YMGUI_Draw_Line(s, mx + mw - 1, my, mx + mw - 1, my + mh - 1, d->grid_c);
			//文字(取锚点格,按锚点对齐)
			uint8* ap = alignAt(d, (uint16)m->r0, (uint16)m->c0);
			drawCellText(s, font, &mr, cellAt(d, (uint16)m->r0, (uint16)m->c0), d->cell_fg,
			             ap ? *ap : GY_ALIGN_LEFT);
		}
		s->clip = self_clip;
	}

	//===== 列名行(sticky 顶部,随 scroll_x 横移):[body_x, body_x+bw) x [abs->y, abs->y+head_h) =====
	GYrect chead = { body_x, abs->y, bw, d->head_h };
	GYrect chead_clip;
	if (d->head_h > 0 && bw > 0 && GY_Rect_Intersect(&chead_clip, &chead, &self_clip))
	{
		s->clip = chead_clip;
		YMGUI_Draw_Fill(s, &chead, d->head_bg, GY_OPA_COVER);
		for (uint16 c = 0; c < d->col_cap; c++)
		{
			GYcoord cx = body_x + (GYcoord)(colLeft(d, c) - d->scroll_x);
			GYcoord cw = d->col_w[c];
			if (cx + cw <= body_x) continue;
			if (cx >= body_x + bw) break;
			char nm[4];
			colName(c, nm, sizeof(nm));
			GYrect hc = { cx, abs->y, cw, d->head_h };
			GYcolor fg = ((int32)c == d->sel_col) ? d->head_fg : GY_ARGB(0xFF, 0xC8, 0xC8, 0xD0);
			//选中列表头底色微亮
			if ((int32)c == d->sel_col)
				YMGUI_Draw_Fill(s, &hc, d->sel_head_bg, GY_OPA_COVER);
			//列名居中:近似用左内边距(单字母够窄,视觉可接受)
			GYcoord tw = YMGUI_Font_TextWidth(font, nm);
			GYcoord tx = cx + ((cw > tw) ? (cw - tw) / 2 : 4);
			GYrect hcell = { cx, abs->y, cw, d->head_h };
			//列名居中绘制(裁到该表头单元格)
			GYrect saved2 = s->clip; GYrect cc;
			if (GY_Rect_Intersect(&cc, &hcell, &saved2))
			{
				s->clip = cc;
				GYcoord ty = abs->y + ((d->head_h > font->cell_h) ? (d->head_h - font->cell_h) / 2 : 0);
				YMGUI_Draw_Text(s, font, tx, ty, nm, fg);
				s->clip = saved2;
			}
			YMGUI_Draw_Line(s, cx + cw, abs->y, cx + cw, abs->y + d->head_h - 1, d->grid_c);
		}
		YMGUI_Draw_Line(s, body_x, abs->y + d->head_h - 1, body_x + bw - 1, abs->y + d->head_h - 1, d->grid_c);
		s->clip = self_clip;
	}

	//===== 行号列(sticky 左侧,随 scroll_y 纵移):[abs->x, abs->x+head_col_w) x [body_y, body_y+bh) =====
	GYrect rhead = { abs->x, body_y, d->head_col_w, bh };
	GYrect rhead_clip;
	if (d->head_col_w > 0 && bh > 0 && GY_Rect_Intersect(&rhead_clip, &rhead, &self_clip))
	{
		s->clip = rhead_clip;
		YMGUI_Draw_Fill(s, &rhead, d->head_bg, GY_OPA_COVER);
		for (uint16 r = 0; r < d->row_cap; r++)
		{
			GYcoord rh = d->row_hs[r];
			GYcoord ry = body_y + (GYcoord)(rowTop(d, r) - d->scroll_y);
			if (ry + rh <= body_y) continue;
			if (ry >= body_y + bh)
				break;
			//选区所含行 → 行号底色微亮(用归一选区行范围)
			int32 rsel0 = d->anchor_row, rsel1 = d->sel_row;
			if (rsel0 > rsel1) { int32 t = rsel0; rsel0 = rsel1; rsel1 = t; }
			uint8 row_in_sel = (d->sel_row >= 0 && (int32)r >= rsel0 && (int32)r <= rsel1);
			if (row_in_sel)
			{
				GYrect hr = { abs->x, ry, d->head_col_w, rh };
				YMGUI_Draw_Fill(s, &hr, d->sel_head_bg, GY_OPA_COVER);
			}
			char nm[12];
			uintStr((uint32)r + 1, nm, sizeof(nm));
			GYcolor fg = row_in_sel ? d->head_fg : GY_ARGB(0xFF, 0xC8, 0xC8, 0xD0);
			GYcoord tw = YMGUI_Font_TextWidth(font, nm);
			GYcoord tx = abs->x + ((d->head_col_w > tw) ? (d->head_col_w - tw) / 2 : 2);
			GYrect hcell = { abs->x, ry, d->head_col_w, rh };
			GYrect saved2 = s->clip; GYrect cc;
			if (GY_Rect_Intersect(&cc, &hcell, &saved2))
			{
				s->clip = cc;
				GYcoord ty = ry + ((rh > font->cell_h) ? (rh - font->cell_h) / 2 : 0);
				YMGUI_Draw_Text(s, font, tx, ty, nm, fg);
				s->clip = saved2;
			}
			YMGUI_Draw_Line(s, abs->x, ry + rh - 1, abs->x + d->head_col_w - 1, ry + rh - 1, d->grid_c);
		}
		YMGUI_Draw_Line(s, abs->x + d->head_col_w - 1, body_y, abs->x + d->head_col_w - 1, body_y + bh - 1, d->grid_c);
		s->clip = self_clip;
	}

	//===== 左上角块(全固定) =====
	if (d->head_h > 0 && d->head_col_w > 0)
	{
		GYrect corner = { abs->x, abs->y, d->head_col_w, d->head_h };
		GYrect cclip;
		if (GY_Rect_Intersect(&cclip, &corner, &self_clip))
		{
			s->clip = cclip;
			YMGUI_Draw_Fill(s, &corner, d->head_bg, GY_OPA_COVER);
			YMGUI_Draw_Line(s, abs->x + d->head_col_w - 1, abs->y, abs->x + d->head_col_w - 1, abs->y + d->head_h - 1, d->grid_c);
			YMGUI_Draw_Line(s, abs->x, abs->y + d->head_h - 1, abs->x + d->head_col_w - 1, abs->y + d->head_h - 1, d->grid_c);
			s->clip = self_clip;
		}
	}

	s->clip = saved_clip;
}
//---- 命中/事件 ----
/**
  * @brief 屏幕点 (px,py) 落在哪个单元格 → 写 *row/*col 并返回 1;不在单元格区返回 0
  */
static uint8 cellAtPointer(GYOBJ g, GYgrid_data* d, GYcoord px, GYcoord py, uint16* row, uint16* col)
{
	GYrect abs;
	YMGUI_Obj_GetAbsArea(g, &abs);
	GYcoord body_x = abs.x + d->head_col_w;
	GYcoord body_y = abs.y + d->head_h;
	if (px < body_x || px >= abs.x + abs.w || py < body_y || py >= abs.y + abs.h)
		return 0;//在表头或控件外
	int32 ry = (int32)(py - body_y) + d->scroll_y;
	if (ry < 0 || ry >= contentH(d))
		return 0;//落在最后一行之下的空白
	int32 r = rowAtY(d, ry);
	int32 cx = (int32)(px - body_x) + d->scroll_x;
	int32 acc = 0;
	int32 cfound = -1;
	for (uint16 c = 0; c < d->col_cap; c++)
	{
		if (cx >= acc && cx < acc + d->col_w[c]) { cfound = c; break; }
		acc += d->col_w[c];
	}
	if (cfound < 0)
		return 0;
	*row = (uint16)r;
	*col = (uint16)cfound;
	return 1;
}

/**
  * @brief 是否落在单元格区(行号列之右、列名行之下、控件内)。框选/平移分流用。
  */
static uint8 pointInBody(GYOBJ g, GYgrid_data* d, GYcoord px, GYcoord py)
{
	GYrect abs;
	YMGUI_Obj_GetAbsArea(g, &abs);
	GYcoord body_x = abs.x + d->head_col_w;
	GYcoord body_y = abs.y + d->head_h;
	return (px >= body_x && px < abs.x + abs.w && py >= body_y && py < abs.y + abs.h);
}

/**
  * @brief 把屏幕点钳到最近的合法单元格(框选时指针越出视口也能持续扩选)。恒返回合法 row/col。
  */
static void cellAtPointerClamp(GYOBJ g, GYgrid_data* d, GYcoord px, GYcoord py, uint16* row, uint16* col)
{
	GYrect abs;
	YMGUI_Obj_GetAbsArea(g, &abs);
	GYcoord body_x = abs.x + d->head_col_w;
	GYcoord body_y = abs.y + d->head_h;
	int32 ry = (int32)(py - body_y) + d->scroll_y;
	int32 r = rowAtY(d, ry);
	int32 cx = (int32)(px - body_x) + d->scroll_x;
	int32 acc = 0, cfound = (int32)d->col_cap - 1;
	if (cx < 0) cfound = 0;
	else for (uint16 c = 0; c < d->col_cap; c++)
	{
		if (cx < acc + d->col_w[c]) { cfound = c; break; }
		acc += d->col_w[c];
	}
	*row = (uint16)r;
	*col = (uint16)cfound;
}

static void gridEventCb(GYOBJ obj, GYEvent e)
{
	GYgrid_data* d = (GYgrid_data*)obj->user_data;
	GYcoord px = obj->ctx->point_x;
	GYcoord py = obj->ctx->point_y;
	switch (e)
	{
	case GY_EVENT_Pressed:
		d->drag_start_x = px;
		d->drag_start_y = py;
		d->drag_start_sx = d->scroll_x;
		d->drag_start_sy = d->scroll_y;
		d->drag_moved = 0;
		//单元格区起手 → 框选;表头(行号列/列名行)起手 → 平移滚动(保留鼠标滚动通道)
		d->drag_select = pointInBody(obj, d, px, py);
		break;
	case GY_EVENT_Pressing:
	{
		int32 dx = d->drag_start_x - px;
		int32 dy = d->drag_start_y - py;
		int32 adx = (dx < 0) ? -dx : dx;
		int32 ady = (dy < 0) ? -dy : dy;
		GYcoord amax = (GYcoord)((adx > ady) ? adx : ady);
		if (amax > d->drag_moved)
			d->drag_moved = amax;
		if (d->drag_select)
		{
			//框选:活动格跟指针(钳到合法格),锚点不动
			if (d->anchor_row < 0 || d->anchor_col < 0)
			{
				uint16 ar, ac;
				cellAtPointerClamp(obj, d, d->drag_start_x, d->drag_start_y, &ar, &ac);
				d->anchor_row = ar; d->anchor_col = ac;
			}
			uint16 r, c;
			cellAtPointerClamp(obj, d, px, py, &r, &c);
			d->sel_row = r; d->sel_col = c;
			//指针接近视口边 → 自动滚屏,使活动格入视
			YMGUI_Grid_EnsureVisible(obj, r, c);
			YMGUI_Obj_Invalidate(obj);
		}
		else
		{
			//表头拖拽:平移滚动
			d->scroll_x = d->drag_start_sx + dx;
			d->scroll_y = d->drag_start_sy + dy;
			clampScroll(obj);
			YMGUI_Obj_Invalidate(obj);
		}
		break;
	}
	case GY_EVENT_Clicked:
		if (d->drag_moved <= DRAG_THRESH)
		{
			uint16 r, c;
			if (cellAtPointer(obj, d, px, py, &r, &c))
			{
				//命中合并区子格 → 吸附到锚点(否则选到隐藏格,输入写进不可见格)
				snapToAnchor(d, &r, &c);
				//单击 = 单格选区(锚点=活动格)
				d->sel_row = r; d->sel_col = c;
				d->anchor_row = r; d->anchor_col = c;
				YMGUI_Obj_Invalidate(obj);
				if (d->sel_cb != NULL)
					d->sel_cb(obj, r, c);
			}
		}
		else if (d->drag_select && d->sel_cb != NULL && d->sel_row >= 0)
		{
			//框选结束:回调报活动格(app 可据 GetSelectedRange 取整片)
			d->sel_cb(obj, (uint16)d->sel_row, (uint16)d->sel_col);
		}
		break;
	case GY_EVENT_DoubleClicked:
	{
		uint16 r, c;
		if (cellAtPointer(obj, d, px, py, &r, &c))
		{
			snapToAnchor(d, &r, &c);//命中合并区 → 编辑锚点格
			d->sel_row = r; d->sel_col = c;
			d->anchor_row = r; d->anchor_col = c;
			YMGUI_Obj_Invalidate(obj);
			if (d->edit_cb != NULL)
				d->edit_cb(obj, r, c);
		}
		break;
	}
	case GY_EVENT_Key:
	{
		//方向键移动活动格(仅在已有选中时);Shift+方向扩选;回车请求编辑
		uint32 k = obj->ctx->last_key;
		if (d->sel_row < 0 || d->sel_col < 0)
			break;
		int32 nr = d->sel_row, nc = d->sel_col;
		uint8 shift = 0;
		switch (k)
		{
		case GY_KEY_LEFT:  case GY_KEY_SHIFT_LEFT:  if (nc > 0)                     nc--; shift = (k == GY_KEY_SHIFT_LEFT);  break;
		case GY_KEY_RIGHT: case GY_KEY_SHIFT_RIGHT: if (nc < (int32)d->col_cap - 1) nc++; shift = (k == GY_KEY_SHIFT_RIGHT); break;
		case GY_KEY_UP:    case GY_KEY_SHIFT_UP:    if (nr > 0)                     nr--; shift = (k == GY_KEY_SHIFT_UP);    break;
		case GY_KEY_DOWN:  case GY_KEY_SHIFT_DOWN:  if (nr < (int32)d->row_cap - 1) nr++; shift = (k == GY_KEY_SHIFT_DOWN);  break;
		case GY_KEY_ENTER:
			obj->ctx->key_handled = 1;
			if (d->edit_cb != NULL)
				d->edit_cb(obj, (uint16)d->sel_row, (uint16)d->sel_col);
			return;
		default:
			return;
		}
		d->sel_row = nr;
		d->sel_col = nc;
		if (!shift) { d->anchor_row = nr; d->anchor_col = nc; }//非扩选:锚点跟随(单格)
		obj->ctx->key_handled = 1;
		YMGUI_Grid_EnsureVisible(obj, (uint16)nr, (uint16)nc);
		YMGUI_Obj_Invalidate(obj);
		break;
	}
	default:
		break;
	}
}

static void gridFreeCb(GYOBJ obj)
{
	GYgrid_data* d = (GYgrid_data*)obj->user_data;
	if (d != NULL)
	{
		if (d->cells != NULL)
			GY_free1(d->cells);
		if (d->align != NULL)
			GY_free1(d->align);
		GY_free0(d);
		obj->user_data = NULL;
	}
}
//===========================================================================
// 公共 API
//===========================================================================
GYOBJ YMGUI_Creat_Grid_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h,
                             uint16 rows, uint16 cols)
{
	GYOBJ g = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(g);
	if (g == NULL)
		return NULL;
	GYgrid_data* d = (GYgrid_data*)GY_malloc0(sizeof(GYgrid_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "网格数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(g); return NULL; }

	if (rows == 0) rows = 1;
	if (cols == 0) cols = 1;
	if (rows > GY_GRID_MAX_ROWS) rows = GY_GRID_MAX_ROWS;
	if (cols > GY_GRID_MAX_COLS) cols = GY_GRID_MAX_COLS;
	d->row_cap = rows;
	d->col_cap = cols;

	size_t nbytes = (size_t)rows * cols * GY_GRID_CELL_LEN;
	d->cells = (char*)GY_malloc1(nbytes);
	gy_assert(d->cells);
	gy_log_explain(d->cells == NULL, GY_LOG_Mem1, "网格单元格缓冲申请失败");
	if (d->cells == NULL) { GY_free0(d); YMGUI_Free_ObjFree(g); return NULL; }
	for (size_t i = 0; i < nbytes; i++)
		d->cells[i] = '\0';

	size_t nalign = (size_t)rows * cols;
	d->align = (uint8*)GY_malloc1(nalign);
	gy_assert(d->align);
	gy_log_explain(d->align == NULL, GY_LOG_Mem1, "网格对齐缓冲申请失败");
	if (d->align == NULL) { GY_free1(d->cells); GY_free0(d); YMGUI_Free_ObjFree(g); return NULL; }
	for (size_t i = 0; i < nalign; i++)
		d->align[i] = GY_ALIGN_LEFT;

	for (uint16 c = 0; c < GY_GRID_MAX_COLS; c++)
		d->col_w[c] = 72;//默认列宽
	d->row_h = 22;
	for (uint16 r = 0; r < GY_GRID_MAX_ROWS; r++)
		d->row_hs[r] = 22;//默认各行高
	d->head_h = 22;
	d->head_col_w = 40;
	d->sel_row = -1;
	d->sel_col = -1;
	d->anchor_row = -1;
	d->anchor_col = -1;
	d->drag_select = 0;
	for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
		d->merges[i].in_use = 0;
	d->scroll_x = 0;
	d->scroll_y = 0;
	d->sel_cb = NULL;
	d->edit_cb = NULL;
	d->drag_moved = 0;
	d->bg          = GY_ARGB(0xFF, 0x1C, 0x1C, 0x24);
	d->grid_c      = GY_ARGB(0xFF, 0x40, 0x40, 0x48);
	d->head_bg     = GY_ARGB(0xFF, 0x2A, 0x2A, 0x34);
	d->head_fg     = GY_ARGB(0xFF, 0xF0, 0xC0, 0x40);
	d->sel_bg      = GY_ARGB(0xFF, 0x35, 0x5A, 0x8A);
	d->sel_head_bg = GY_ARGB(0xFF, 0x3A, 0x4A, 0x60);
	d->cell_fg     = GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0);

	g->type = GY_OBJ_Base;
	g->state |= GY_STATE_Focusable;//可获焦,方向键移动选中格
	g->user_data = d;
	g->draw_cb = gridDrawCb;
	g->event_cb = gridEventCb;
	g->free_cb = gridFreeCb;
	g->bg_color = d->bg;
	YMGUI_Obj_Invalidate(g);
	return g;
}

void YMGUI_Grid_SetColWidth(GYOBJ grid, uint16 col, GYcoord width)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (col >= d->col_cap || width <= 0)
		return;
	d->col_w[col] = width;
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_SetRowHeight(GYOBJ grid, GYcoord row_h, GYcoord head_h)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (row_h > 0)
	{
		d->row_h = row_h;//默认值
		for (uint16 r = 0; r < d->row_cap; r++)//全局设:铺到每一行
			d->row_hs[r] = row_h;
	}
	if (head_h >= 0) d->head_h = head_h;
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_SetRowHeightAt(GYOBJ grid, uint16 row, GYcoord h)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (row >= d->row_cap || h <= 0)
		return;
	d->row_hs[row] = h;
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

GYcoord YMGUI_Grid_GetRowHeightAt(GYOBJ grid, uint16 row)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	return (row < d->row_cap) ? d->row_hs[row] : d->row_h;
}

GYcoord YMGUI_Grid_GetColWidth(GYOBJ grid, uint16 col)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	return (col < d->col_cap) ? d->col_w[col] : 0;
}

void YMGUI_Grid_SetHeadColWidth(GYOBJ grid, GYcoord w)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (w >= 0) d->head_col_w = w;
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_SetCellText(GYOBJ grid, uint16 row, uint16 col, const char* text)
{
	gy_assert(grid && grid->user_data && text);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL) || (text == NULL), GY_LOG_PtrI, "网格/数据/文本不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	char* cell = cellAt(d, row, col);
	if (cell == NULL)
		return;
	uint16 i = 0;
	while (text[i] != '\0' && i < GY_GRID_CELL_LEN - 1) { cell[i] = text[i]; i++; }
	cell[i] = '\0';
	YMGUI_Obj_Invalidate(grid);
}

const char* YMGUI_Grid_GetCellText(GYOBJ grid, uint16 row, uint16 col)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	char* cell = cellAt(d, row, col);
	return (cell != NULL) ? cell : "";
}

uint16 YMGUI_Grid_GetRowCount(GYOBJ grid)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	return ((GYgrid_data*)grid->user_data)->row_cap;
}

uint16 YMGUI_Grid_GetColCount(GYOBJ grid)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	return ((GYgrid_data*)grid->user_data)->col_cap;
}

void YMGUI_Grid_SetSelected(GYOBJ grid, int32 row, int32 col)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (row < 0 || col < 0 || row >= (int32)d->row_cap || col >= (int32)d->col_cap)
	{
		d->sel_row = -1;    d->sel_col = -1;
		d->anchor_row = -1; d->anchor_col = -1;
	}
	else
	{
		d->sel_row = row;    d->sel_col = col;
		d->anchor_row = row; d->anchor_col = col;//单格选区
	}
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_GetSelected(GYOBJ grid, int32* row, int32* col)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (row != NULL) *row = d->sel_row;
	if (col != NULL) *col = d->sel_col;
}

void YMGUI_Grid_SetSelectedRange(GYOBJ grid, int32 r0, int32 c0, int32 r1, int32 c1)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (r0 < 0 || c0 < 0 || r1 < 0 || c1 < 0 ||
	    r0 >= (int32)d->row_cap || c0 >= (int32)d->col_cap ||
	    r1 >= (int32)d->row_cap || c1 >= (int32)d->col_cap)
	{
		d->sel_row = -1;    d->sel_col = -1;
		d->anchor_row = -1; d->anchor_col = -1;
	}
	else
	{
		d->anchor_row = r0; d->anchor_col = c0;//锚点
		d->sel_row = r1;    d->sel_col = c1;   //活动格
	}
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_GetSelectedRange(GYOBJ grid, int32* r0, int32* c0, int32* r1, int32* c1)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	int32 a = d->anchor_row, b = d->anchor_col, e = d->sel_row, f = d->sel_col;
	if (a < 0 || b < 0 || e < 0 || f < 0)
	{ a = b = e = f = -1; }
	else
		normRange(&a, &b, &e, &f);
	if (r0 != NULL) *r0 = a;
	if (c0 != NULL) *c0 = b;
	if (r1 != NULL) *r1 = e;
	if (c1 != NULL) *c1 = f;
}

void YMGUI_Grid_SetScroll(GYOBJ grid, int32 scroll_x, int32 scroll_y)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	d->scroll_x = scroll_x;
	d->scroll_y = scroll_y;
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_GetScroll(GYOBJ grid, int32* scroll_x, int32* scroll_y)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (scroll_x != NULL) *scroll_x = d->scroll_x;
	if (scroll_y != NULL) *scroll_y = d->scroll_y;
}

void YMGUI_Grid_EnsureVisible(GYOBJ grid, uint16 row, uint16 col)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (row >= d->row_cap || col >= d->col_cap)
		return;
	GYcoord bw = bodyW(grid, d);
	GYcoord bh = bodyH(grid, d);
	//纵向:行 [top,bottom) 拉进 [scroll_y, scroll_y+bh)
	int32 top = rowTop(d, row);
	int32 bottom = top + d->row_hs[row];
	if (top < d->scroll_y)            d->scroll_y = top;
	else if (bottom > d->scroll_y + bh) d->scroll_y = bottom - bh;
	//横向:列 [left,right) 拉进 [scroll_x, scroll_x+bw)
	int32 left = colLeft(d, col);
	int32 right = left + d->col_w[col];
	if (left < d->scroll_x)           d->scroll_x = left;
	else if (right > d->scroll_x + bw) d->scroll_x = right - bw;
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

uint8 YMGUI_Grid_GetCellRect(GYOBJ grid, uint16 row, uint16 col, GYRECT out)
{
	gy_assert(grid && grid->user_data && out);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL) || (out == NULL), GY_LOG_PtrI, "网格/数据/出参不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (row >= d->row_cap || col >= d->col_cap)
		return 0;
	GYrect abs;
	YMGUI_Obj_GetAbsArea(grid, &abs);
	GYcoord body_x = abs.x + d->head_col_w;   //单元格区左沿(行号列之右)
	GYcoord body_y = abs.y + d->head_h;        //单元格区上沿(列名行之下)
	GYcoord bw = bodyW(grid, d);
	GYcoord bh = bodyH(grid, d);
	//若落在合并区,归一到锚点格并按整片算尺寸
	uint16 er = row, ec = col;
	GYcoord cellw = d->col_w[col], cellh = d->row_hs[row];
	GYgrid_merge* m = mergeCovering(d, row, col);
	if (m != NULL)
	{
		er = (uint16)m->r0; ec = (uint16)m->c0;
		cellw = 0;
		for (int32 c = m->c0; c <= m->c1 && c < (int32)d->col_cap; c++) cellw += d->col_w[c];
		cellh = 0;
		for (int32 r = m->r0; r <= m->r1 && r < (int32)d->row_cap; r++) cellh += d->row_hs[r];
	}
	//该格在单元格区内的屏幕矩形(减滚动)
	GYcoord cx = body_x + (GYcoord)(colLeft(d, ec) - d->scroll_x);
	GYcoord cy = body_y + (GYcoord)(rowTop(d, er) - d->scroll_y);
	GYrect cell = { cx, cy, cellw, cellh };
	GYrect body = { body_x, body_y, bw, bh };
	//与视口求交:完全滚出/被表头遮住 → 不可见
	if (bw <= 0 || bh <= 0 || !GY_Rect_Intersect(out, &cell, &body))
		return 0;
	return 1;
}

uint8 YMGUI_Grid_MergeCells(GYOBJ grid, int32 r0, int32 c0, int32 r1, int32 c1)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	normRange(&r0, &c0, &r1, &c1);
	if (r0 < 0 || c0 < 0 || r1 >= (int32)d->row_cap || c1 >= (int32)d->col_cap)
		return 0;//越界
	if (r0 == r1 && c0 == c1)
		return 0;//单格不合并
	//与任何已有合并区重叠 → 拒绝(保持简单:先取消再合)
	for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
	{
		GYgrid_merge* m = &d->merges[i];
		if (m->in_use && r0 <= m->r1 && r1 >= m->r0 && c0 <= m->c1 && c1 >= m->c0)
			return 0;
	}
	//找空槽
	for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
	{
		GYgrid_merge* m = &d->merges[i];
		if (!m->in_use)
		{
			m->r0 = r0; m->c0 = c0; m->r1 = r1; m->c1 = c1; m->in_use = 1;
			YMGUI_Obj_Invalidate(grid);
			return 1;
		}
	}
	return 0;//满额
}

void YMGUI_Grid_UnmergeAt(GYOBJ grid, uint16 row, uint16 col)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	GYgrid_merge* m = mergeCovering(d, row, col);
	if (m != NULL)
	{
		m->in_use = 0;
		YMGUI_Obj_Invalidate(grid);
	}
}

uint8 YMGUI_Grid_GetMergeAt(GYOBJ grid, uint16 row, uint16 col, int32* r0, int32* c0, int32* r1, int32* c1)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	GYgrid_merge* m = mergeCovering(d, row, col);
	if (m == NULL)
		return 0;
	if (r0 != NULL) *r0 = m->r0;
	if (c0 != NULL) *c0 = m->c0;
	if (r1 != NULL) *r1 = m->r1;
	if (c1 != NULL) *c1 = m->c1;
	return 1;
}

void YMGUI_Grid_SetCellAlign(GYOBJ grid, uint16 row, uint16 col, uint8 align)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	uint8* a = alignAt(d, row, col);
	if (a == NULL || align > GY_ALIGN_RIGHT)
		return;
	*a = align;
	YMGUI_Obj_Invalidate(grid);
}

uint8 YMGUI_Grid_GetCellAlign(GYOBJ grid, uint16 row, uint16 col)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	uint8* a = alignAt(d, row, col);
	return (a != NULL) ? *a : GY_ALIGN_LEFT;
}

//---- 插入/删除整行整列 ----
/**
  * @brief 清一格(文字置空 + 对齐回 LEFT)
  */
static void clearCell(GYgrid_data* d, uint16 row, uint16 col)
{
	char* p = cellAt(d, row, col);
	if (p != NULL) p[0] = '\0';
	uint8* a = alignAt(d, row, col);
	if (a != NULL) *a = GY_ALIGN_LEFT;
}

/**
  * @brief 把 (sr,sc) 的文字+对齐拷到 (dr,dc)
  */
static void copyCell(GYgrid_data* d, uint16 dr, uint16 dc, uint16 sr, uint16 sc)
{
	char* dp = cellAt(d, dr, dc);
	char* sp = cellAt(d, sr, sc);
	if (dp != NULL && sp != NULL)
	{
		size_t i = 0;
		while (sp[i] != '\0' && i < GY_GRID_CELL_LEN - 1) { dp[i] = sp[i]; i++; }
		dp[i] = '\0';
	}
	uint8* da = alignAt(d, dr, dc);
	uint8* sa = alignAt(d, sr, sc);
	if (da != NULL && sa != NULL) *da = *sa;
}

/**
  * @brief 丢弃退化(单格)或越界的合并区
  */
static void dropDegenerateMerges(GYgrid_data* d)
{
	for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
	{
		GYgrid_merge* m = &d->merges[i];
		if (!m->in_use) continue;
		if (m->r0 > m->r1 || m->c0 > m->c1 ||
		    (m->r0 == m->r1 && m->c0 == m->c1) ||
		    m->r0 < 0 || m->c0 < 0 ||
		    m->r1 >= (int32)d->row_cap || m->c1 >= (int32)d->col_cap)
			m->in_use = 0;
	}
}

void YMGUI_Grid_InsertRow(GYOBJ grid, uint16 at)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (at >= d->row_cap)
		return;
	//文字/对齐:自末行向下搬,末行被挤出
	for (int32 r = (int32)d->row_cap - 1; r > (int32)at; r--)
		for (uint16 c = 0; c < d->col_cap; c++)
			copyCell(d, (uint16)r, c, (uint16)(r - 1), c);
	for (uint16 c = 0; c < d->col_cap; c++)
		clearCell(d, at, c);
	//行高:同样后移,新行用默认
	for (int32 r = (int32)d->row_cap - 1; r > (int32)at; r--)
		d->row_hs[r] = d->row_hs[r - 1];
	d->row_hs[at] = d->row_h;
	//合并区:插入点之后整体下移;跨插入点的增高
	for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
	{
		GYgrid_merge* m = &d->merges[i];
		if (!m->in_use) continue;
		if (m->r0 >= (int32)at) { m->r0++; m->r1++; }
		else if (m->r1 >= (int32)at) m->r1++;
	}
	dropDegenerateMerges(d);
	d->sel_row = d->sel_col = d->anchor_row = d->anchor_col = -1;//清选区
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_DeleteRow(GYOBJ grid, uint16 at)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (at >= d->row_cap)
		return;
	//文字/对齐:自 at 向上搬,末行清空
	for (uint16 r = at; r + 1 < d->row_cap; r++)
		for (uint16 c = 0; c < d->col_cap; c++)
			copyCell(d, r, c, (uint16)(r + 1), c);
	for (uint16 c = 0; c < d->col_cap; c++)
		clearCell(d, (uint16)(d->row_cap - 1), c);
	//行高:上移,末行回默认
	for (uint16 r = at; r + 1 < d->row_cap; r++)
		d->row_hs[r] = d->row_hs[r + 1];
	d->row_hs[d->row_cap - 1] = d->row_h;
	//合并区:被删行之后整体上移;跨删除行的缩小;整片落在被删行的丢弃
	for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
	{
		GYgrid_merge* m = &d->merges[i];
		if (!m->in_use) continue;
		if (m->r0 > (int32)at) { m->r0--; m->r1--; }
		else if (m->r1 >= (int32)at) m->r1--;//跨(含起于)删除行 → 高度 -1
	}
	dropDegenerateMerges(d);
	d->sel_row = d->sel_col = d->anchor_row = d->anchor_col = -1;
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_InsertCol(GYOBJ grid, uint16 at)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (at >= d->col_cap)
		return;
	//文字/对齐:每行自末列向右搬,末列挤出
	for (uint16 r = 0; r < d->row_cap; r++)
	{
		for (int32 c = (int32)d->col_cap - 1; c > (int32)at; c--)
			copyCell(d, r, (uint16)c, r, (uint16)(c - 1));
		clearCell(d, r, at);
	}
	//列宽:后移,新列用默认 72
	for (int32 c = (int32)d->col_cap - 1; c > (int32)at; c--)
		d->col_w[c] = d->col_w[c - 1];
	d->col_w[at] = 72;
	for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
	{
		GYgrid_merge* m = &d->merges[i];
		if (!m->in_use) continue;
		if (m->c0 >= (int32)at) { m->c0++; m->c1++; }
		else if (m->c1 >= (int32)at) m->c1++;
	}
	dropDegenerateMerges(d);
	d->sel_row = d->sel_col = d->anchor_row = d->anchor_col = -1;
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_DeleteCol(GYOBJ grid, uint16 at)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	GYgrid_data* d = (GYgrid_data*)grid->user_data;
	if (at >= d->col_cap)
		return;
	for (uint16 r = 0; r < d->row_cap; r++)
	{
		for (uint16 c = at; c + 1 < d->col_cap; c++)
			copyCell(d, r, c, r, (uint16)(c + 1));
		clearCell(d, r, (uint16)(d->col_cap - 1));
	}
	for (uint16 c = at; c + 1 < d->col_cap; c++)
		d->col_w[c] = d->col_w[c + 1];
	d->col_w[d->col_cap - 1] = 72;
	for (int i = 0; i < GY_GRID_MAX_MERGES; i++)
	{
		GYgrid_merge* m = &d->merges[i];
		if (!m->in_use) continue;
		if (m->c0 > (int32)at) { m->c0--; m->c1--; }
		else if (m->c1 >= (int32)at) m->c1--;
	}
	dropDegenerateMerges(d);
	d->sel_row = d->sel_col = d->anchor_row = d->anchor_col = -1;
	clampScroll(grid);
	YMGUI_Obj_Invalidate(grid);
}

void YMGUI_Grid_SetSelectCb(GYOBJ grid, GYgrid_sel_cb cb)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	((GYgrid_data*)grid->user_data)->sel_cb = cb;
}

void YMGUI_Grid_SetEditCb(GYOBJ grid, GYgrid_edit_cb cb)
{
	gy_assert(grid && grid->user_data);
	gy_log_explain((grid == NULL) || (grid->user_data == NULL), GY_LOG_PtrI, "网格或数据不存在");
	((GYgrid_data*)grid->user_data)->edit_cb = cb;
}

#endif // YMGUI_GRID
