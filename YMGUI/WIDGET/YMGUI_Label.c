#include "YMGUI_Label.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Label.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 文本标签控件。draw_cb 可选填背景后居中绘制文本(默认字体)
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

//标签私有数据
typedef struct
{
	char    text[GY_LABEL_TEXT_MAX];
	GYcolor text_color;
	uint8   bg_enable;   //是否画背景(用 obj->bg_color)
}GYlabel_data;

/**
  * @brief 标签绘制:可选背景 + 文本在控件内水平/垂直居中
  */
static void labelDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYlabel_data* d = (GYlabel_data*)obj->user_data;
	if (d->bg_enable)
		YMGUI_Draw_Fill(s, abs, obj->bg_color, GY_OPA_COVER);

	GYFONT font = &YMGUI_Font_Default;
	GYcoord tw = YMGUI_Font_TextWidth(font, d->text);
	GYcoord th = font->cell_h;
	//居中:超出则从左上对齐
	GYcoord tx = abs->x + ((abs->w > tw) ? (abs->w - tw) / 2 : 0);
	GYcoord ty = abs->y + ((abs->h > th) ? (abs->h - th) / 2 : 0);
	YMGUI_Draw_Text(s, font, tx, ty, d->text, d->text_color);
}

/**
  * @brief 标签析构
  */
static void labelFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建标签
  */
GYOBJ YMGUI_Creat_Label_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ lb = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(lb);
	if (lb == NULL)
		return NULL;

	GYlabel_data* d = (GYlabel_data*)GY_malloc0(sizeof(GYlabel_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "标签数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(lb); return NULL; }
	d->text[0] = '\0';
	d->text_color = GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0);
	d->bg_enable = 0;

	lb->type = GY_OBJ_Label;
	lb->user_data = d;
	lb->draw_cb = labelDrawCb;
	lb->free_cb = labelFreeCb;
	lb->event_cb = NULL;//标签默认不响应事件
	YMGUI_Obj_Invalidate(lb);
	return lb;
}

/**
  * @brief 设置文本(拷贝,截断到上限)
  */
void YMGUI_Label_SetText(GYOBJ label, const char* text)
{
	gy_assert(label && label->user_data && text);
	gy_log_explain((label == NULL) || (label->user_data == NULL) || (text == NULL), GY_LOG_PtrI, "标签/数据/文本不存在");
	GYlabel_data* d = (GYlabel_data*)label->user_data;
	uint16 i = 0;
	while (text[i] != '\0' && i < GY_LABEL_TEXT_MAX - 1)
	{
		d->text[i] = text[i];
		i++;
	}
	d->text[i] = '\0';
	YMGUI_Obj_Invalidate(label);
}

/**
  * @brief 设置文字颜色
  */
void YMGUI_Label_SetTextColor(GYOBJ label, GYcolor color)
{
	gy_assert(label && label->user_data);
	gy_log_explain((label == NULL) || (label->user_data == NULL), GY_LOG_PtrI, "标签或数据不存在");
	((GYlabel_data*)label->user_data)->text_color = color;
	YMGUI_Obj_Invalidate(label);
}

/**
  * @brief 设置是否画背景
  */
void YMGUI_Label_SetBgEnable(GYOBJ label, uint8 enable)
{
	gy_assert(label && label->user_data);
	gy_log_explain((label == NULL) || (label->user_data == NULL), GY_LOG_PtrI, "标签或数据不存在");
	((GYlabel_data*)label->user_data)->bg_enable = enable;
	YMGUI_Obj_Invalidate(label);
}
