#include "YMGUI_TextView.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_TextView.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 多行文本视图。只读、纵向可滚,自绘型(单 draw_cb 画可见行)。持文本副本,
  *	              换行位置预算成行表(offset+字节长),只在文本/属性/宽度变化时重算。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.Multiline 开则遇 '\n' 断行;Wrap 开则再按控件宽度贪心折行(按 UTF-8 码点断,不切多字节)。
  * 2.行表随宽度变化重算(记 last_w),绘制只画可见行(scroll_y 裁剪),复用 Table 拖动滚动语义。
  * 3.折行贪心:逐码点累加宽度,超过内容宽则在上一码点边界断;单码点已超宽也强制放(至少一字)。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define DRAG_THRESH 4  //拖动超过此像素则判为滚动(本控件只读,仅用于滚动判定)
#define TV_PAD_X    4  //文字左内边距(像素)
#define LINE_CAP0   8  //行表初始容量

//一显示行:在文本副本里的起始字节偏移 + 字节长(不含结尾换行)
typedef struct
{
	uint32 off;
	uint32 len;
}GYtv_line;

//文本视图私有数据
typedef struct
{
	char*      text;       //文本副本(GY_malloc1,'\0' 结尾);NULL=空
	uint32     text_len;   //text 字节长(不含 '\0')
	GYtv_line* lines;      //行表(GY_malloc0 动态数组)
	uint16     line_count; //行数
	uint16     line_cap;   //行表容量
	GYcoord    line_h;     //行高
	GYcoord    scroll_y;   //纵向滚动偏移
	GYcoord    last_w;     //上次算行表时的控件宽(宽度变则重算)
	uint8      multiline;  //遇 '\n' 断行
	uint8      wrap;       //按宽度自动折行
	//拖动状态
	GYcoord    drag_start_y, drag_start_scr;
	GYcoord    drag_moved;
	GYcolor    bg, text_color;
}GYtv_data;

/**
  * @brief 内容宽 = 控件宽 - 左右内边距(下限 1)
  */
static GYcoord contentW(GYOBJ tv)
{
	GYcoord w = tv->area.w - TV_PAD_X * 2;
	return (w > 0) ? w : 1;
}

/**
  * @brief 行表追加一行(自动扩容;失败静默丢弃该行)
  */
static void linesPush(GYtv_data* d, uint32 off, uint32 len)
{
	if (d->line_count >= d->line_cap)
	{
		uint16 ncap = (d->line_cap == 0) ? LINE_CAP0 : (uint16)(d->line_cap * 2);
		GYtv_line* nl = (GYtv_line*)GY_realloc0(d->lines, (size_t)ncap * sizeof(GYtv_line));
		if (nl == NULL)
			return;//扩容失败:保留已有行,丢弃后续(裸机内存兜底)
		d->lines = nl;
		d->line_cap = ncap;
	}
	d->lines[d->line_count].off = off;
	d->lines[d->line_count].len = len;
	d->line_count++;
}

/**
  * @brief UTF-8 lead byte → 该码点字节数(1..4);非法/续字节按 1 处理
  */
static uint32 utf8Bytes(uint8 c)
{
	if (c < 0x80)              return 1;
	if ((c & 0xE0) == 0xC0)    return 2;
	if ((c & 0xF0) == 0xE0)    return 3;
	if ((c & 0xF8) == 0xF0)    return 4;
	return 1;
}

/**
  * @brief 把一段逻辑行 [seg, seg+seg_len) 折行后压进行表(off_base 为 seg 在 text 里的偏移)。
  *        wrap 关:整段压成一行。wrap 开:逐码点累加宽度,超内容宽则在上一码点边界断行,
  *        单码点已超宽也强制放(保证至少推进一字,不死循环)。
  */
static void emitSegment(GYtv_data* d, GYFONT font, uint32 off_base, const char* seg, uint32 seg_len, GYcoord cw)
{
	if (!d->wrap)
	{
		linesPush(d, off_base, seg_len);
		return;
	}
	uint32 line_start = 0;     //本折行段起点(相对 seg)
	uint32 i = 0;              //扫描位置(相对 seg)
	GYcoord line_w = 0;        //本折行段已累计宽
	while (i < seg_len)
	{
		uint32 nb = utf8Bytes((uint8)seg[i]);
		if (i + nb > seg_len)
			nb = 1;//截断多字节:吞一字节
		GYcoord gw = YMGUI_Font_TextWidthN(font, seg + i, nb);
		//放不下且本行已有内容 → 先断行,当前码点进下一行
		if (line_w + gw > cw && i > line_start)
		{
			linesPush(d, off_base + line_start, i - line_start);
			line_start = i;
			line_w = 0;
		}
		line_w += gw;
		i += nb;
	}
	//末段(空段也压:保证空文本/纯空行有一行占位)
	linesPush(d, off_base + line_start, seg_len - line_start);
}

/**
  * @brief 重算行表:按 multiline 切 '\n' 得逻辑行,每段再按 wrap 折行。text 为空则一行空。
  */
static void rebuildLines(GYOBJ tv)
{
	GYtv_data* d = (GYtv_data*)tv->user_data;
	GYFONT font = &YMGUI_Font_Default;
	d->line_count = 0;
	d->last_w = tv->area.w;
	if (d->text == NULL || d->text_len == 0)
	{
		linesPush(d, 0, 0);//一行空,给个占位视口
		return;
	}
	GYcoord cw = contentW(tv);
	if (d->multiline)
	{
		//按 '\n' 切逻辑行(不含换行符本身),每段再折行
		uint32 seg_start = 0;
		for (uint32 i = 0; i <= d->text_len; i++)
		{
			if (i == d->text_len || d->text[i] == '\n')
			{
				emitSegment(d, font, seg_start, d->text + seg_start, i - seg_start, cw);
				seg_start = i + 1;
			}
		}
	}
	else
	{
		//忽略 '\n',整段当一条逻辑行(交给折行处理)
		emitSegment(d, font, 0, d->text, d->text_len, cw);
	}
}

/**
  * @brief 内容总高 = 行数 * 行高
  */
static GYcoord contentH(GYtv_data* d)
{
	return (GYcoord)((int32)d->line_count * d->line_h);
}

/**
  * @brief scroll_y 钳到 [0, max(0, 内容高 - 视口高)]
  */
static void clampScroll(GYOBJ tv)
{
	GYtv_data* d = (GYtv_data*)tv->user_data;
	GYcoord maxs = contentH(d) - tv->area.h;
	if (maxs < 0)
		maxs = 0;
	d->scroll_y = GYLimitMaxMin(0, d->scroll_y, maxs);
}

/**
  * @brief 绘制:背景 → 裁到自身 → 只画可见行(首行=scroll_y/line_h,滚出下沿 break)
  */
static void tvDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYtv_data* d = (GYtv_data*)obj->user_data;
	GYFONT font = &YMGUI_Font_Default;

	//宽度变了(如布局改尺寸):重算行表 + 钳滚动
	if (d->last_w != obj->area.w)
	{
		rebuildLines(obj);
		clampScroll(obj);
	}

	//裁到自身区域(自绘型手动做 ClipChildren)
	GYrect self_clip;
	if (!GY_Rect_Intersect(&self_clip, abs, &s->clip))
		return;
	GYrect saved_clip = s->clip;
	s->clip = self_clip;

	YMGUI_Draw_Fill(s, abs, d->bg, GY_OPA_COVER);

	if (d->line_h > 0 && d->text != NULL)
	{
		uint16 first = (uint16)(d->scroll_y / d->line_h);
		for (uint16 li = first; li < d->line_count; li++)
		{
			GYcoord ly = abs->y + (GYcoord)((int32)li * d->line_h - d->scroll_y);
			if (ly >= abs->y + abs->h)
				break;//滚出下沿
			GYtv_line* ln = &d->lines[li];
			if (ln->len > 0)
				YMGUI_Draw_TextN(s, font, abs->x + TV_PAD_X, ly, d->text + ln->off, ln->len, d->text_color);
		}
	}

	s->clip = saved_clip;
}

static void tvFreeCb(GYOBJ obj)
{
	GYtv_data* d = (GYtv_data*)obj->user_data;
	if (d != NULL)
	{
		if (d->text != NULL)
			GY_free1(d->text);
		if (d->lines != NULL)
			GY_free0(d->lines);
		GY_free0(d);
		obj->user_data = NULL;
	}
}

/**
  * @brief 事件:按下记锚点;按住拖动改 scroll(累计位移);只读控件,无点击选中
  */
static void tvEventCb(GYOBJ obj, GYEvent e)
{
	GYtv_data* d = (GYtv_data*)obj->user_data;
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
	default:
		break;
	}
}

//---- 公共 API ----
/**
  * @brief 创建文本视图(空文本,Multiline 开、Wrap 关)
  */
GYOBJ YMGUI_Creat_TextView_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ tv = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(tv);
	if (tv == NULL)
		return NULL;
	GYtv_data* d = (GYtv_data*)GY_malloc0(sizeof(GYtv_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "文本视图数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(tv); return NULL; }
	d->text = NULL;
	d->text_len = 0;
	d->lines = NULL;
	d->line_count = 0;
	d->line_cap = 0;
	d->line_h = YMGUI_Font_Default.cell_h + 4;
	d->scroll_y = 0;
	d->last_w = w;
	d->multiline = 1;
	d->wrap = 0;
	d->drag_moved = 0;
	d->bg         = GY_ARGB(0xFF, 0x1C, 0x1C, 0x24);
	d->text_color = GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0);

	tv->type = GY_OBJ_Base;
	tv->user_data = d;
	tv->draw_cb = tvDrawCb;
	tv->event_cb = tvEventCb;
	tv->free_cb = tvFreeCb;
	tv->bg_color = d->bg;
	rebuildLines(tv);//空文本 → 一行占位
	YMGUI_Obj_Invalidate(tv);
	return tv;
}

/**
  * @brief 设文本(深拷;NULL/"" 清空)。重算行表 + 钳滚动 + 标脏
  */
void YMGUI_TextView_SetText(GYOBJ tv, const char* text)
{
	gy_assert(tv && tv->user_data);
	gy_log_explain((tv == NULL) || (tv->user_data == NULL), GY_LOG_PtrI, "文本视图或数据不存在");
	GYtv_data* d = (GYtv_data*)tv->user_data;
	if (d->text != NULL)
	{
		GY_free1(d->text);
		d->text = NULL;
		d->text_len = 0;
	}
	if (text != NULL && text[0] != '\0')
	{
		uint32 n = 0;
		while (text[n] != '\0')
			n++;
		d->text = (char*)GY_malloc1((size_t)n + 1);
		gy_assert(d->text);
		gy_log_explain(d->text == NULL, GY_LOG_Mem0, "文本视图文本内存申请失败");
		if (d->text != NULL)
		{
			GY_memcpy(d->text, text, (size_t)n + 1);
			d->text_len = n;
		}
	}
	d->scroll_y = 0;
	rebuildLines(tv);
	clampScroll(tv);
	YMGUI_Obj_Invalidate(tv);
}

/**
  * @brief 取当前文本(内部副本;空返回 "")
  */
const char* YMGUI_TextView_GetText(GYOBJ tv)
{
	gy_assert(tv && tv->user_data);
	gy_log_explain((tv == NULL) || (tv->user_data == NULL), GY_LOG_PtrI, "文本视图或数据不存在");
	GYtv_data* d = (GYtv_data*)tv->user_data;
	return (d->text != NULL) ? d->text : "";
}

/**
  * @brief 设 Multiline(按 '\n' 断行),变则重算行表 + 钳滚动 + 标脏
  */
void YMGUI_TextView_SetMultiline(GYOBJ tv, uint8 on)
{
	gy_assert(tv && tv->user_data);
	gy_log_explain((tv == NULL) || (tv->user_data == NULL), GY_LOG_PtrI, "文本视图或数据不存在");
	GYtv_data* d = (GYtv_data*)tv->user_data;
	uint8 nv = (on != 0) ? 1 : 0;
	if (d->multiline == nv)
		return;
	d->multiline = nv;
	rebuildLines(tv);
	clampScroll(tv);
	YMGUI_Obj_Invalidate(tv);
}

/**
  * @brief 设 Wrap(按宽度自动折行),变则重算行表 + 钳滚动 + 标脏
  */
void YMGUI_TextView_SetWrap(GYOBJ tv, uint8 on)
{
	gy_assert(tv && tv->user_data);
	gy_log_explain((tv == NULL) || (tv->user_data == NULL), GY_LOG_PtrI, "文本视图或数据不存在");
	GYtv_data* d = (GYtv_data*)tv->user_data;
	uint8 nv = (on != 0) ? 1 : 0;
	if (d->wrap == nv)
		return;
	d->wrap = nv;
	rebuildLines(tv);
	clampScroll(tv);
	YMGUI_Obj_Invalidate(tv);
}

/**
  * @brief 设行高(<=0 保持),钳滚动 + 标脏
  */
void YMGUI_TextView_SetLineHeight(GYOBJ tv, GYcoord line_h)
{
	gy_assert(tv && tv->user_data);
	gy_log_explain((tv == NULL) || (tv->user_data == NULL), GY_LOG_PtrI, "文本视图或数据不存在");
	GYtv_data* d = (GYtv_data*)tv->user_data;
	if (line_h > 0)
		d->line_h = line_h;
	clampScroll(tv);
	YMGUI_Obj_Invalidate(tv);
}

/**
  * @brief 设文字颜色,标脏
  */
void YMGUI_TextView_SetTextColor(GYOBJ tv, GYcolor color)
{
	gy_assert(tv && tv->user_data);
	gy_log_explain((tv == NULL) || (tv->user_data == NULL), GY_LOG_PtrI, "文本视图或数据不存在");
	((GYtv_data*)tv->user_data)->text_color = color;
	YMGUI_Obj_Invalidate(tv);
}

/**
  * @brief 设滚动位置(钳制),标脏
  */
void YMGUI_TextView_SetScroll(GYOBJ tv, GYcoord scroll_y)
{
	gy_assert(tv && tv->user_data);
	gy_log_explain((tv == NULL) || (tv->user_data == NULL), GY_LOG_PtrI, "文本视图或数据不存在");
	((GYtv_data*)tv->user_data)->scroll_y = scroll_y;
	clampScroll(tv);
	YMGUI_Obj_Invalidate(tv);
}

GYcoord YMGUI_TextView_GetScroll(GYOBJ tv)
{
	gy_assert(tv && tv->user_data);
	gy_log_explain((tv == NULL) || (tv->user_data == NULL), GY_LOG_PtrI, "文本视图或数据不存在");
	return ((GYtv_data*)tv->user_data)->scroll_y;
}

uint16 YMGUI_TextView_GetLineCount(GYOBJ tv)
{
	gy_assert(tv && tv->user_data);
	gy_log_explain((tv == NULL) || (tv->user_data == NULL), GY_LOG_PtrI, "文本视图或数据不存在");
	return ((GYtv_data*)tv->user_data)->line_count;
}
