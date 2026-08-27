#include "YMGUI_Roller.h"

#if YMGUI_ROLLER

#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Roller.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 通用居中高亮平滑滚动列表。自绘型:等高文本行,选中行绘制在控件竖直正中并高亮,
  *	              其余行上下排开、越界裁掉。当前位置 cur(16.16 定点行号)每帧向 target 缓动逼近
  *	              → 平滑滚动。程序态只调 SetSelected(歌词/时间/日历);交互态拖动改滚动、抬起吸附
  *	              到最近行并触发 changed。行文本深拷进定长数组。不认识"歌词"等业务语义。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息:
  * 1.cur 是 16.16 定点"行号"(0=第0行居中),Tick 里 cur += (target-cur)/ease_div 缓动。
  * 2.绘制以控件竖直中线为锚:第 i 行中心 y = mid_y + (i - cur)*row_h。只画落在视口内的行。
  * 3.交互拖动累积像素位移换算成行号偏移改 cur;抬起把 target 吸附到 round(cur) 最近行。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define ROLLER_DRAG_THRESH 3 //拖动超此像素判为滚动(抬起不算点击吸附另算)

typedef struct
{
	char*   lines;      //行文本缓冲(count * GY_ROLLER_LINE_MAX 连续),深拷
	int32   count;      //行数
	int32   cap;        //已分配行容量

	int32   target;     //目标选中行
	GYvalue cur;        //当前位置(16.16 定点行号)
	int32   ease_div;   //缓动分母(cur += (target-cur)/ease_div),<=1 立即到位

	GYcoord row_h;      //行高
	int32   vis_rows;   //可见行数(仅供默认高度参考,绘制按视口实算)

	GYcolor bg, c_normal, c_hi, c_hi_bg;

	uint8   interactive;//交互态开关
	//拖动状态
	GYcoord drag_start_y;
	GYvalue drag_start_cur;
	GYcoord drag_moved;

	GYroller_changed_cb changed;
}GYroller_data;

//单通道线性插值:lo + (hi-lo)*num/den,钳 [0,255]
static uint8 lerpChan(uint8 lo, uint8 hi, int32 num, int32 den)
{
	if (den <= 0) return lo;
	int32 v = (int32)lo + ((int32)hi - (int32)lo) * num / den;
	return (uint8)GYLimitMaxMin(0, v, 255);
}

//按比例在两色间插值(num/den 越大越接近 hi)
static GYcolor lerpColor(GYcolor lo, GYcolor hi, int32 num, int32 den)
{
	uint8 r = lerpChan((uint8)GY_COLOR_R(lo), (uint8)GY_COLOR_R(hi), num, den);
	uint8 g = lerpChan((uint8)GY_COLOR_G(lo), (uint8)GY_COLOR_G(hi), num, den);
	uint8 b = lerpChan((uint8)GY_COLOR_B(lo), (uint8)GY_COLOR_B(hi), num, den);
	return GY_ARGB(0xFF, r, g, b);
}

//取第 i 行文本指针(越界返空串)
static const char* lineAt(GYroller_data* d, int32 i)
{
	if (i < 0 || i >= d->count || d->lines == NULL)
		return "";
	return d->lines + (size_t)i * GY_ROLLER_LINE_MAX;
}

//保证容量至少 need 行(按需扩,倍增)。失败返回 0
static uint8 ensureCap(GYroller_data* d, int32 need)
{
	if (need <= d->cap)
		return 1;
	int32 ncap = (d->cap > 0) ? d->cap : 8;
	while (ncap < need)
		ncap *= 2;
	char* nb = (char*)GY_malloc1((size_t)ncap * GY_ROLLER_LINE_MAX);
	if (nb == NULL)
		return 0;
	if (d->lines != NULL && d->count > 0)
	{
		//拷旧内容
		size_t i;
		size_t bytes = (size_t)d->count * GY_ROLLER_LINE_MAX;
		for (i = 0; i < bytes; i++)
			nb[i] = d->lines[i];
	}
	if (d->lines != NULL)
		GY_free1(d->lines);
	d->lines = nb;
	d->cap = ncap;
	return 1;
}

//深拷一行到第 i 槽(截断到 GY_ROLLER_LINE_MAX-1 字节 + '\0')
static void copyLine(GYroller_data* d, int32 i, const char* text)
{
	char* dst = d->lines + (size_t)i * GY_ROLLER_LINE_MAX;
	int32 k = 0;
	if (text != NULL)
	{
		while (text[k] != '\0' && k < GY_ROLLER_LINE_MAX - 1)
		{
			dst[k] = text[k];
			k++;
		}
	}
	dst[k] = '\0';
}

//钳选中行到 [0, count-1](空列表钳 0)
static int32 clampIdx(GYroller_data* d, int32 i)
{
	if (d->count <= 0)
		return 0;
	return GYLimitMaxMin(0, i, d->count - 1);
}

/**
  * @brief 绘制:背景 → 高亮行底 → 各可见行(以竖直中线为锚,选中行居中高亮)
  */
static void rollerDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYroller_data* d = (GYroller_data*)obj->user_data;
	GYFONT font = &YMGUI_Font_Default;

	//背景
	YMGUI_Draw_Fill(s, abs, d->bg, GY_OPA_COVER);

	//裁到自身
	GYrect self_clip;
	if (!GY_Rect_Intersect(&self_clip, abs, &s->clip))
		return;
	GYrect saved_clip = s->clip;
	s->clip = self_clip;

	GYcoord mid_y = abs->y + abs->h / 2;         //控件竖直中线
	//当前位置(定点行号)转整数部分 + 小数偏移
	//第 i 行中心 y = mid_y + (i*FP - cur)*row_h / FP
	int32 rh = d->row_h;
	if (rh <= 0) rh = 1;

	//高亮行底(画在正中一格)
	if (d->c_hi_bg != 0)
	{
		GYrect hb = {abs->x, (GYcoord)(mid_y - rh / 2), abs->w, (GYcoord)rh};
		YMGUI_Draw_Fill(s, &hb, d->c_hi_bg, GY_OPA_COVER);
	}

	if (d->count > 0)
	{
		//可见行范围:以 cur(四舍五入到最近行)为中心,视口能容纳的行 + 余量
		int32 half = abs->h / (2 * rh) + 2;
		int32 center_i = (int32)((d->cur + (GY_FP(1) / 2)) >> GY_FP_SHIFT);
		int32 i0 = center_i - half;
		int32 i1 = center_i + half;
		int32 i;
		for (i = i0; i <= i1; i++)
		{
			if (i < 0 || i >= d->count)
				continue;
			//行中心 y(定点算,避免整数误差)
			GYvalue off = GY_FP(i) - d->cur;                 //(i-cur) 定点
			GYcoord cy = (GYcoord)(mid_y + (GYcoord)((int64)off * rh >> GY_FP_SHIFT));
			GYcoord ty = (GYcoord)(cy - font->cell_h / 2);   //文字基线顶
			//亮度按"离正中线的距离"渐变:恰在中线=全高亮,±1 行外=全普通,
			//中间线性过渡 —— 滚动时高亮随内容平滑跟随,不再等缓动到位才跳行。
			GYvalue ad = (off < 0) ? -off : off;             //|i-cur| 定点
			GYcolor col;
			if (ad >= GY_FP(1))
				col = d->c_normal;
			else
			{
				//near = (1 - dist) 比例:dist=0→全 c_hi,dist=1→全 c_normal
				int32 near = (int32)(GY_FP(1) - ad);
				col = lerpColor(d->c_normal, d->c_hi, near, GY_FP(1));
			}
			const char* txt = lineAt(d, i);
			GYcoord tw = YMGUI_Font_TextWidth(font, txt);
			GYcoord tx = abs->x + (abs->w - tw) / 2;         //水平居中
			YMGUI_Draw_Text(s, font, tx, ty, txt, col);
		}
	}

	s->clip = saved_clip;
}

//触发 changed(值真变才发)
static void fireChanged(GYOBJ obj, GYroller_data* d, int32 old)
{
	if (d->changed != NULL && d->target != old)
		d->changed(obj, d->target);
}

/**
  * @brief 事件(仅交互态):按下记锚点;拖动改 cur;抬起吸附到最近行并触发 changed
  */
static void rollerEventCb(GYOBJ obj, GYEvent ev)
{
	GYroller_data* d = (GYroller_data*)obj->user_data;
	if (!d->interactive)
		return;
	GYctx* ctx = obj->ctx;
	GYcoord py = (ctx != NULL) ? ctx->point_y : 0;
	int32 rh = (d->row_h > 0) ? d->row_h : 1;

	switch (ev)
	{
	case GY_EVENT_Pressed:
		d->drag_start_y = py;
		d->drag_start_cur = d->cur;
		d->drag_moved = 0;
		break;
	case GY_EVENT_Pressing:
	{
		GYcoord delta = (GYcoord)(d->drag_start_y - py); //向上拖 = 增行号
		GYcoord ad = (delta < 0) ? (GYcoord)(-delta) : delta;
		if (ad > d->drag_moved)
			d->drag_moved = ad;
		//像素位移换算成定点行号:delta 像素 = delta/rh 行
		GYvalue cur = d->drag_start_cur + (GYvalue)(((int64)delta << GY_FP_SHIFT) / rh);
		//钳到 [0, count-1]
		GYvalue lo = 0, hi = (d->count > 0) ? GY_FP(d->count - 1) : 0;
		if (cur < lo) cur = lo;
		if (cur > hi) cur = hi;
		d->cur = cur;
		YMGUI_Obj_Invalidate(obj);
		break;
	}
	case GY_EVENT_Released:
	case GY_EVENT_ReleasedOff:
	{
		//抬起吸附到最近行(拖动才吸附;未动的点击也吸附到当前中心行=无变化)
		int32 old = d->target;
		int32 nearest = (int32)((d->cur + (GY_FP(1) / 2)) >> GY_FP_SHIFT);
		d->target = clampIdx(d, nearest);
		fireChanged(obj, d, old);
		//交给 Tick 缓动到位(target 已定)
		YMGUI_Obj_Invalidate(obj);
		break;
	}
	default:
		break;
	}
}

static void rollerFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GYroller_data* d = (GYroller_data*)obj->user_data;
		if (d->lines != NULL)
			GY_free1(d->lines);
		GY_free0(d);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建 Roller
  */
GYOBJ YMGUI_Creat_Roller_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ r = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(r);
	if (r == NULL)
		return NULL;
	GYroller_data* d = (GYroller_data*)GY_malloc0(sizeof(GYroller_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "Roller 数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(r); return NULL; }
	GY_memset(d, 0, sizeof(GYroller_data));

	d->lines    = NULL;
	d->count    = 0;
	d->cap      = 0;
	d->target   = 0;
	d->cur      = 0;
	d->ease_div = 4;
	d->row_h    = (GYcoord)(YMGUI_Font_Default.cell_h + 8);
	d->vis_rows = 5;
	d->bg       = GY_ARGB(0xFF, 0x18, 0x18, 0x20);
	d->c_normal = GY_ARGB(0xFF, 0x90, 0x90, 0x98);
	d->c_hi     = GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
	d->c_hi_bg  = 0; //默认不画高亮底
	d->interactive = 0;
	d->changed  = NULL;

	r->type = GY_OBJ_Base;
	r->user_data = d;
	r->draw_cb = rollerDrawCb;
	r->event_cb = rollerEventCb;
	r->free_cb = rollerFreeCb;
	YMGUI_Obj_Invalidate(r);
	return r;
}

void YMGUI_Roller_SetLines(GYOBJ roller, const char* const* lines, int32 n)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return;
	GYroller_data* d = (GYroller_data*)roller->user_data;
	if (n < 0) n = 0;
	if (n > 0 && !ensureCap(d, n))
		return;
	int32 i;
	for (i = 0; i < n; i++)
		copyLine(d, i, (lines != NULL) ? lines[i] : "");
	d->count  = n;
	d->target = clampIdx(d, d->target);
	d->cur    = GY_FP(d->target);
	YMGUI_Obj_Invalidate(roller);
}

int32 YMGUI_Roller_AddLine(GYOBJ roller, const char* text)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return 0;
	GYroller_data* d = (GYroller_data*)roller->user_data;
	if (!ensureCap(d, d->count + 1))
		return d->count;
	copyLine(d, d->count, text);
	d->count++;
	YMGUI_Obj_Invalidate(roller);
	return d->count;
}

void YMGUI_Roller_Clear(GYOBJ roller)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return;
	GYroller_data* d = (GYroller_data*)roller->user_data;
	d->count  = 0;
	d->target = 0;
	d->cur    = 0;
	YMGUI_Obj_Invalidate(roller);
}

int32 YMGUI_Roller_GetCount(GYOBJ roller)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return 0;
	return ((GYroller_data*)roller->user_data)->count;
}

void YMGUI_Roller_SetSelected(GYOBJ roller, int32 index, uint8 animate)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return;
	GYroller_data* d = (GYroller_data*)roller->user_data;
	int32 old = d->target;
	d->target = clampIdx(d, index);
	if (!animate || d->ease_div <= 1)
		d->cur = GY_FP(d->target);
	if (d->changed != NULL && d->target != old)
		d->changed(roller, d->target);
	YMGUI_Obj_Invalidate(roller);
}

int32 YMGUI_Roller_GetSelected(GYOBJ roller)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return 0;
	return ((GYroller_data*)roller->user_data)->target;
}

void YMGUI_Roller_SetVisibleRows(GYOBJ roller, int32 rows)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return;
	if (rows > 0)
		((GYroller_data*)roller->user_data)->vis_rows = rows;
}

void YMGUI_Roller_SetRowHeight(GYOBJ roller, GYcoord row_h)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return;
	if (row_h > 0)
	{
		((GYroller_data*)roller->user_data)->row_h = row_h;
		YMGUI_Obj_Invalidate(roller);
	}
}

void YMGUI_Roller_SetColors(GYOBJ roller, GYcolor bg, GYcolor normal, GYcolor hi, GYcolor hi_bg)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return;
	GYroller_data* d = (GYroller_data*)roller->user_data;
	d->bg = bg;
	d->c_normal = normal;
	d->c_hi = hi;
	d->c_hi_bg = hi_bg;
	YMGUI_Obj_Invalidate(roller);
}

void YMGUI_Roller_SetInteractive(GYOBJ roller, uint8 on)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return;
	((GYroller_data*)roller->user_data)->interactive = on ? 1 : 0;
}

void YMGUI_Roller_SetEaseDiv(GYOBJ roller, int32 div)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return;
	((GYroller_data*)roller->user_data)->ease_div = div;
}

void YMGUI_Roller_SetChanged(GYOBJ roller, GYroller_changed_cb cb)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return;
	((GYroller_data*)roller->user_data)->changed = cb;
}

uint8 YMGUI_Roller_Tick(GYOBJ roller)
{
	gy_assert(roller && roller->user_data);
	if (roller == NULL || roller->user_data == NULL)
		return 0;
	GYroller_data* d = (GYroller_data*)roller->user_data;
	//交互拖动进行中:cur 由指针实时驱动,别让缓动把它往旧 target 拉回(否则原地上下跳)。
	//抬起(Released)才把 target 吸附到最近行,之后 Tick 再缓动到位。
	if (d->interactive && (roller->state & GY_STATE_Pressed))
		return 1;
	GYvalue goal = GY_FP(d->target);
	GYvalue diff = goal - d->cur;
	if (diff == 0)
		return 0;
	if (d->ease_div <= 1)
	{
		d->cur = goal;
	}
	else
	{
		GYvalue step = diff / d->ease_div;
		//保证每帧至少移动 1/256 行,避免尾巴无限逼近不到位
		if (step == 0)
			step = (diff > 0) ? (GY_FP(1) >> 8) : -(GY_FP(1) >> 8);
		d->cur += step;
		//越过则贴住
		if ((diff > 0 && d->cur > goal) || (diff < 0 && d->cur < goal))
			d->cur = goal;
	}
	YMGUI_Obj_Invalidate(roller);
	return (d->cur != goal) ? 1 : 0;
}

#endif // YMGUI_ROLLER
