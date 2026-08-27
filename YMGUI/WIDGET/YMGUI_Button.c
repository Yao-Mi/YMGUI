#include "YMGUI_Button.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawImg.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Button.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 按钮控件。draw_cb 按状态换色 + 居中标题,event_cb 收 Pressed/Released 标脏,Clicked 转调用户回调
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

//按钮私有数据(挂在 obj->user_data,走小内存)
typedef struct
{
	GYcolor          normal;  //常态色
	GYcolor          pressed; //按下态色
	GYbtn_clicked_cb clicked; //用户点击回调
	char             text[GY_BTN_TEXT_MAX];//标题文字
	GYIMG            src;     //图源(不拥有;非 NULL 则贴图不画文字)
	uint8            draw_bg; //底色+边框是否画(默认 1)
	uint32           repeat_delay;
	uint32           repeat_interval;
	uint32           repeat_elapsed;
	uint8            repeat_active;
	uint8            repeat_fired;
}GYbtn_data;

static uint8 btnRepeatTick(GYOBJ btn);

/**
  * @brief 按钮绘制:按状态选色填充,画边框,再居中标题文字
  */
static void btnDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYbtn_data* d = (GYbtn_data*)obj->user_data;
	//底色+边框(可关:纯图标/透明按钮)
	if (d->draw_bg)
	{
		GYcolor fill = (obj->state & GY_STATE_Pressed) ? d->pressed : d->normal;
		YMGUI_Draw_Fill(s, abs, fill, GY_OPA_COVER);
		//边框(上下左右各 2px 深色)
		GYcolor border = GY_ARGB(0xFF, 0x20, 0x20, 0x20);
		GYrect top = {abs->x, abs->y, abs->w, 2};
		GYrect bot = {abs->x, abs->y + abs->h - 2, abs->w, 2};
		GYrect lft = {abs->x, abs->y, 2, abs->h};
		GYrect rgt = {abs->x + abs->w - 2, abs->y, 2, abs->h};
		YMGUI_Draw_Fill(s, &top, border, GY_OPA_COVER);
		YMGUI_Draw_Fill(s, &bot, border, GY_OPA_COVER);
		YMGUI_Draw_Fill(s, &lft, border, GY_OPA_COVER);
		YMGUI_Draw_Fill(s, &rgt, border, GY_OPA_COVER);
	}
	//内容必须裁在按钮内。首帧通常是整屏脏区，不能依赖脏区恰好等于按钮区域来兜底。
	GYrect saved_clip = s->clip;
	GYrect content_clip;
	if (!GY_Rect_Intersect(&content_clip, abs, &saved_clip))
		return;
	s->clip = content_clip;
	//图优先:设了图源就居中 blit,不画文字(播放/暂停等状态由 app 换图)
	if (d->src != NULL && d->src->data != NULL)
	{
		GYcoord ix = abs->x + ((abs->w > d->src->w) ? (abs->w - d->src->w) / 2 : 0);
		GYcoord iy = abs->y + ((abs->h > d->src->h) ? (abs->h - d->src->h) / 2 : 0);
		YMGUI_Draw_Img(s, d->src, ix, iy);
	}
	//无图:居中标题
	else if (d->text[0] != '\0')
	{
		GYFONT font = &YMGUI_Font_Default;
		GYcoord tw = YMGUI_Font_TextWidth(font, d->text);
		GYcoord th = font->cell_h;
		GYcoord tx = abs->x + ((abs->w > tw) ? (abs->w - tw) / 2 : 0);
		GYcoord ty = abs->y + ((abs->h > th) ? (abs->h - th) / 2 : 0);
		YMGUI_Draw_Text(s, font, tx, ty, d->text, GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	}
	s->clip = saved_clip;
}

/**
  * @brief 按钮事件:按下/抬起时标脏(触发换色重绘),Clicked 转调用户回调
  */
static void btnEventCb(GYOBJ obj, GYEvent e)
{
	GYbtn_data* d = (GYbtn_data*)obj->user_data;
	switch (e)
	{
	case GY_EVENT_Pressed:
		d->repeat_elapsed = 0;
		d->repeat_active = 0;
		d->repeat_fired = 0;
		YMGUI_Obj_Invalidate(obj);//状态变了,重绘
		break;
	case GY_EVENT_Released:
		d->repeat_elapsed = 0;
		d->repeat_active = 0;
		YMGUI_Obj_Invalidate(obj);//状态变了,重绘
		break;
	case GY_EVENT_ReleasedOff:
		d->repeat_elapsed = 0;
		d->repeat_active = 0;
		d->repeat_fired = 0;
		YMGUI_Obj_Invalidate(obj);//状态变了,重绘
		break;
	case GY_EVENT_Clicked:
		if (!d->repeat_fired && d->clicked != NULL)
			d->clicked(obj);
		d->repeat_fired = 0;
		break;
	case GY_EVENT_Tick:
		btnRepeatTick(obj);
		break;
	default:
		break;
	}
}

/**
  * @brief 按钮析构:释放私有数据
  */
static void btnFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建按钮
  */
GYOBJ YMGUI_Creat_Button_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ btn = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(btn);
	if (btn == NULL)
		return NULL;

	GYbtn_data* d = (GYbtn_data*)GY_malloc0(sizeof(GYbtn_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "按钮数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(btn); return NULL; }
	GY_memset(d, 0, sizeof(GYbtn_data));
	d->normal  = GY_ARGB(0xFF, 0x40, 0x80, 0xC0);
	d->pressed = GY_ARGB(0xFF, 0x20, 0x50, 0x90);
	d->clicked = NULL;
	d->text[0] = '\0';
	d->src     = NULL;
	d->draw_bg = 1;
	d->repeat_delay = 0;
	d->repeat_interval = 0;
	d->repeat_elapsed = 0;
	d->repeat_active = 0;
	d->repeat_fired = 0;

	btn->type = GY_OBJ_Button;
	btn->user_data = d;
	btn->draw_cb = btnDrawCb;
	btn->event_cb = btnEventCb;
	btn->free_cb = btnFreeCb;
	YMGUI_Obj_Invalidate(btn);
	return btn;
}

/**
  * @brief 设置常态/按下态颜色
  */
void YMGUI_Button_SetColors(GYOBJ btn, GYcolor normal, GYcolor pressed)
{
	gy_assert(btn && btn->user_data);
	gy_log_explain((btn == NULL) || (btn->user_data == NULL), GY_LOG_PtrI, "按钮或其数据不存在");
	GYbtn_data* d = (GYbtn_data*)btn->user_data;
	d->normal = normal;
	d->pressed = pressed;
	YMGUI_Obj_Invalidate(btn);
}

/**
  * @brief 设置点击回调
  */
void YMGUI_Button_SetClicked(GYOBJ btn, GYbtn_clicked_cb cb)
{
	gy_assert(btn && btn->user_data);
	gy_log_explain((btn == NULL) || (btn->user_data == NULL), GY_LOG_PtrI, "按钮或其数据不存在");
	((GYbtn_data*)btn->user_data)->clicked = cb;
}

void YMGUI_Button_SetRepeat(GYOBJ btn, uint32 delay_ms, uint32 interval_ms)
{
	gy_assert(btn && btn->user_data);
	gy_log_explain((btn == NULL) || (btn->user_data == NULL), GY_LOG_PtrI, "按钮或其数据不存在");
	if (btn == NULL || btn->user_data == NULL)
		return;
	GYbtn_data* d = (GYbtn_data*)btn->user_data;
	d->repeat_delay = delay_ms;
	d->repeat_interval = interval_ms;
	d->repeat_elapsed = 0;
	d->repeat_active = 0;
	if (!(btn->state & GY_STATE_Pressed))
		d->repeat_fired = 0;
}

static uint8 btnRepeatTick(GYOBJ btn)
{
	GYbtn_data* d = (GYbtn_data*)btn->user_data;
	if (d->repeat_delay == 0 || d->repeat_interval == 0)
		return 0;

	//拖出按钮后暂停并重置延迟；已发生过连发的标志保留，避免拖回抬起又补一次 Clicked。
	GYOBJ hit = YMGUI_HitTest(btn->ctx, btn->ctx->point_x, btn->ctx->point_y);
	while (hit != NULL && hit != btn)
		hit = hit->parent;
	if (hit != btn)
	{
		d->repeat_elapsed = 0;
		d->repeat_active = 0;
		return 0;
	}

	uint32 elapsed_ms = btn->ctx->tick_elapsed;
	uint32 threshold = d->repeat_active ? d->repeat_interval : d->repeat_delay;
	uint32 remaining = UINT32_MAX - d->repeat_elapsed;
	uint32 total = elapsed_ms > remaining ? UINT32_MAX : d->repeat_elapsed + elapsed_ms;
	if (total < threshold)
	{
		d->repeat_elapsed = total;
		return 0;
	}
	d->repeat_elapsed = (total - threshold) % d->repeat_interval;
	d->repeat_active = 1;
	d->repeat_fired = 1;
	if (d->clicked != NULL)
		d->clicked(btn);
	return 1;
}

/**
  * @brief 设置按钮标题文字(拷贝,截断到上限)
  */
void YMGUI_Button_SetText(GYOBJ btn, const char* text)
{
	gy_assert(btn && btn->user_data && text);
	gy_log_explain((btn == NULL) || (btn->user_data == NULL) || (text == NULL), GY_LOG_PtrI, "按钮/数据/文本不存在");
	GYbtn_data* d = (GYbtn_data*)btn->user_data;
	uint16 i = 0;
	while (text[i] != '\0' && i < GY_BTN_TEXT_MAX - 1)
	{
		d->text[i] = text[i];
		i++;
	}
	d->text[i] = '\0';
	YMGUI_Obj_Invalidate(btn);
}

/**
  * @brief 设置按钮图源(居中 blit,不拥有像素;NULL=清图回退文字)
  */
void YMGUI_Button_SetImage(GYOBJ btn, GYIMG src)
{
	gy_assert(btn && btn->user_data);
	gy_log_explain((btn == NULL) || (btn->user_data == NULL), GY_LOG_PtrI, "按钮或其数据不存在");
	if (btn == NULL || btn->user_data == NULL)
		return;
	((GYbtn_data*)btn->user_data)->src = src;
	YMGUI_Obj_Invalidate(btn);
}

/**
  * @brief 底色+边框是否绘制(默认开;关掉即纯图标/透明按钮)
  */
void YMGUI_Button_SetBgVisible(GYOBJ btn, uint8 on)
{
	gy_assert(btn && btn->user_data);
	gy_log_explain((btn == NULL) || (btn->user_data == NULL), GY_LOG_PtrI, "按钮或其数据不存在");
	if (btn == NULL || btn->user_data == NULL)
		return;
	((GYbtn_data*)btn->user_data)->draw_bg = on ? 1 : 0;
	YMGUI_Obj_Invalidate(btn);
}
