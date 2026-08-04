#include "YMGUI_TextInput.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
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
	char   text[GY_TI_TEXT_MAX];
	uint16 len;      //当前字节数(不含 '\0')
	uint16 cursor;   //光标字节位置(0..len,始终落在 UTF-8 码点边界)
	GYti_changed_cb changed;//文本变更回调(内容变才触发)
}GYti_data;

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
  * @brief 从 cursor 向前回退一个码点边界(跳过续字节 0x80..0xBF),返回前一码点起点字节数
  */
static uint16 prevCpBytes(GYti_data* d)
{
	if (d->cursor == 0)
		return 0;
	uint16 i = d->cursor - 1;
	//续字节 10xxxxxx:继续回退,直到 lead byte 或串首
	while (i > 0 && ((uint8)d->text[i] & 0xC0) == 0x80)
		i--;
	return (uint16)(d->cursor - i);
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
	//文本(左内边距 5px,垂直居中)
	GYFONT font = &YMGUI_Font_Default;
	GYcoord tx = abs->x + 5;
	GYcoord ty = abs->y + ((abs->h > font->cell_h) ? (abs->h - font->cell_h) / 2 : 0);
	if (d->text[0] != '\0')
		YMGUI_Draw_Text(s, font, tx, ty, d->text, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0));
	//光标(仅编辑态显示,选择态只高亮不显光标,在光标前文本的像素宽处画竖线,支持中英混排)
	if (obj->state & GY_STATE_Editing)
	{
		GYcoord cx = tx + YMGUI_Font_TextWidthN(font, d->text, d->cursor);
		GYrect cur = {cx, ty, 2, font->cell_h};
		YMGUI_Draw_Fill(s, &cur, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0), GY_OPA_COVER);
	}
}

/**
  * @brief 在光标处插入一个字符
  */
static void insertChar(GYti_data* d, char c)
{
	if (d->len >= GY_TI_TEXT_MAX - 1)
		return;
	//光标后的字符整体右移一位
	for (uint16 i = d->len; i > d->cursor; i--)
		d->text[i] = d->text[i - 1];
	d->text[d->cursor] = c;
	d->len++;
	d->cursor++;
	d->text[d->len] = '\0';
}

/**
  * @brief 删除光标前一个码点(退格),整字删除不劈半个汉字
  */
static void backspace(GYti_data* d)
{
	uint16 nb = prevCpBytes(d);
	if (nb == 0)
		return;
	//[cursor, len] 整体左移 nb 字节(含结尾 '\0')
	for (uint16 i = d->cursor; i <= d->len; i++)
		d->text[i - nb] = d->text[i];
	d->len -= nb;
	d->cursor -= nb;
}

/**
  * @brief 删除光标处一个码点(Del)
  */
static void delChar(GYti_data* d)
{
	if (d->cursor >= d->len)
		return;
	uint16 nb = (uint16)utf8Bytes((uint8)d->text[d->cursor]);
	if (d->cursor + nb > d->len)
		nb = 1;//截断多字节:吞一字节
	for (uint16 i = (uint16)(d->cursor + nb); i <= d->len; i++)
		d->text[i - nb] = d->text[i];
	d->len -= nb;
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
		YMGUI_Obj_Invalidate(obj);//光标显隐
		break;
	case GY_EVENT_Key:
	{
		uint32 k = obj->ctx->last_key;
		uint16 len_before = d->len;//判内容是否真变(区分编辑与纯移光标)

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
				insertChar(d, (char)k);
				YMGUI_Obj_Invalidate(obj);
				if (d->len != len_before && d->changed != NULL)
					d->changed(obj, d->text);
			}
			//Tab 及其它键:不消费(key_handled 保持 0)→ Event_Key 拿 Tab 去轮转
			return;
		}

		//----- 编辑态 -----
		if (k == GY_KEY_ENTER)
		{
			//单行:Enter 结束编辑 + 直接轮转到下一个字段(表单手感,少按一次 Tab)。
			//FocusNext 会清掉本对象的 Editing 位(失焦即退出编辑)。
			obj->state &= (uint8)~GY_STATE_Editing;
			obj->ctx->key_handled = 1;
			YMGUI_FocusNext(obj->ctx);
			YMGUI_Obj_Invalidate(obj);
			return;
		}
		if (k == GY_KEY_TAB)
		{
			//编辑态里 Tab = 空格(B 方案:即便单行也插空格,消费不轮转)
			insertChar(d, ' ');
			YMGUI_Obj_Invalidate(obj);
			obj->ctx->key_handled = 1;
			if (d->changed != NULL) d->changed(obj, d->text);
			return;
		}
		if (k == GY_KEY_BACKSPACE)
			backspace(d);
		else if (k == GY_KEY_DEL)
			delChar(d);
		else if (k == GY_KEY_LEFT)
		{
			if (d->cursor > 0) d->cursor -= prevCpBytes(d);//左移一整个码点
		}
		else if (k == GY_KEY_RIGHT)
		{
			if (d->cursor < d->len)//右移一整个码点
			{
				uint16 nb = (uint16)utf8Bytes((uint8)d->text[d->cursor]);
				d->cursor = (uint16)((d->cursor + nb > d->len) ? d->len : d->cursor + nb);
			}
		}
		else if (k >= 0x20 && k <= 0xFF)//可打印 ASCII + UTF-8 字节(SDL 逐字节注入,拼回整码点)
			insertChar(d, (char)k);
		else
			return;//其他键忽略,不标脏
		YMGUI_Obj_Invalidate(obj);
		if (d->len != len_before && d->changed != NULL)
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
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建文本输入框
  */
GYOBJ YMGUI_Creat_TextInput_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ ti = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(ti);
	if (ti == NULL)
		return NULL;
	GYti_data* d = (GYti_data*)GY_malloc0(sizeof(GYti_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "输入框数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(ti); return NULL; }
	d->text[0] = '\0';
	d->len = 0;
	d->cursor = 0;
	d->changed = NULL;

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
	uint16 i = 0;
	while (text[i] != '\0' && i < GY_TI_TEXT_MAX - 1)
	{
		d->text[i] = text[i];
		i++;
	}
	d->text[i] = '\0';
	d->len = i;
	d->cursor = i;
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

/**
  * @brief 设文本变更回调
  */
void YMGUI_TextInput_SetChanged(GYOBJ ti, GYti_changed_cb cb)
{
	gy_assert(ti && ti->user_data);
	gy_log_explain((ti == NULL) || (ti->user_data == NULL), GY_LOG_PtrI, "输入框或数据不存在");
	((GYti_data*)ti->user_data)->changed = cb;
}
