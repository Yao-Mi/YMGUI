#include "YMGUI_Checkbox.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Checkbox.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 复选框控件。左侧方框(勾选时内填亮块) + 右侧文字。点击切换 checked 并回调
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	uint8           checked;
	char            text[GY_CB_TEXT_MAX];
	GYcb_changed_cb changed;
}GYcb_data;

/**
  * @brief 绘制:方框(边框+勾选内填) + 右侧居中文字
  */
static void cbDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYcb_data* d = (GYcb_data*)obj->user_data;
	GYcoord box = abs->h;//方框边长 = 控件高
	//方框背景
	GYrect boxr = {abs->x, abs->y, box, box};
	YMGUI_Draw_Fill(s, &boxr, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0), GY_OPA_COVER);
	//边框
	GYcolor bd = GY_ARGB(0xFF, 0x40, 0x40, 0x40);
	GYrect t = {abs->x, abs->y, box, 2};
	GYrect b = {abs->x, abs->y + box - 2, box, 2};
	GYrect l = {abs->x, abs->y, 2, box};
	GYrect r = {abs->x + box - 2, abs->y, 2, box};
	YMGUI_Draw_Fill(s, &t, bd, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &b, bd, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &l, bd, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &r, bd, GY_OPA_COVER);
	//勾选:内填亮蓝块
	if (d->checked)
	{
		GYrect fill = {abs->x + 4, abs->y + 4, box - 8, box - 8};
		YMGUI_Draw_Fill(s, &fill, GY_ARGB(0xFF, 0x30, 0x90, 0xE0), GY_OPA_COVER);
	}
	//右侧文字(方框右边留 6px)
	if (d->text[0] != '\0')
	{
		GYFONT font = &YMGUI_Font_Default;
		GYcoord ty = abs->y + ((box > font->cell_h) ? (box - font->cell_h) / 2 : 0);
		YMGUI_Draw_Text(s, font, abs->x + box + 6, ty, d->text, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0));
	}
}

/**
  * @brief 事件:点击切换 checked,标脏并回调
  */
static void cbEventCb(GYOBJ obj, GYEvent e)
{
	GYcb_data* d = (GYcb_data*)obj->user_data;
	if (e == GY_EVENT_Clicked)
	{
		d->checked = !d->checked;
		YMGUI_Obj_Invalidate(obj);
		if (d->changed != NULL)
			d->changed(obj, d->checked);
	}
}

static void cbFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建复选框
  */
GYOBJ YMGUI_Creat_Checkbox_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ cb = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(cb);
	if (cb == NULL)
		return NULL;
	GYcb_data* d = (GYcb_data*)GY_malloc0(sizeof(GYcb_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "复选框数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(cb); return NULL; }
	d->checked = 0;
	d->text[0] = '\0';
	d->changed = NULL;

	cb->type = GY_OBJ_Base;
	cb->user_data = d;
	cb->draw_cb = cbDrawCb;
	cb->event_cb = cbEventCb;
	cb->free_cb = cbFreeCb;
	cb->bg_color = GY_ARGB(0xFF, 0x18, 0x18, 0x20);
	YMGUI_Obj_Invalidate(cb);
	return cb;
}

/**
  * @brief 设标签文字
  */
void YMGUI_Checkbox_SetText(GYOBJ cb, const char* text)
{
	gy_assert(cb && cb->user_data && text);
	gy_log_explain((cb == NULL) || (cb->user_data == NULL) || (text == NULL), GY_LOG_PtrI, "复选框/数据/文本不存在");
	GYcb_data* d = (GYcb_data*)cb->user_data;
	uint16 i = 0;
	while (text[i] != '\0' && i < GY_CB_TEXT_MAX - 1)
	{
		d->text[i] = text[i];
		i++;
	}
	d->text[i] = '\0';
	YMGUI_Obj_Invalidate(cb);
}

/**
  * @brief 设勾选态
  */
void YMGUI_Checkbox_SetChecked(GYOBJ cb, uint8 checked)
{
	gy_assert(cb && cb->user_data);
	gy_log_explain((cb == NULL) || (cb->user_data == NULL), GY_LOG_PtrI, "复选框或数据不存在");
	((GYcb_data*)cb->user_data)->checked = checked ? 1 : 0;
	YMGUI_Obj_Invalidate(cb);
}

/**
  * @brief 读勾选态
  */
uint8 YMGUI_Checkbox_GetChecked(GYOBJ cb)
{
	gy_assert(cb && cb->user_data);
	gy_log_explain((cb == NULL) || (cb->user_data == NULL), GY_LOG_PtrI, "复选框或数据不存在");
	return ((GYcb_data*)cb->user_data)->checked;
}

/**
  * @brief 设值变回调
  */
void YMGUI_Checkbox_SetChanged(GYOBJ cb, GYcb_changed_cb cb_fn)
{
	gy_assert(cb && cb->user_data);
	gy_log_explain((cb == NULL) || (cb->user_data == NULL), GY_LOG_PtrI, "复选框或数据不存在");
	((GYcb_data*)cb->user_data)->changed = cb_fn;
}
