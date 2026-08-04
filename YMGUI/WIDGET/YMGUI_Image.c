#include "YMGUI_Image.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Image.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 图片控件。背景填充后在控件区居中 blit GYimg。不复制像素数据
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

typedef struct
{
	GYIMG src;      //图片源(不拥有)
	uint8 draw_bg;  //是否填背景
	uint8 mode;     //缩放模式 GYimg_scale_mode
}GYimg_data;

/**
  * @brief 绘制:可选背景 + 按缩放模式显示
  *   NONE 居中原尺寸(旧行为);FIT 等比适配留黑边;FILL 拉伸铺满
  */
static void imgDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYimg_data* d = (GYimg_data*)obj->user_data;
	if (d->draw_bg)
		YMGUI_Draw_Fill(s, abs, obj->bg_color, GY_OPA_COVER);
	if (d->src == NULL || d->src->data == NULL || d->src->w <= 0 || d->src->h <= 0)
		return;

	if (d->mode == GY_IMG_NONE)
	{
		//原尺寸居中(大图溢出由裁剪挡住)
		GYcoord ix = abs->x + ((abs->w > d->src->w) ? (abs->w - d->src->w) / 2 : 0);
		GYcoord iy = abs->y + ((abs->h > d->src->h) ? (abs->h - d->src->h) / 2 : 0);
		YMGUI_Draw_Img(s, d->src, ix, iy);
		return;
	}

	GYrect dst;
	if (d->mode == GY_IMG_FILL)
	{
		//拉伸铺满内容盒
		dst = *abs;
	}
	else //GY_IMG_FIT:等比缩放到刚好放进内容盒,居中留黑边
	{
		//按较小比例定缩放尺寸(定点:sw = img_w * min(box_w/img_w, box_h/img_h))
		//  用整数比较 img_w*box_h 与 img_h*box_w 判定哪个方向受限,避免浮点
		int32 iw = d->src->w, ih = d->src->h;
		int32 bw = abs->w, bh = abs->h;
		int32 dw, dh;
		if ((int64)iw * bh <= (int64)ih * bw)
		{
			//高度受限:铺满高,宽按比例
			dh = bh;
			dw = (int32)((int64)iw * bh / ih);
		}
		else
		{
			//宽度受限:铺满宽,高按比例
			dw = bw;
			dh = (int32)((int64)ih * bw / iw);
		}
		if (dw < 1) dw = 1;
		if (dh < 1) dh = 1;
		dst.x = abs->x + (bw - dw) / 2;
		dst.y = abs->y + (bh - dh) / 2;
		dst.w = (GYcoord)dw;
		dst.h = (GYcoord)dh;
	}
	YMGUI_Draw_ImgScaled(s, d->src, dst);
}

static void imgFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 创建图片控件
  */
GYOBJ YMGUI_Creat_Image_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ obj = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(obj);
	if (obj == NULL)
		return NULL;
	GYimg_data* d = (GYimg_data*)GY_malloc0(sizeof(GYimg_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "图片控件数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(obj); return NULL; }
	d->src = NULL;
	d->draw_bg = 1;
	d->mode = GY_IMG_NONE;

	obj->type = GY_OBJ_Base;
	obj->user_data = d;
	obj->draw_cb = imgDrawCb;
	obj->event_cb = NULL;
	obj->free_cb = imgFreeCb;
	obj->bg_color = GY_ARGB(0xFF, 0x18, 0x18, 0x20);
	YMGUI_Obj_Invalidate(obj);
	return obj;
}

/**
  * @brief 设图片源
  */
void YMGUI_Image_SetSrc(GYOBJ img, GYIMG src)
{
	gy_assert(img && img->user_data);
	gy_log_explain((img == NULL) || (img->user_data == NULL), GY_LOG_PtrI, "图片控件或数据不存在");
	((GYimg_data*)img->user_data)->src = src;
	YMGUI_Obj_Invalidate(img);
}

/**
  * @brief 设缩放模式(标脏)
  */
void YMGUI_Image_SetScaleMode(GYOBJ img, GYimg_scale_mode mode)
{
	gy_assert(img && img->user_data);
	gy_log_explain((img == NULL) || (img->user_data == NULL), GY_LOG_PtrI, "图片控件或数据不存在");
	if (img == NULL || img->user_data == NULL) return;
	((GYimg_data*)img->user_data)->mode = (uint8)mode;
	YMGUI_Obj_Invalidate(img);
}

/**
  * @brief 取缩放模式
  */
GYimg_scale_mode YMGUI_Image_GetScaleMode(GYOBJ img)
{
	gy_assert(img && img->user_data);
	if (img == NULL || img->user_data == NULL) return GY_IMG_NONE;
	return (GYimg_scale_mode)((GYimg_data*)img->user_data)->mode;
}
