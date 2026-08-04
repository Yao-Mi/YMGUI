#include "YMGUI_EditView.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_EditView.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 可编辑多行文本框。自绘型(单 draw_cb 画可见行 + 光标)。定容文本缓冲,永远按 '\n'
  *	              断行,Wrap 可选。聚焦时按键编辑:UTF-8 字节插入、ENTER 换行、退格/删除按整码点
  *	              (跨行合并),方向键按码点/按显示行移光标。中文正确(逐字节拼回整码点)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.行表沿用 TextView 语义:'\n' 切逻辑行(不含换行符),Wrap 开则每段再按宽度贪心折行。
  * 2.光标以字节位记录,始终落在 UTF-8 码点边界;渲染/移动都按码点或像素宽换算,支持中英混排。
  * 3.编辑后重算行表并把光标行滚入可见区;拖动滚动语义与 TextView 一致。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define EV_PAD_X    4  //文字左内边距(像素)
#define LINE_CAP0   8  //行表初始容量
#define EV_NL_MARK  6  //选中含换行时行尾高亮块宽(像素,示意"选中了换行")
#define EV_FIND_MAX 64 //查找词缓冲上限(字节)

//一显示行:在文本缓冲里的起始字节偏移 + 字节长(不含结尾换行)
typedef struct
{
	size_t off;
	size_t len;
}GYev_line;

//可编辑文本框私有数据
typedef struct
{
	char*      text;       //文本缓冲('\0' 结尾),GY_malloc1(capacity+1)
	size_t     capacity;   //可用字节数(len 上限;缓冲实占 capacity+1)
	size_t     len;        //text 字节长(不含 '\0')
	size_t     cursor;     //光标字节位置(0..len,落在码点边界)
	ptrdiff_t  sel_anchor; //选区锚点字节位(-1=无选区);选区=[min(anchor,cursor),max(...))。ptrdiff_t 配 size_t 偏移且留 -1 哨兵
	GYev_line* lines;      //行表(GY_malloc0 动态数组)
	size_t     line_count; //显示行数
	size_t     line_cap;   //行表容量
	GYcoord    line_h;     //行高
	int32      scroll_y;   //纵向滚动偏移(像素,可负;大文件像素高可超 int16,用 int32)
	GYcoord    last_w;     //上次算行表时的控件宽(宽度变则重算)
	uint8      wrap;       //按宽度自动折行
	//拖动状态
	GYcoord    drag_start_y;
	int32      drag_start_scr;//拖动起始滚动值(像素,随 scroll_y 用 int32)
	//编辑回调
	GYev_changed_cb changed;
	GYev_action_cb  find_cb;//Ctrl+F 请求(上层弹查找条)
	GYcolor    bg, text_color, sel_color, find_color, border_color;
	//单级撤销快照(改动前存一份,Ctrl+Z 回滚)。undo_text 默认分配,SetUndoEnabled(0) 置 NULL 省 RAM
	char*      undo_text;  //NULL=撤销关闭
	size_t     undo_len;
	size_t     undo_cursor;
	ptrdiff_t  undo_anchor;
	uint8      undo_valid;  //快照是否有效
	//查找词(匹配高亮用)
	char       find[EV_FIND_MAX];
	size_t     find_len;
}GYev_data;

//前置声明
static void rebuildLines(GYOBJ ev);
static void ensureCursorVisible(GYOBJ ev);
static uint8 hasSel(GYev_data* d);
static void selRange(GYev_data* d, size_t* s, size_t* e);

/**
  * @brief 内容宽 = 控件宽 - 左右内边距(下限 1)
  */
static GYcoord contentW(GYOBJ ev)
{
	GYcoord w = ev->area.w - EV_PAD_X * 2;
	return (w > 0) ? w : 1;
}

/**
  * @brief UTF-8 lead byte → 该码点字节数(1..4);非法/续字节按 1 处理
  */
static size_t utf8Bytes(uint8 c)
{
	if (c < 0x80)              return 1;
	if ((c & 0xE0) == 0xC0)    return 2;
	if ((c & 0xF0) == 0xE0)    return 3;
	if ((c & 0xF8) == 0xF0)    return 4;
	return 1;
}

/**
  * @brief 从 cursor 向前回退一个码点边界(跳过续字节),返回前一码点字节数(0=已在串首)
  */
static size_t prevCpBytes(GYev_data* d)
{
	if (d->cursor == 0)
		return 0;
	size_t i = d->cursor - 1;
	while (i > 0 && ((uint8)d->text[i] & 0xC0) == 0x80)
		i--;
	return (size_t)(d->cursor - i);
}
/**
  * @brief 行表追加一行(自动扩容;失败静默丢弃该行)
  */
static void linesPush(GYev_data* d, size_t off, size_t len)
{
	if (d->line_count >= d->line_cap)
	{
		size_t ncap = (d->line_cap == 0) ? LINE_CAP0 : (size_t)(d->line_cap * 2);
		GYev_line* nl = (GYev_line*)GY_realloc0(d->lines, (size_t)ncap * sizeof(GYev_line));
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
  * @brief 把逻辑行 [seg, seg+seg_len) 折行后压进行表(off_base 为 seg 在 text 里的偏移)。
  *        wrap 关:整段压成一行。wrap 开:逐码点累加宽度,超内容宽则在上一码点边界断,
  *        单码点已超宽也强制放(保证至少推进一字,不死循环)。
  */
static void emitSegment(GYev_data* d, GYFONT font, size_t off_base, const char* seg, size_t seg_len, GYcoord cw)
{
	if (!d->wrap)
	{
		linesPush(d, off_base, seg_len);
		return;
	}
	size_t line_start = 0;
	size_t i = 0;
	GYcoord line_w = 0;
	while (i < seg_len)
	{
		size_t nb = utf8Bytes((uint8)seg[i]);
		if (i + nb > seg_len)
			nb = 1;//截断多字节:吞一字节
		GYcoord gw = YMGUI_Font_TextWidthN(font, seg + i, nb);
		if (line_w + gw > cw && i > line_start)
		{
			linesPush(d, off_base + line_start, i - line_start);
			line_start = i;
			line_w = 0;
		}
		line_w += gw;
		i += nb;
	}
	linesPush(d, off_base + line_start, seg_len - line_start);//末段(空段也压)
}

/**
  * @brief 重算行表:永远按 '\n' 切逻辑行(不含换行符),每段再按 wrap 折行。空文本给一行占位
  */
static void rebuildLines(GYOBJ ev)
{
	GYev_data* d = (GYev_data*)ev->user_data;
	GYFONT font = &YMGUI_Font_Default;
	d->line_count = 0;
	d->last_w = ev->area.w;
	GYcoord cw = contentW(ev);
	size_t seg_start = 0;
	for (size_t i = 0; i <= d->len; i++)
	{
		if (i == d->len || d->text[i] == '\n')
		{
			emitSegment(d, font, seg_start, d->text + seg_start, i - seg_start, cw);
			seg_start = i + 1;
		}
	}
}

/**
  * @brief 内容总高 = 行数 * 行高
  */
static int32 contentH(GYev_data* d)
{
	return (int32)d->line_count * d->line_h;//大文件行数多,像素高用 int32
}

/**
  * @brief scroll_y 钳到 [0, max(0, 内容高 - 视口高)]
  */
static void clampScroll(GYOBJ ev)
{
	GYev_data* d = (GYev_data*)ev->user_data;
	int32 maxs = contentH(d) - ev->area.h;
	if (maxs < 0)
		maxs = 0;
	d->scroll_y = GYLimitMaxMin(0, d->scroll_y, maxs);
}
/**
  * @brief 光标字节位所在的显示行索引。取"起点 off <= cursor"的最大行;若 cursor 落在该行
  *        内容之后(如 '\n' 位置),仍归该行末尾。行按 off 递增排列,线性扫即可。
  */
static size_t cursorLine(GYev_data* d)
{
	size_t li = 0;
	for (size_t i = 0; i < d->line_count; i++)
	{
		if (d->lines[i].off <= d->cursor)
			li = i;
		else
			break;
	}
	return li;
}

/**
  * @brief 光标在其所在行内的像素 x 偏移(相对文字左边缘)
  */
static GYcoord cursorPixelX(GYev_data* d, GYFONT font, size_t li)
{
	GYev_line* ln = &d->lines[li];
	size_t col = (d->cursor > ln->off) ? (d->cursor - ln->off) : 0;
	if (col > ln->len)
		col = ln->len;//光标超出本行内容(落在换行符处)→ 贴行尾
	return YMGUI_Font_TextWidthN(font, d->text + ln->off, col);
}

/**
  * @brief 在第 li 显示行上,找像素 x 最接近 target_x 的码点边界,返回其字节位置(全局 off)
  */
static size_t lineByteAtPixel(GYev_data* d, GYFONT font, size_t li, GYcoord target_x)
{
	GYev_line* ln = &d->lines[li];
	size_t best_col = 0;
	GYcoord best_dx = target_x;//到行首(x=0)的距离
	if (best_dx < 0) best_dx = -best_dx;
	GYcoord acc = 0;
	size_t i = 0;
	while (i < ln->len)
	{
		size_t nb = utf8Bytes((uint8)d->text[ln->off + i]);
		if (i + nb > ln->len)
			nb = 1;
		acc += YMGUI_Font_TextWidthN(font, d->text + ln->off + i, nb);
		i += nb;
		GYcoord dx = acc - target_x;
		if (dx < 0) dx = -dx;
		if (dx < best_dx)
		{
			best_dx = dx;
			best_col = i;
		}
	}
	return (size_t)(ln->off + best_col);
}

/**
  * @brief 把光标所在行滚入可见区(行顶在视口上沿之上则上滚,行底在下沿之下则下滚)
  */
static void ensureCursorVisible(GYOBJ ev)
{
	GYev_data* d = (GYev_data*)ev->user_data;
	size_t li = cursorLine(d);
	int32 top = (int32)li * d->line_h;
	int32 bot = top + d->line_h;
	if (top < d->scroll_y)
		d->scroll_y = top;
	else if (bot > d->scroll_y + ev->area.h)
		d->scroll_y = bot - ev->area.h;
	clampScroll(ev);
}
/**
  * @brief 在光标处插入一个字节(SDL 逐字节注入,按序拼回整码点);缓冲满则忽略
  */
static void insertByte(GYev_data* d, char c)
{
	if (d->len >= d->capacity)
		return;
	for (size_t i = d->len; i > d->cursor; i--)
		d->text[i] = d->text[i - 1];
	d->text[d->cursor] = c;
	d->len++;
	d->cursor++;
	d->text[d->len] = '\0';
}

/**
  * @brief 删光标前一整个码点(退格);跨行则删掉 '\n' 合并行。返回是否删了内容
  */
static uint8 backspace(GYev_data* d)
{
	size_t nb = prevCpBytes(d);
	if (nb == 0)
		return 0;
	for (size_t i = d->cursor; i <= d->len; i++)
		d->text[i - nb] = d->text[i];
	d->len -= nb;
	d->cursor -= nb;
	return 1;
}

/**
  * @brief 删光标处一整个码点(Del);删 '\n' 即合并下一行。返回是否删了内容
  */
static uint8 delChar(GYev_data* d)
{
	if (d->cursor >= d->len)
		return 0;
	size_t nb = (size_t)utf8Bytes((uint8)d->text[d->cursor]);
	if (d->cursor + nb > d->len)
		nb = 1;
	for (size_t i = (size_t)(d->cursor + nb); i <= d->len; i++)
		d->text[i - nb] = d->text[i];
	d->len -= nb;
	return 1;
}

/**
  * @brief 是否有有效选区(锚点有效且不等于光标)
  */
static uint8 hasSel(GYev_data* d)
{
	return (d->sel_anchor >= 0 && (size_t)d->sel_anchor != d->cursor);
}

/**
  * @brief 取选区字节范围 [s,e)(min/max of 锚点与光标);无选区则 s==e==光标
  */
static void selRange(GYev_data* d, size_t* s, size_t* e)
{
	if (!hasSel(d))
	{
		*s = *e = d->cursor;
		return;
	}
	size_t a = (size_t)d->sel_anchor;
	if (a < d->cursor) { *s = a; *e = d->cursor; }
	else               { *s = d->cursor; *e = a; }
}

/**
  * @brief 存撤销快照(每次会改动内容前调一次)。单级:覆盖上一份
  */
static void undoSnapshot(GYev_data* d)
{
	if (d->undo_text == NULL)//撤销已关闭:不存快照
		return;
	for (size_t i = 0; i <= d->len; i++)
		d->undo_text[i] = d->text[i];
	d->undo_len = d->len;
	d->undo_cursor = d->cursor;
	d->undo_anchor = d->sel_anchor;
	d->undo_valid = 1;
}

/**
  * @brief 撤销:与当前状态互换(再按一次即重做上一步),需 undo_valid
  */
static void undoRestore(GYev_data* d)
{
	if (!d->undo_valid || d->undo_text == NULL)
		return;
	//互换 text/len/cursor/anchor(swap 使连续 Ctrl+Z 在两态间切换,单级不丢当前)
	for (size_t i = 0; i <= (d->len > d->undo_len ? d->len : d->undo_len); i++)
	{
		char t = d->text[i]; d->text[i] = d->undo_text[i]; d->undo_text[i] = t;
	}
	size_t tl = d->len; d->len = d->undo_len; d->undo_len = tl;
	size_t tc = d->cursor; d->cursor = d->undo_cursor; d->undo_cursor = tc;
	ptrdiff_t ta = d->sel_anchor; d->sel_anchor = d->undo_anchor; d->undo_anchor = ta;
	d->text[d->len] = '\0';
}

/**
  * @brief 删除选区内容(有选区时)。光标落到选区起点,清选区。返回是否删了内容
  */
static uint8 deleteSelection(GYev_data* d)
{
	if (!hasSel(d))
		return 0;
	size_t s, e;
	selRange(d, &s, &e);
	size_t n = (size_t)(e - s);
	for (size_t i = e; i <= d->len; i++)
		d->text[i - n] = d->text[i];
	d->len -= n;
	d->cursor = s;
	d->sel_anchor = -1;
	return 1;
}

/**
  * @brief 在光标处插入一段字节串(用于粘贴)。缓冲满则尽量插入
  */
static void insertBytes(GYev_data* d, const char* buf, size_t n)
{
	for (size_t i = 0; i < n; i++)
	{
		if (d->len >= d->capacity)
			break;
		for (size_t j = d->len; j > d->cursor; j--)
			d->text[j] = d->text[j - 1];
		d->text[d->cursor] = buf[i];
		d->len++;
		d->cursor++;
	}
	d->text[d->len] = '\0';
}

/**
  * @brief 码点分类(用于双击选词):0=空白,1=词字符(ASCII 字母数字下划线),
  *        2=其它(中文/标点等,按单字选)。中文每字一"词"符合直觉。
  */
static uint8 charClass(uint8 c)
{
	if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
		return 0;
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	    (c >= '0' && c <= '9') || c == '_')
		return 1;
	return 2;//含 UTF-8 续字节:归"其它",按整码点单选
}

/**
  * @brief 双击选词:以光标为中心,ASCII 词连续扩展;中文/标点选当前一个码点
  */
static void selectWordAt(GYev_data* d, size_t pos)
{
	if (d->len == 0)
		return;
	if (pos >= d->len)
		pos = (d->len > 0) ? (size_t)(d->len - 1) : 0;
	//回退到码点边界
	while (pos > 0 && ((uint8)d->text[pos] & 0xC0) == 0x80)
		pos--;
	uint8 cls = charClass((uint8)d->text[pos]);
	size_t s = pos, e;
	if (cls == 1)
	{
		//ASCII 词:两侧扩展同类
		while (s > 0 && charClass((uint8)d->text[s - 1]) == 1)
			s--;
		e = pos;
		while (e < d->len && charClass((uint8)d->text[e]) == 1)
			e++;
	}
	else
	{
		//中文/标点/其它:选当前一个整码点
		s = pos;
		size_t nb = utf8Bytes((uint8)d->text[pos]);
		e = (size_t)((pos + nb > d->len) ? d->len : pos + nb);
	}
	d->sel_anchor = s;
	d->cursor = e;
}

/**
  * @brief 像素 y(相对控件顶,已含 scroll)落在第几显示行
  */
static size_t lineAtPixelY(GYev_data* d, int32 y_in_content)
{
	if (d->line_h <= 0 || d->line_count == 0)
		return 0;
	int32 li = y_in_content / d->line_h;
	if (li < 0) li = 0;
	if ((size_t)li >= d->line_count) li = (int32)d->line_count - 1;
	return (size_t)li;
}

/**
  * @brief 光标移到文本某字节位(钳到码点边界不做,调用方保证);shift=1 保留锚点扩选,否则清选区
  */
static void moveCursorTo(GYev_data* d, size_t pos, uint8 shift)
{
	if (shift)
	{
		if (d->sel_anchor < 0)
			d->sel_anchor = d->cursor;//开始扩选:锚点=移动前光标
	}
	else
		d->sel_anchor = -1;
	d->cursor = pos;
}

/**
  * @brief 行首字节位(光标所在显示行的 off)
  */
static size_t lineHomePos(GYev_data* d)
{
	return (size_t)d->lines[cursorLine(d)].off;
}

/**
  * @brief 行尾字节位(光标所在显示行 off+len)
  */
static size_t lineEndPos(GYev_data* d)
{
	GYev_line* ln = &d->lines[cursorLine(d)];
	return (size_t)(ln->off + ln->len);
}

/**
  * @brief 朴素子串查找:在 text 里从 from 起找 needle,返回字节位或 -1
  */
static ptrdiff_t findFrom(GYev_data* d, const char* needle, size_t nlen, size_t from)
{
	if (nlen == 0 || nlen > d->len)
		return -1;
	for (size_t i = from; i + nlen <= d->len; i++)
	{
		size_t j = 0;
		while (j < nlen && d->text[i + j] == needle[j])
			j++;
		if (j == nlen)
			return (ptrdiff_t)i;
	}
	return -1;
}

/**
  * @brief 由指针屏幕坐标反算文本字节位(用于点击定位/拖动选择)
  */
static size_t posFromPoint(GYOBJ ev, GYcoord px, GYcoord py)
{
	GYev_data* d = (GYev_data*)ev->user_data;
	GYFONT font = &YMGUI_Font_Default;
	GYrect abs;
	YMGUI_Obj_GetAbsArea(ev, &abs);
	int32 y_content = (int32)(py - abs.y) + d->scroll_y;
	size_t li = lineAtPixelY(d, y_content);
	GYcoord target_x = (GYcoord)(px - abs.x - EV_PAD_X);
	if (target_x < 0) target_x = 0;
	return lineByteAtPixel(d, font, li, target_x);
}

/**
  * @brief 绘制:背景 → 裁到自身 → 选区/匹配高亮 → 可见行 + 光标(聚焦时)
  */
static void evDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYev_data* d = (GYev_data*)obj->user_data;
	GYFONT font = &YMGUI_Font_Default;

	//宽度变了(布局改尺寸):重算行表 + 钳滚动
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

	//选区范围(字节),供逐行高亮
	size_t selS = 0, selE = 0;
	uint8 sel_on = hasSel(d);
	if (sel_on)
		selRange(d, &selS, &selE);

	if (d->line_h > 0)
	{
		size_t first = (size_t)(d->scroll_y / d->line_h);
		for (size_t li = first; li < d->line_count; li++)
		{
			GYcoord ly = abs->y + (GYcoord)((int32)li * d->line_h - d->scroll_y);
			if (ly >= abs->y + abs->h)
				break;//滚出下沿
			GYev_line* ln = &d->lines[li];
			GYcoord tx = abs->x + EV_PAD_X;

			//—— 查找匹配高亮(在选区之下先画):本行内所有 needle 出现处 ——
			if (d->find_len > 0 && d->find_len <= ln->len)
			{
				for (size_t i = 0; i + d->find_len <= ln->len; )
				{
					size_t j = 0;
					while (j < d->find_len && d->text[ln->off + i + j] == d->find[j])
						j++;
					if (j == d->find_len)
					{
						GYcoord hx = tx + YMGUI_Font_TextWidthN(font, d->text + ln->off, i);
						GYcoord hw = YMGUI_Font_TextWidthN(font, d->text + ln->off + i, d->find_len);
						GYrect hr = { hx, ly, hw, d->line_h };
						YMGUI_Draw_Fill(s, &hr, d->find_color, GY_OPA_COVER);
						i += d->find_len;
					}
					else
						i++;
				}
			}

			//—— 选区高亮:本行与 [selS,selE) 的交集 ——
			if (sel_on)
			{
				size_t lo = (size_t)ln->off;
				size_t hi = (size_t)(ln->off + ln->len);
				//行内容与选区交集
				size_t a = (selS > lo) ? selS : lo;
				size_t b = (selE < hi) ? selE : hi;
				if (a < b)
				{
					GYcoord hx = tx + YMGUI_Font_TextWidthN(font, d->text + ln->off, (size_t)(a - lo));
					GYcoord hw = YMGUI_Font_TextWidthN(font, d->text + a, (size_t)(b - a));
					GYrect hr = { hx, ly, hw, d->line_h };
					YMGUI_Draw_Fill(s, &hr, d->sel_color, GY_OPA_COVER);
				}
				//选区跨过本行末尾的换行符(selE 落在下一行或更后)→ 行尾补一小块示意
				if (selS <= hi && selE > hi)
				{
					GYcoord hx = tx + YMGUI_Font_TextWidthN(font, d->text + ln->off, (size_t)ln->len);
					GYrect hr = { hx, ly, EV_NL_MARK, d->line_h };
					YMGUI_Draw_Fill(s, &hr, d->sel_color, GY_OPA_COVER);
				}
			}

			if (ln->len > 0)
				YMGUI_Draw_TextN(s, font, tx, ly, d->text + ln->off, ln->len, d->text_color);
		}
	}

	//光标(仅编辑态显示,选择态不显):定位到所在显示行 + 行内像素偏移,画竖线
	if (obj->state & GY_STATE_Editing)
	{
		size_t li = cursorLine(d);
		GYcoord cy = abs->y + (GYcoord)((int32)li * d->line_h - d->scroll_y);
		GYcoord cx = abs->x + EV_PAD_X + cursorPixelX(d, font, li);
		GYrect cur = {cx, cy, 2, font->cell_h};
		//仅当光标行在可视区内才画
		if (cy + font->cell_h > abs->y && cy < abs->y + abs->h)
			YMGUI_Draw_Fill(s, &cur, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0), GY_OPA_COVER);
	}

	//边框 1px(勾出编辑区范围,不管背景什么色都看得见)。alpha=0 则不画
	if (GY_COLOR_A(d->border_color) != 0)
	{
		GYrect top    = { abs->x,             abs->y,             abs->w, 1 };
		GYrect bottom = { abs->x,             abs->y + abs->h - 1, abs->w, 1 };
		GYrect left   = { abs->x,             abs->y,             1, abs->h };
		GYrect right  = { abs->x + abs->w - 1, abs->y,             1, abs->h };
		YMGUI_Draw_Fill(s, &top,    d->border_color, GY_OPA_COVER);
		YMGUI_Draw_Fill(s, &bottom, d->border_color, GY_OPA_COVER);
		YMGUI_Draw_Fill(s, &left,   d->border_color, GY_OPA_COVER);
		YMGUI_Draw_Fill(s, &right,  d->border_color, GY_OPA_COVER);
	}

	s->clip = saved_clip;
}

static void evFreeCb(GYOBJ obj)
{
	GYev_data* d = (GYev_data*)obj->user_data;
	if (d != NULL)
	{
		if (d->text != NULL)
			GY_free1(d->text);
		if (d->undo_text != NULL)
			GY_free1(d->undo_text);
		if (d->lines != NULL)
			GY_free0(d->lines);
		GY_free0(d);
		obj->user_data = NULL;
	}
}
//移光标到显示行 li 的像素列 px(保持列),shift 扩选。UP/DOWN 复用
static void moveByDisplayLine(GYOBJ obj, GYev_data* d, GYFONT font, int dir, uint8 shift)
{
	size_t li = cursorLine(d);
	GYcoord px = cursorPixelX(d, font, li);
	size_t np = d->cursor;
	if (dir < 0 && li > 0)
		np = lineByteAtPixel(d, font, (size_t)(li - 1), px);
	else if (dir > 0 && li + 1 < d->line_count)
		np = lineByteAtPixel(d, font, (size_t)(li + 1), px);
	moveCursorTo(d, np, shift);
}

/**
  * @brief 事件:聚焦变化标脏;按下定位光标;拖动选择;双击选词;按键编辑/移光标/剪贴板/查找
  */
static void evEventCb(GYOBJ obj, GYEvent e)
{
	GYev_data* d = (GYev_data*)obj->user_data;
	GYFONT font = &YMGUI_Font_Default;
	switch (e)
	{
	case GY_EVENT_FocusGot:
	case GY_EVENT_FocusLost:
		YMGUI_Obj_Invalidate(obj);//光标显隐
		break;
	case GY_EVENT_Pressed:
	{
		//点击定位光标(此前缺口:只记拖动锚点不设光标)。清选区(不在此设锚点——
		//否则"点一下就打字"会因 anchor!=cursor 误判为有选区而删字)。拖动锚点在首个 Pressing 里设。
		size_t pos = posFromPoint(obj, obj->ctx->point_x, obj->ctx->point_y);
		d->cursor = pos;
		d->sel_anchor = -1;
		ensureCursorVisible(obj);
		YMGUI_Obj_Invalidate(obj);
		break;
	}
	case GY_EVENT_Pressing:
	{
		//拖动选择:首个 Pressing 时把当前光标(即按下点)设为锚点,之后移光标到指针处
		if (d->sel_anchor < 0)
			d->sel_anchor = d->cursor;
		size_t pos = posFromPoint(obj, obj->ctx->point_x, obj->ctx->point_y);
		d->cursor = pos;
		ensureCursorVisible(obj);
		YMGUI_Obj_Invalidate(obj);
		break;
	}
	case GY_EVENT_DoubleClicked:
	{
		size_t pos = posFromPoint(obj, obj->ctx->point_x, obj->ctx->point_y);
		selectWordAt(d, pos);
		ensureCursorVisible(obj);
		YMGUI_Obj_Invalidate(obj);
		break;
	}
	case GY_EVENT_Key:
	{
		size_t k = obj->ctx->last_key;
		size_t len_before = d->len;

		//Ctrl+F:转发给上层(弹查找条),消费掉。两级状态下都响应
		if (k == GY_KEY_FIND)
		{
			if (d->find_cb != NULL)
				d->find_cb(obj);
			obj->ctx->key_handled = 1;
			return;
		}

		//----- 选择态(未编辑):Enter/打字进编辑;Tab 放行去轮转焦点 -----
		//(多行:进编辑后 Enter 才是换行;选择态第一下 Enter 只是进编辑,不换行)
		if (!(obj->state & GY_STATE_Editing))
		{
			if (k == GY_KEY_ENTER)
			{
				obj->state |= GY_STATE_Editing;
				YMGUI_Obj_Invalidate(obj);
				obj->ctx->key_handled = 1;
			}
			else if (k >= 0x20 && k <= 0xFF)
			{
				obj->state |= GY_STATE_Editing;
				undoSnapshot(d);
				if (hasSel(d)) deleteSelection(d);
				insertByte(d, (char)k);
				rebuildLines(obj);
				ensureCursorVisible(obj);
				YMGUI_Obj_Invalidate(obj);
				if (d->len != len_before && d->changed != NULL)
					d->changed(obj, d->text);
			}
			//Tab 及其它键不消费 → Event_Key 拿 Tab 轮转焦点(选择态可 Tab 跳过编辑器)
			return;
		}

		//----- 编辑态 -----
		//先分出"会改内容"的键,统一在改前存撤销快照
		//剪贴/粘贴/撤销/复制走公开 API(自带快照/标脏/回调),消费后直接返回,不落下方通用尾。
		if (k == GY_KEY_CUT)   { YMGUI_EditView_Cut(obj);   obj->ctx->key_handled = 1; return; }
		if (k == GY_KEY_PASTE) { YMGUI_EditView_Paste(obj); obj->ctx->key_handled = 1; return; }
		if (k == GY_KEY_COPY)  { YMGUI_EditView_Copy(obj);  obj->ctx->key_handled = 1; return; }
		if (k == GY_KEY_UNDO)  { YMGUI_EditView_Undo(obj);  obj->ctx->key_handled = 1; return; }

		//其余会改内容的键,统一在改前存撤销快照
		uint8 mutates = (k == GY_KEY_BACKSPACE || k == GY_KEY_DEL || k == GY_KEY_ENTER ||
		                 k == GY_KEY_TAB ||
		                 (k >= 0x20 && k <= 0xFF));
		if (mutates)
			undoSnapshot(d);

		if (k == GY_KEY_BACKSPACE)
		{
			if (hasSel(d)) deleteSelection(d);
			else           backspace(d);
			rebuildLines(obj);
		}
		else if (k == GY_KEY_DEL)
		{
			if (hasSel(d)) deleteSelection(d);
			else           delChar(d);
			rebuildLines(obj);
		}
		else if (k == GY_KEY_ENTER)
		{
			if (hasSel(d)) deleteSelection(d);
			insertByte(d, '\n');
			rebuildLines(obj);
		}
		else if (k == GY_KEY_TAB)
		{
			//编辑器里 Tab = 缩进:插 GY_EV_TAB_WIDTH 个空格。置 key_handled 消费,不轮转焦点。
			if (hasSel(d)) deleteSelection(d);
			for (int i = 0; i < GY_EV_TAB_WIDTH; i++)
				insertByte(d, ' ');
			rebuildLines(obj);
			obj->ctx->key_handled = 1;
		}
		//—— 不改内容的键 ——
		else if (k == GY_KEY_SEL_ALL)
		{
			d->sel_anchor = 0;
			d->cursor = d->len;
			YMGUI_Obj_Invalidate(obj);
			return;
		}
		else if (k == GY_KEY_LEFT || k == GY_KEY_SHIFT_LEFT)
		{
			uint8 sh = (k == GY_KEY_SHIFT_LEFT);
			//无 shift 且有选区:折叠到选区左端
			if (!sh && hasSel(d)) { size_t a,b; selRange(d,&a,&b); d->cursor=a; d->sel_anchor=-1; }
			else if (d->cursor > 0) moveCursorTo(d, (size_t)(d->cursor - prevCpBytes(d)), sh);
			else moveCursorTo(d, d->cursor, sh);
		}
		else if (k == GY_KEY_RIGHT || k == GY_KEY_SHIFT_RIGHT)
		{
			uint8 sh = (k == GY_KEY_SHIFT_RIGHT);
			if (!sh && hasSel(d)) { size_t a,b; selRange(d,&a,&b); d->cursor=b; d->sel_anchor=-1; }
			else if (d->cursor < d->len)
			{
				size_t nb = (size_t)utf8Bytes((uint8)d->text[d->cursor]);
				moveCursorTo(d, (size_t)((d->cursor + nb > d->len) ? d->len : d->cursor + nb), sh);
			}
			else moveCursorTo(d, d->cursor, sh);
		}
		else if (k == GY_KEY_UP)        moveByDisplayLine(obj, d, font, -1, 0);
		else if (k == GY_KEY_DOWN)      moveByDisplayLine(obj, d, font, +1, 0);
		else if (k == GY_KEY_SHIFT_UP)  moveByDisplayLine(obj, d, font, -1, 1);
		else if (k == GY_KEY_SHIFT_DOWN)moveByDisplayLine(obj, d, font, +1, 1);
		else if (k == GY_KEY_HOME)       moveCursorTo(d, lineHomePos(d), 0);
		else if (k == GY_KEY_END)        moveCursorTo(d, lineEndPos(d), 0);
		else if (k == GY_KEY_SHIFT_HOME) moveCursorTo(d, lineHomePos(d), 1);
		else if (k == GY_KEY_SHIFT_END)  moveCursorTo(d, lineEndPos(d), 1);
		else if (k == GY_KEY_DOC_HOME)   moveCursorTo(d, 0, 0);
		else if (k == GY_KEY_DOC_END)    moveCursorTo(d, d->len, 0);
		else if (k >= 0x20 && k <= 0xFF)//可打印 ASCII + UTF-8 字节
		{
			if (hasSel(d)) deleteSelection(d);
			insertByte(d, (char)k);
			rebuildLines(obj);
		}
		else
			return;//其他键忽略,不标脏
		ensureCursorVisible(obj);
		YMGUI_Obj_Invalidate(obj);
		if (d->len != len_before && d->changed != NULL)
			d->changed(obj, d->text);//内容变才通知
		break;
	}
	default:
		break;
	}
}

//---- 公共 API ----
/**
  * @brief 创建可编辑多行文本框(空文本,Wrap 关,可获焦)
  */
GYOBJ YMGUI_Creat_EditView_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h, size_t capacity)
{
	GYOBJ ev = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(ev);
	if (ev == NULL)
		return NULL;
	GYev_data* d = (GYev_data*)GY_malloc0(sizeof(GYev_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "可编辑文本框数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(ev); return NULL; }
	if (capacity == 0) capacity = 1;//至少放得下一个 '\0'
	d->capacity = capacity;
	//正文缓冲 + 撤销镜像(默认开)都按 capacity+1 从大数据区分配
	d->text      = (char*)GY_malloc1(capacity + 1);
	d->undo_text = (char*)GY_malloc1(capacity + 1);
	gy_assert(d->text && d->undo_text);
	gy_log_explain((d->text == NULL) || (d->undo_text == NULL), GY_LOG_Mem0, "可编辑文本框缓冲内存申请失败");
	if (d->text == NULL || d->undo_text == NULL)
	{
		if (d->text) GY_free1(d->text);
		if (d->undo_text) GY_free1(d->undo_text);
		GY_free0(d);
		YMGUI_Free_ObjFree(ev);
		return NULL;
	}
	d->text[0] = '\0';
	d->len = 0;
	d->cursor = 0;
	d->sel_anchor = -1;
	d->lines = NULL;
	d->line_count = 0;
	d->line_cap = 0;
	d->line_h = YMGUI_Font_Default.cell_h + 4;
	d->scroll_y = 0;
	d->last_w = w;
	d->wrap = 0;
	d->changed = NULL;
	d->find_cb = NULL;
	d->undo_valid = 0;
	d->undo_anchor = -1;
	d->find_len = 0;
	d->bg           = GY_ARGB(0xFF, 0x1C, 0x1C, 0x24);
	d->text_color   = GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0);
	d->sel_color    = GY_ARGB(0xFF, 0x33, 0x55, 0x99);//选区蓝
	d->find_color   = GY_ARGB(0xFF, 0x88, 0x66, 0x22);//匹配暗黄
	d->border_color = GY_ARGB(0xFF, 0x50, 0x50, 0x60);//边框浅灰(勾出编辑区范围)

	ev->type = GY_OBJ_Base;
	ev->state |= GY_STATE_Focusable;//点击可获焦
	ev->user_data = d;
	ev->draw_cb = evDrawCb;
	ev->event_cb = evEventCb;
	ev->free_cb = evFreeCb;
	ev->bg_color = d->bg;
	rebuildLines(ev);//空文本 → 一行占位
	YMGUI_Obj_Invalidate(ev);
	return ev;
}
/**
  * @brief 设文本(拷进内部缓冲;NULL/"" 清空)。光标移末尾,重算行表 + 钳滚动 + 标脏
  */
void YMGUI_EditView_SetText(GYOBJ ev, const char* text)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	GYev_data* d = (GYev_data*)ev->user_data;
	size_t i = 0;
	if (text != NULL)
	{
		while (text[i] != '\0' && i < d->capacity)
		{
			d->text[i] = text[i];
			i++;
		}
	}
	d->text[i] = '\0';
	d->len = i;
	d->cursor = i;
	d->sel_anchor = -1;
	d->undo_valid = 0;
	d->scroll_y = 0;
	rebuildLines(ev);
	clampScroll(ev);
	YMGUI_Obj_Invalidate(ev);
}

/**
  * @brief 取当前文本(内部缓冲;空返回 "")
  */
const char* YMGUI_EditView_GetText(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	return ((GYev_data*)ev->user_data)->text;
}

/**
  * @brief 设 Wrap(按宽度自动折行),变则重算行表 + 钳滚动 + 标脏
  */
void YMGUI_EditView_SetWrap(GYOBJ ev, uint8 on)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	GYev_data* d = (GYev_data*)ev->user_data;
	uint8 nv = (on != 0) ? 1 : 0;
	if (d->wrap == nv)
		return;
	d->wrap = nv;
	rebuildLines(ev);
	clampScroll(ev);
	YMGUI_Obj_Invalidate(ev);
}

/**
  * @brief 设文字颜色,标脏
  */
void YMGUI_EditView_SetTextColor(GYOBJ ev, GYcolor color)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	((GYev_data*)ev->user_data)->text_color = color;
	YMGUI_Obj_Invalidate(ev);
}

/**
  * @brief 设背景色(编辑区底色),标脏。同步 bg_color 供框架用
  */
void YMGUI_EditView_SetBgColor(GYOBJ ev, GYcolor color)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	((GYev_data*)ev->user_data)->bg = color;
	ev->bg_color = color;
	YMGUI_Obj_Invalidate(ev);
}

/**
  * @brief 设边框颜色(1px 外框);alpha=0 则不画边框。标脏
  */
void YMGUI_EditView_SetBorderColor(GYOBJ ev, GYcolor color)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	((GYev_data*)ev->user_data)->border_color = color;
	YMGUI_Obj_Invalidate(ev);
}

/**
  * @brief 撤销开关。开→按 capacity 分配镜像;关→释放镜像省 RAM,Undo 变空操作
  */
void YMGUI_EditView_SetUndoEnabled(GYOBJ ev, uint8 on)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	GYev_data* d = (GYev_data*)ev->user_data;
	if (on)
	{
		if (d->undo_text == NULL)
		{
			d->undo_text = (char*)GY_malloc1(d->capacity + 1);
			gy_assert(d->undo_text);
			gy_log_explain(d->undo_text == NULL, GY_LOG_Mem0, "撤销镜像内存申请失败");
		}
	}
	else
	{
		if (d->undo_text != NULL)
		{
			GY_free1(d->undo_text);
			d->undo_text = NULL;
		}
		d->undo_valid = 0;//关掉后残留快照作废
	}
}

/**
  * @brief 设滚动位置(钳制),标脏
  */
void YMGUI_EditView_SetScroll(GYOBJ ev, int32 scroll_y)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	((GYev_data*)ev->user_data)->scroll_y = scroll_y;
	clampScroll(ev);
	YMGUI_Obj_Invalidate(ev);
}

int32 YMGUI_EditView_GetScroll(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	return ((GYev_data*)ev->user_data)->scroll_y;
}

size_t YMGUI_EditView_GetLineCount(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	return ((GYev_data*)ev->user_data)->line_count;
}

size_t YMGUI_EditView_GetCursor(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	return ((GYev_data*)ev->user_data)->cursor;
}

/**
  * @brief 设文本变更回调
  */
void YMGUI_EditView_SetChanged(GYOBJ ev, GYev_changed_cb cb)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	((GYev_data*)ev->user_data)->changed = cb;
}

/**
  * @brief 设查找请求回调(Ctrl+F)
  */
void YMGUI_EditView_SetFindCb(GYOBJ ev, GYev_action_cb cb)
{
	gy_assert(ev && ev->user_data);
	gy_log_explain((ev == NULL) || (ev->user_data == NULL), GY_LOG_PtrI, "可编辑文本框或数据不存在");
	((GYev_data*)ev->user_data)->find_cb = cb;
}

//======================== 选区 ========================
uint8 YMGUI_EditView_HasSelection(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	return hasSel((GYev_data*)ev->user_data);
}

void YMGUI_EditView_GetSelection(GYOBJ ev, size_t* start, size_t* end)
{
	gy_assert(ev && ev->user_data);
	size_t s, e;
	selRange((GYev_data*)ev->user_data, &s, &e);
	if (start) *start = s;
	if (end)   *end = e;
}

void YMGUI_EditView_SelectAll(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	GYev_data* d = (GYev_data*)ev->user_data;
	d->sel_anchor = 0;
	d->cursor = d->len;
	YMGUI_Obj_Invalidate(ev);
}

void YMGUI_EditView_ClearSelection(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	((GYev_data*)ev->user_data)->sel_anchor = -1;
	YMGUI_Obj_Invalidate(ev);
}

size_t YMGUI_EditView_GetSelectionText(GYOBJ ev, char* out, size_t out_cap)
{
	gy_assert(ev && ev->user_data);
	if (out == NULL || out_cap == 0)
		return 0;
	GYev_data* d = (GYev_data*)ev->user_data;
	size_t s, e;
	selRange(d, &s, &e);
	size_t n = (size_t)(e - s);
	if (n > out_cap - 1)
		n = (size_t)(out_cap - 1);
	for (size_t i = 0; i < n; i++)
		out[i] = d->text[s + i];
	out[n] = '\0';
	return n;
}

//======================== 编辑动作(菜单/快捷键共用)========================
//这些不依赖焦点/编辑态,菜单可直接调;键盘路径也复用(避免逻辑两份)。
void YMGUI_EditView_Undo(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	if (ev == NULL || ev->user_data == NULL) return;
	GYev_data* d = (GYev_data*)ev->user_data;
	undoRestore(d);
	rebuildLines(ev);
	ensureCursorVisible(ev);
	YMGUI_Obj_Invalidate(ev);
	if (d->changed != NULL)
		d->changed(ev, d->text);
}

//把当前选区写入剪贴板。不用 128KB 栈临时:在选区末尾就地插 '\0' 传指针,再还原
static void copySelToClipboard(GYev_data* d)
{
	size_t s, e;
	selRange(d, &s, &e);
	char saved = d->text[e];
	d->text[e] = '\0';
	YMGUI_Clipboard_SetText(d->text + s);
	d->text[e] = saved;
}

void YMGUI_EditView_Copy(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	if (ev == NULL || ev->user_data == NULL) return;
	GYev_data* d = (GYev_data*)ev->user_data;
	if (!hasSel(d)) return;
	copySelToClipboard(d);
}

void YMGUI_EditView_Cut(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	if (ev == NULL || ev->user_data == NULL) return;
	GYev_data* d = (GYev_data*)ev->user_data;
	if (!hasSel(d)) return;
	copySelToClipboard(d);
	undoSnapshot(d);
	deleteSelection(d);
	rebuildLines(ev);
	ensureCursorVisible(ev);
	YMGUI_Obj_Invalidate(ev);
	if (d->changed != NULL)
		d->changed(ev, d->text);
}

void YMGUI_EditView_Paste(GYOBJ ev)
{
	gy_assert(ev && ev->user_data);
	if (ev == NULL || ev->user_data == NULL) return;
	GYev_data* d = (GYev_data*)ev->user_data;
	const char* clip = YMGUI_Clipboard_GetText();
	if (clip == NULL || clip[0] == '\0') return;
	size_t cl = 0;
	while (clip[cl] != '\0') cl++;
	undoSnapshot(d);
	if (hasSel(d)) deleteSelection(d);
	insertBytes(d, clip, cl);
	rebuildLines(ev);
	ensureCursorVisible(ev);
	YMGUI_Obj_Invalidate(ev);
	if (d->changed != NULL)
		d->changed(ev, d->text);
}

//======================== 光标行列 ========================
void YMGUI_EditView_GetCursorRowCol(GYOBJ ev, size_t* row, size_t* col)
{
	gy_assert(ev && ev->user_data);
	GYev_data* d = (GYev_data*)ev->user_data;
	//按 '\n' 计逻辑行/列(不受 wrap 影响,符合"文件里第几行第几列"直觉)
	size_t r = 1, c = 1;
	size_t col_bytes = 0;//本行已过字节(用于回退码点边界不必,列按码点数)
	(void)col_bytes;
	for (size_t i = 0; i < d->cursor && i < d->len; )
	{
		if (d->text[i] == '\n')
		{
			r++;
			c = 1;
			i++;
		}
		else
		{
			size_t nb = utf8Bytes((uint8)d->text[i]);
			if (i + nb > d->len) nb = 1;
			i += nb;
			c++;
		}
	}
	if (row) *row = r;
	if (col) *col = c;
}

//======================== 查找 / 替换 ========================
/**
  * @brief 设查找词(匹配高亮用)。拷进内部 find 缓冲(截断到上限),标脏
  */
void YMGUI_EditView_SetFindNeedle(GYOBJ ev, const char* needle)
{
	gy_assert(ev && ev->user_data);
	GYev_data* d = (GYev_data*)ev->user_data;
	size_t i = 0;
	if (needle != NULL)
		while (needle[i] != '\0' && i < EV_FIND_MAX - 1) { d->find[i] = needle[i]; i++; }
	d->find[i] = '\0';
	d->find_len = i;
	YMGUI_Obj_Invalidate(ev);
}

/**
  * @brief 内部:选中 [s, s+nlen) 并滚入可见,标脏
  */
static void selectMatch(GYOBJ ev, size_t s, size_t nlen)
{
	GYev_data* d = (GYev_data*)ev->user_data;
	d->sel_anchor = s;
	d->cursor = (size_t)(s + nlen);
	ensureCursorVisible(ev);
	YMGUI_Obj_Invalidate(ev);
}

uint8 YMGUI_EditView_FindNext(GYOBJ ev, const char* needle)
{
	gy_assert(ev && ev->user_data);
	GYev_data* d = (GYev_data*)ev->user_data;
	YMGUI_EditView_SetFindNeedle(ev, needle);
	size_t nlen = d->find_len;
	if (nlen == 0)
		return 0;
	//从光标(或选区末尾)后一位起找,找不到再从头回卷
	size_t from = d->cursor;
	size_t s, e;
	selRange(d, &s, &e);
	if (hasSel(d) && (size_t)(e - s) == nlen)
		from = s + 1;//已停在一个匹配上 → 从其后找下一个
	ptrdiff_t hit = findFrom(d, d->find, nlen, from);
	if (hit < 0)
		hit = findFrom(d, d->find, nlen, 0);//回卷
	if (hit < 0)
		return 0;
	selectMatch(ev, (size_t)hit, nlen);
	return 1;
}

uint8 YMGUI_EditView_FindPrev(GYOBJ ev, const char* needle)
{
	gy_assert(ev && ev->user_data);
	GYev_data* d = (GYev_data*)ev->user_data;
	YMGUI_EditView_SetFindNeedle(ev, needle);
	size_t nlen = d->find_len;
	if (nlen == 0)
		return 0;
	//在光标之前找最后一个匹配;没有则从末尾回卷找最后一个
	size_t s, e;
	selRange(d, &s, &e);
	size_t limit = s;//选区起点之前
	int32 best = -1;
	for (int32 i = 0; i + nlen <= d->len && (size_t)i < limit; i++)
	{
		size_t j = 0;
		while (j < nlen && d->text[i + j] == d->find[j]) j++;
		if (j == nlen) best = i;
	}
	if (best < 0)
	{
		//回卷:全文找最后一个
		for (int32 i = 0; i + nlen <= d->len; i++)
		{
			size_t j = 0;
			while (j < nlen && d->text[i + j] == d->find[j]) j++;
			if (j == nlen) best = i;
		}
	}
	if (best < 0)
		return 0;
	selectMatch(ev, (size_t)best, nlen);
	return 1;
}

uint8 YMGUI_EditView_Replace(GYOBJ ev, const char* needle, const char* repl)
{
	gy_assert(ev && ev->user_data);
	GYev_data* d = (GYev_data*)ev->user_data;
	if (needle == NULL || needle[0] == '\0')
		return 0;
	size_t nlen = 0;
	while (needle[nlen] != '\0') nlen++;
	//若当前选区正好是 needle,替换;否则先 FindNext 到一个
	size_t s, e;
	selRange(d, &s, &e);
	uint8 sel_is_needle = 0;
	if (hasSel(d) && (size_t)(e - s) == nlen)
	{
		size_t j = 0;
		while (j < nlen && d->text[s + j] == needle[j]) j++;
		sel_is_needle = (j == nlen);
	}
	if (!sel_is_needle)
		return YMGUI_EditView_FindNext(ev, needle);//没停在匹配上:先跳到下一个
	undoSnapshot(d);
	deleteSelection(d);
	size_t rlen = 0;
	if (repl != NULL) while (repl[rlen] != '\0') rlen++;
	insertBytes(d, repl != NULL ? repl : "", rlen);
	rebuildLines(ev);
	ensureCursorVisible(ev);
	YMGUI_Obj_Invalidate(ev);
	if (d->changed != NULL)
		d->changed(ev, d->text);
	YMGUI_EditView_FindNext(ev, needle);//跳到下一个匹配,方便连续替换
	return 1;
}

size_t YMGUI_EditView_ReplaceAll(GYOBJ ev, const char* needle, const char* repl)
{
	gy_assert(ev && ev->user_data);
	GYev_data* d = (GYev_data*)ev->user_data;
	if (needle == NULL || needle[0] == '\0')
		return 0;
	size_t nlen = 0;
	while (needle[nlen] != '\0') nlen++;
	size_t rlen = 0;
	if (repl != NULL) while (repl[rlen] != '\0') rlen++;
	undoSnapshot(d);//一次撤销回滚整个 ReplaceAll
	size_t count = 0;
	int32 pos = findFrom(d, needle, nlen, 0);
	while (pos >= 0)
	{
		//删 [pos, pos+nlen),在 pos 处插 repl
		size_t p = (size_t)pos;
		for (size_t i = (size_t)(p + nlen); i <= d->len; i++)
			d->text[i - nlen] = d->text[i];
		d->len -= nlen;
		//插 repl(逐字节,注意缓冲上限)
		d->cursor = p;
		insertBytes(d, repl != NULL ? repl : "", rlen);
		count++;
		//下一个从替换串之后找(避免 repl 含 needle 无限循环)
		pos = findFrom(d, needle, nlen, (size_t)(p + rlen));
	}
	if (count > 0)
	{
		d->sel_anchor = -1;
		rebuildLines(ev);
		clampScroll(ev);
		YMGUI_Obj_Invalidate(ev);
		if (d->changed != NULL)
			d->changed(ev, d->text);
	}
	else
		d->undo_valid = 0;
	return count;
}
