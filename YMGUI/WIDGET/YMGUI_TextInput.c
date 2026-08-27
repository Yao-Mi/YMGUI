#include "YMGUI_TextInput.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Hal.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_TextInput.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 文本输入框。点击聚焦,GY_EVENT_Key 处理:可打印字符插入光标处,退格/删除/左右移光标
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	char*  text;     //动态文本缓冲,实占 capacity+1
	size_t capacity; //最大可用字节数(不含 '\0')
	size_t len;      //当前字节数(不含 '\0')
	size_t cursor;   //光标字节位置(0..len,始终落在 UTF-8 码点边界)
	ptrdiff_t sel_anchor;//选区锚点(-1=无选区)
	int32 scroll_x;  //文本内容水平滚动像素
	uint8 skip_utf8; //容量不足拒绝多字节字符后,还需丢弃的续字节数
	GYti_changed_cb changed;//文本变更回调(内容变才触发)
	GYti_submitted_cb submitted;//Enter 提交;NULL=默认轮转焦点
}GYti_data;

#define TI_PAD_X 5
#define TI_CURSOR_W 2

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

static size_t cpBytesAt(GYti_data* d, size_t pos)
{
	if (pos >= d->len)
		return 0;
	size_t nb = utf8Bytes((uint8)d->text[pos]);
	return (pos + nb <= d->len) ? nb : 1;
}

static uint8 hasSel(GYti_data* d)
{
	return d->sel_anchor >= 0 && (size_t)d->sel_anchor != d->cursor;
}

static void selRange(GYti_data* d, size_t* start, size_t* end)
{
	if (!hasSel(d))
	{
		*start = *end = d->cursor;
		return;
	}
	size_t anchor = (size_t)d->sel_anchor;
	if (anchor < d->cursor) { *start = anchor; *end = d->cursor; }
	else                    { *start = d->cursor; *end = anchor; }
}

static int32 textWidthRange(GYti_data* d, size_t start, size_t end)
{
	int32 width = 0;
	for (size_t pos = start; pos < end;)
	{
		size_t nb = cpBytesAt(d, pos);
		if (nb == 0) break;
		width += YMGUI_Font_TextWidthN(&YMGUI_Font_Default, d->text + pos, (uint32)nb);
		pos += nb;
	}
	return width;
}

static void ensureCursorVisible(GYOBJ obj)
{
	GYti_data* d = (GYti_data*)obj->user_data;
	int32 view_w = (int32)obj->area.w - TI_PAD_X * 2;
	if (view_w < TI_CURSOR_W) view_w = TI_CURSOR_W;
	int32 cursor_x = textWidthRange(d, 0, d->cursor);
	if (cursor_x < d->scroll_x)
		d->scroll_x = cursor_x;
	else if (cursor_x + TI_CURSOR_W > d->scroll_x + view_w)
		d->scroll_x = cursor_x + TI_CURSOR_W - view_w;
	if (d->scroll_x < 0) d->scroll_x = 0;
	int32 total_w = textWidthRange(d, 0, d->len);
	int32 max_scroll = total_w + TI_CURSOR_W - view_w;
	if (max_scroll < 0) max_scroll = 0;
	if (d->scroll_x > max_scroll) d->scroll_x = max_scroll;
}

static size_t posFromPoint(GYOBJ obj, GYcoord px)
{
	GYti_data* d = (GYti_data*)obj->user_data;
	GYrect abs;
	YMGUI_Obj_GetAbsArea(obj, &abs);
	int32 target = (int32)px - (abs.x + TI_PAD_X) + d->scroll_x;
	if (target <= 0) return 0;
	int32 x = 0;
	for (size_t pos = 0; pos < d->len;)
	{
		size_t nb = cpBytesAt(d, pos);
		int32 cw = YMGUI_Font_TextWidthN(&YMGUI_Font_Default, d->text + pos, (uint32)nb);
		if (target < x + cw / 2)
			return pos;
		x += cw;
		if (target < x)
			return pos + nb;
		pos += nb;
	}
	return d->len;
}

/**
  * @brief 从 cursor 向前回退一个码点边界(跳过续字节 0x80..0xBF),返回前一码点起点字节数
  */
static size_t prevCpBytes(GYti_data* d)
{
	if (d->cursor == 0)
		return 0;
	size_t i = d->cursor - 1;
	//续字节 10xxxxxx:继续回退,直到 lead byte 或串首
	while (i > 0 && ((uint8)d->text[i] & 0xC0) == 0x80)
		i--;
	return d->cursor - i;
}

/**
  * @brief 绘制:边框框 + 文本 + 光标(仅聚焦时)
  */
static void tiDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYti_data* d = (GYti_data*)obj->user_data;
	//背景(聚焦时略亮)
	GYcolor bg = (obj->state & GY_STATE_Focused) ? GY_ARGB(0xFF, 0x30, 0x30, 0x3A) : GY_ARGB(0xFF, 0x24, 0x24, 0x2C);
	YMGUI_Draw_Fill(s, abs, bg, GY_OPA_COVER);
	//边框(聚焦时高亮蓝)
	GYcolor bd = (obj->state & GY_STATE_Focused) ? GY_ARGB(0xFF, 0x40, 0x90, 0xE0) : GY_ARGB(0xFF, 0x50, 0x50, 0x58);
	GYrect t = {abs->x, abs->y, abs->w, 2};
	GYrect b = {abs->x, abs->y + abs->h - 2, abs->w, 2};
	GYrect l = {abs->x, abs->y, 2, abs->h};
	GYrect r = {abs->x + abs->w - 2, abs->y, 2, abs->h};
	YMGUI_Draw_Fill(s, &t, bd, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &b, bd, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &l, bd, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &r, bd, GY_OPA_COVER);
	//文本视口:所有文字、选区和光标都严格裁在边框内。
	GYFONT font = &YMGUI_Font_Default;
	GYrect content = {abs->x + 2, abs->y + 2, abs->w - 4, abs->h - 4};
	GYrect content_clip;
	if (!GY_Rect_Intersect(&content_clip, &content, &s->clip))
		return;
	GYrect saved_clip = s->clip;
	s->clip = content_clip;

	int32 skipped_x = 0;
	size_t visible_start = 0;
	while (visible_start < d->len)
	{
		size_t nb = cpBytesAt(d, visible_start);
		int32 cw = YMGUI_Font_TextWidthN(font, d->text + visible_start, (uint32)nb);
		if (skipped_x + cw > d->scroll_x)
			break;
		skipped_x += cw;
		visible_start += nb;
	}
	GYcoord tx = (GYcoord)(abs->x + TI_PAD_X - (d->scroll_x - skipped_x));
	GYcoord ty = abs->y + ((abs->h > font->cell_h) ? (abs->h - font->cell_h) / 2 : 0);
	if (hasSel(d))
	{
		size_t start, end;
		selRange(d, &start, &end);
		int32 x1 = abs->x + TI_PAD_X + textWidthRange(d, 0, start) - d->scroll_x;
		int32 x2 = abs->x + TI_PAD_X + textWidthRange(d, 0, end) - d->scroll_x;
		int32 clip_x1 = content.x;
		int32 clip_x2 = content.x + content.w;
		if (x1 < clip_x1) x1 = clip_x1;
		if (x2 > clip_x2) x2 = clip_x2;
		if (x2 > x1)
		{
			GYrect sel = {(GYcoord)x1, ty, (GYcoord)(x2 - x1), font->cell_h};
			YMGUI_Draw_Fill(s, &sel, GY_ARGB(0xFF, 0x33, 0x55, 0x99), GY_OPA_COVER);
		}
	}
	if (visible_start < d->len)
		YMGUI_Draw_TextN(s, font, tx, ty, d->text + visible_start,
		                   (uint32)(d->len - visible_start), GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0));
	//光标(仅编辑态显示,选择态只高亮不显光标,在光标前文本的像素宽处画竖线,支持中英混排)
	if (obj->state & GY_STATE_Editing)
	{
		GYcoord cx = (GYcoord)(abs->x + TI_PAD_X + textWidthRange(d, 0, d->cursor) - d->scroll_x);
		GYrect cur = {cx, ty, 2, font->cell_h};
		YMGUI_Draw_Fill(s, &cur, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0), GY_OPA_COVER);
	}
	s->clip = saved_clip;
}

/**
  * @brief 在光标处插入一个字符
  */
static uint8 insertChar(GYti_data* d, char c)
{
	uint8 uc = (uint8)c;
	if (d->skip_utf8 > 0)
	{
		if ((uc & 0xC0) == 0x80)
		{
			d->skip_utf8--;
			return 0;
		}
		d->skip_utf8 = 0;
	}
	uint32 cp_bytes = utf8Bytes(uc);
	if (cp_bytes > 1 && d->capacity - d->len < cp_bytes)
	{
		d->skip_utf8 = (uint8)(cp_bytes - 1);
		return 0;
	}
	if (d->len >= d->capacity)
		return 0;
	//光标后的字符整体右移一位
	for (size_t i = d->len; i > d->cursor; i--)
		d->text[i] = d->text[i - 1];
	d->text[d->cursor] = c;
	d->len++;
	d->cursor++;
	d->text[d->len] = '\0';
	return 1;
}

/**
  * @brief 删除光标前一个码点(退格),整字删除不劈半个汉字
  */
static uint8 backspace(GYti_data* d)
{
	size_t nb = prevCpBytes(d);
	if (nb == 0)
		return 0;
	//[cursor, len] 整体左移 nb 字节(含结尾 '\0')
	for (size_t i = d->cursor; i <= d->len; i++)
		d->text[i - nb] = d->text[i];
	d->len -= nb;
	d->cursor -= nb;
	return 1;
}

/**
  * @brief 删除光标处一个码点(Del)
  */
static uint8 delChar(GYti_data* d)
{
	if (d->cursor >= d->len)
		return 0;
	size_t nb = utf8Bytes((uint8)d->text[d->cursor]);
	if (d->cursor + nb > d->len)
		nb = 1;//截断多字节:吞一字节
	for (size_t i = d->cursor + nb; i <= d->len; i++)
		d->text[i - nb] = d->text[i];
	d->len -= nb;
	return 1;
}

static uint8 deleteSelection(GYti_data* d)
{
	if (!hasSel(d))
		return 0;
	size_t start, end;
	selRange(d, &start, &end);
	for (size_t i = end; i <= d->len; i++)
		d->text[start + i - end] = d->text[i];
	d->len -= end - start;
	d->cursor = start;
	d->sel_anchor = -1;
	return 1;
}

static void moveCursorTo(GYti_data* d, size_t pos, uint8 shift)
{
	if (shift)
	{
		if (d->sel_anchor < 0)
			d->sel_anchor = (ptrdiff_t)d->cursor;
	}
	else
		d->sel_anchor = -1;
	d->cursor = pos;
}

static uint8 insertSpan(GYti_data* d, const char* bytes, size_t count)
{
	if (count == 0 || count > d->capacity - d->len)
		return 0;
	for (size_t i = d->len + count; i > d->cursor + count; i--)
		d->text[i] = d->text[i - count];
	for (size_t i = 0; i < count; i++)
		d->text[d->cursor + i] = bytes[i];
	d->len += count;
	d->cursor += count;
	d->text[d->len] = '\0';
	return 1;
}

/**
  * @brief 事件:聚焦变化标脏;按键编辑文本
  */
static void tiEventCb(GYOBJ obj, GYEvent e)
{
	GYti_data* d = (GYti_data*)obj->user_data;
	switch (e)
	{
	case GY_EVENT_FocusGot:
	case GY_EVENT_FocusLost:
		ensureCursorVisible(obj);
		YMGUI_Obj_Invalidate(obj);//光标显隐
		break;
	case GY_EVENT_Pressed:
		d->cursor = posFromPoint(obj, obj->ctx->point_x);
		d->sel_anchor = -1;
		ensureCursorVisible(obj);
		YMGUI_Obj_Invalidate(obj);
		break;
	case GY_EVENT_Pressing:
		if (d->sel_anchor < 0)
			d->sel_anchor = (ptrdiff_t)d->cursor;
		d->cursor = posFromPoint(obj, obj->ctx->point_x);
		ensureCursorVisible(obj);
		YMGUI_Obj_Invalidate(obj);
		break;
	case GY_EVENT_Key:
	{
		uint32 k = obj->ctx->last_key;
		uint8 changed = 0;

		if (k == GY_KEY_SEL_ALL) { YMGUI_TextInput_SelectAll(obj); obj->ctx->key_handled = 1; return; }
		if (k == GY_KEY_COPY)    { YMGUI_TextInput_Copy(obj);      obj->ctx->key_handled = 1; return; }
		if (k == GY_KEY_CUT)     { YMGUI_TextInput_Cut(obj);       obj->ctx->key_handled = 1; return; }
		if (k == GY_KEY_PASTE)   { YMGUI_TextInput_Paste(obj);     obj->ctx->key_handled = 1; return; }

		//----- 选择态(未编辑):Enter/打字进编辑;Tab 放行去轮转焦点 -----
		if (!(obj->state & GY_STATE_Editing))
		{
			if (k == GY_KEY_ENTER)
			{
				//进编辑态,不产生字符
				obj->state |= GY_STATE_Editing;
				YMGUI_Obj_Invalidate(obj);
				obj->ctx->key_handled = 1;
			}
			else if (k >= 0x20 && k <= 0xFF)
			{
				//打字即进编辑并落下这一字符(Tab 过来后直接开打)
				obj->state |= GY_STATE_Editing;
				if (hasSel(d)) changed |= deleteSelection(d);
				changed |= insertChar(d, (char)k);
				ensureCursorVisible(obj);
				YMGUI_Obj_Invalidate(obj);
				if (changed && d->changed != NULL)
					d->changed(obj, d->text);
			}
			//Tab 及其它键:不消费(key_handled 保持 0)→ Event_Key 拿 Tab 去轮转
			return;
		}

		//----- 编辑态 -----
			if (k == GY_KEY_ENTER)
			{
				//单行:Enter 先退出编辑。绑定提交回调时交给上层处理(如路径跳转),
				//未绑定则保持原有表单语义:轮转到下一个字段。
				obj->state &= (uint8)~GY_STATE_Editing;
				obj->ctx->key_handled = 1;
				YMGUI_Obj_Invalidate(obj);
				if (d->submitted != NULL)
					d->submitted(obj, d->text);
				else
					YMGUI_FocusNext(obj->ctx);
				return;
		}
		if (k == GY_KEY_TAB)
		{
			//编辑态里 Tab = 空格(B 方案:即便单行也插空格,消费不轮转)
			if (hasSel(d)) changed |= deleteSelection(d);
			changed |= insertChar(d, ' ');
			ensureCursorVisible(obj);
			YMGUI_Obj_Invalidate(obj);
			obj->ctx->key_handled = 1;
			if (changed && d->changed != NULL) d->changed(obj, d->text);
			return;
		}
		if (k == GY_KEY_BACKSPACE)
			changed = hasSel(d) ? deleteSelection(d) : backspace(d);
		else if (k == GY_KEY_DEL)
			changed = hasSel(d) ? deleteSelection(d) : delChar(d);
		else if (k == GY_KEY_LEFT || k == GY_KEY_SHIFT_LEFT)
		{
			uint8 shift = (k == GY_KEY_SHIFT_LEFT);
			if (!shift && hasSel(d))
			{
				size_t start, end; selRange(d, &start, &end);
				moveCursorTo(d, start, 0);
			}
			else if (d->cursor > 0)
				moveCursorTo(d, d->cursor - prevCpBytes(d), shift);
		}
		else if (k == GY_KEY_RIGHT || k == GY_KEY_SHIFT_RIGHT)
		{
			uint8 shift = (k == GY_KEY_SHIFT_RIGHT);
			if (!shift && hasSel(d))
			{
				size_t start, end; selRange(d, &start, &end);
				moveCursorTo(d, end, 0);
			}
			else if (d->cursor < d->len)//右移一整个码点
			{
				size_t nb = cpBytesAt(d, d->cursor);
				moveCursorTo(d, d->cursor + nb, shift);
			}
		}
		else if (k == GY_KEY_HOME || k == GY_KEY_DOC_HOME)
			moveCursorTo(d, 0, 0);
		else if (k == GY_KEY_END || k == GY_KEY_DOC_END)
			moveCursorTo(d, d->len, 0);
		else if (k == GY_KEY_SHIFT_HOME)
			moveCursorTo(d, 0, 1);
		else if (k == GY_KEY_SHIFT_END)
			moveCursorTo(d, d->len, 1);
		else if (k >= 0x20 && k <= 0xFF)//可打印 ASCII + UTF-8 字节(SDL 逐字节注入,拼回整码点)
		{
			if (hasSel(d)) changed |= deleteSelection(d);
			changed |= insertChar(d, (char)k);
		}
		else
			return;//其他键忽略,不标脏
		ensureCursorVisible(obj);
		YMGUI_Obj_Invalidate(obj);
		if (changed && d->changed != NULL)
			d->changed(obj, d->text);//内容变才通知(退格/删除/插入)
		break;
	}
	default:
		break;
	}
}

static void tiFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GYti_data* d = (GYti_data*)obj->user_data;
		if (d->text != NULL)
			GY_free1(d->text);
		GY_free0(d);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建文本输入框
  */
GYOBJ YMGUI_Creat_TextInput_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h, size_t capacity)
{
	if (capacity == (size_t)-1)
		return NULL;//capacity+1 溢出
	GYOBJ ti = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(ti);
	if (ti == NULL)
		return NULL;
	GYti_data* d = (GYti_data*)GY_malloc0(sizeof(GYti_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "输入框数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(ti); return NULL; }
	GY_memset(d, 0, sizeof(GYti_data));
	d->text = (char*)GY_malloc1(capacity + 1);
	gy_assert(d->text);
	gy_log_explain(d->text == NULL, GY_LOG_Mem1, "输入框文本缓冲申请失败");
	if (d->text == NULL) { GY_free0(d); YMGUI_Free_ObjFree(ti); return NULL; }
	d->capacity = capacity;
	d->text[0] = '\0';
	d->len = 0;
	d->cursor = 0;
	d->sel_anchor = -1;
	d->scroll_x = 0;
	d->skip_utf8 = 0;
	d->changed = NULL;
	d->submitted = NULL;

	ti->type = GY_OBJ_Base;
	ti->state |= GY_STATE_Focusable;//点击可获焦
	ti->user_data = d;
	ti->draw_cb = tiDrawCb;
	ti->event_cb = tiEventCb;
	ti->free_cb = tiFreeCb;
	YMGUI_Obj_Invalidate(ti);
	return ti;
}

/**
  * @brief 设文本(拷贝,光标移末尾)
  */
void YMGUI_TextInput_SetText(GYOBJ ti, const char* text)
{
	gy_assert(ti && ti->user_data && text);
	gy_log_explain((ti == NULL) || (ti->user_data == NULL) || (text == NULL), GY_LOG_PtrI, "输入框/数据/文本不存在");
	GYti_data* d = (GYti_data*)ti->user_data;
	size_t src = 0, dst = 0;
	while (text[src] != '\0')
	{
		if (text[src] == '\r' || text[src] == '\n')
		{
			src++;
			continue;
		}
		size_t nb = utf8Bytes((uint8)text[src]);
		for (size_t j = 1; j < nb; j++)
			if (text[src + j] == '\0') { nb = 1; break; }
		if (dst + nb > d->capacity)
			break;
		for (size_t j = 0; j < nb; j++)
			d->text[dst++] = text[src++];
	}
	d->text[dst] = '\0';
	d->len = dst;
	d->cursor = dst;
	d->sel_anchor = -1;
	d->skip_utf8 = 0;
	ensureCursorVisible(ti);
	YMGUI_Obj_Invalidate(ti);
}

/**
  * @brief 读文本
  */
const char* YMGUI_TextInput_GetText(GYOBJ ti)
{
	gy_assert(ti && ti->user_data);
	gy_log_explain((ti == NULL) || (ti->user_data == NULL), GY_LOG_PtrI, "输入框或数据不存在");
	return ((GYti_data*)ti->user_data)->text;
}

uint8 YMGUI_TextInput_HasSelection(GYOBJ ti)
{
	gy_assert(ti && ti->user_data);
	return (ti != NULL && ti->user_data != NULL) ? hasSel((GYti_data*)ti->user_data) : 0;
}

void YMGUI_TextInput_GetSelection(GYOBJ ti, size_t* start, size_t* end)
{
	gy_assert(ti && ti->user_data);
	if (ti == NULL || ti->user_data == NULL) return;
	size_t s, e;
	selRange((GYti_data*)ti->user_data, &s, &e);
	if (start != NULL) *start = s;
	if (end != NULL) *end = e;
}

void YMGUI_TextInput_SelectAll(GYOBJ ti)
{
	gy_assert(ti && ti->user_data);
	if (ti == NULL || ti->user_data == NULL) return;
	GYti_data* d = (GYti_data*)ti->user_data;
	d->sel_anchor = 0;
	d->cursor = d->len;
	ensureCursorVisible(ti);
	YMGUI_Obj_Invalidate(ti);
}

void YMGUI_TextInput_ClearSelection(GYOBJ ti)
{
	gy_assert(ti && ti->user_data);
	if (ti == NULL || ti->user_data == NULL) return;
	((GYti_data*)ti->user_data)->sel_anchor = -1;
	YMGUI_Obj_Invalidate(ti);
}

size_t YMGUI_TextInput_GetSelectionText(GYOBJ ti, char* out, size_t out_cap)
{
	gy_assert(ti && ti->user_data);
	if (ti == NULL || ti->user_data == NULL) return 0;
	GYti_data* d = (GYti_data*)ti->user_data;
	size_t start, end;
	selRange(d, &start, &end);
	size_t len = end - start;
	if (out != NULL && out_cap > 0)
	{
		size_t n = (len < out_cap - 1) ? len : out_cap - 1;
		for (size_t i = 0; i < n; i++) out[i] = d->text[start + i];
		out[n] = '\0';
	}
	return len;
}

static void copySelectionToClipboard(GYti_data* d)
{
	size_t start, end;
	selRange(d, &start, &end);
	char saved = d->text[end];
	d->text[end] = '\0';
	YMGUI_Clipboard_SetText(d->text + start);
	d->text[end] = saved;
}

void YMGUI_TextInput_Copy(GYOBJ ti)
{
	gy_assert(ti && ti->user_data);
	if (ti == NULL || ti->user_data == NULL) return;
	GYti_data* d = (GYti_data*)ti->user_data;
	if (hasSel(d)) copySelectionToClipboard(d);
}

void YMGUI_TextInput_Cut(GYOBJ ti)
{
	gy_assert(ti && ti->user_data);
	if (ti == NULL || ti->user_data == NULL) return;
	GYti_data* d = (GYti_data*)ti->user_data;
	if (!hasSel(d)) return;
	copySelectionToClipboard(d);
	deleteSelection(d);
	ensureCursorVisible(ti);
	YMGUI_Obj_Invalidate(ti);
	if (d->changed != NULL) d->changed(ti, d->text);
}

void YMGUI_TextInput_Paste(GYOBJ ti)
{
	gy_assert(ti && ti->user_data);
	if (ti == NULL || ti->user_data == NULL) return;
	GYti_data* d = (GYti_data*)ti->user_data;
	const char* clip = YMGUI_Clipboard_GetText();
	if (clip == NULL || clip[0] == '\0') return;
	uint8 changed = deleteSelection(d);
	for (size_t pos = 0; clip[pos] != '\0';)
	{
		if (clip[pos] == '\r' || clip[pos] == '\n')
		{
			pos++;
			continue;
		}
		size_t nb = utf8Bytes((uint8)clip[pos]);
		for (size_t j = 1; j < nb; j++)
			if (clip[pos + j] == '\0') { nb = 1; break; }
		if (nb > d->capacity - d->len)
			break;
		changed |= insertSpan(d, clip + pos, nb);
		pos += nb;
	}
	d->skip_utf8 = 0;
	ensureCursorVisible(ti);
	YMGUI_Obj_Invalidate(ti);
	if (changed && d->changed != NULL) d->changed(ti, d->text);
}

int32 YMGUI_TextInput_GetScrollX(GYOBJ ti)
{
	gy_assert(ti && ti->user_data);
	return (ti != NULL && ti->user_data != NULL) ? ((GYti_data*)ti->user_data)->scroll_x : 0;
}

/**
  * @brief 设文本变更回调
  */
void YMGUI_TextInput_SetChanged(GYOBJ ti, GYti_changed_cb cb)
{
	gy_assert(ti && ti->user_data);
	gy_log_explain((ti == NULL) || (ti->user_data == NULL), GY_LOG_PtrI, "输入框或数据不存在");
	((GYti_data*)ti->user_data)->changed = cb;
}

void YMGUI_TextInput_SetSubmitted(GYOBJ ti, GYti_submitted_cb cb)
{
	gy_assert(ti && ti->user_data);
	if (ti == NULL || ti->user_data == NULL) return;
	((GYti_data*)ti->user_data)->submitted = cb;
}
