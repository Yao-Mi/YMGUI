#include "YMGUI_ColorPicker.h"

#if YMGUI_COLORPICKER

#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawPx.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_ColorPicker.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: HSV 取色器。左 SV 方块 + 右 色相条,整数 HSV<->RGB(无 FPU)。自绘型控件。
  *	@Version:     1.0
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define HUE_BAR_W  18   //色相条宽
#define GAP        6    //方块与色相条间距
#define MARK_R     4    //选中标记半径

typedef struct
{
	uint16 h;   //0..359
	uint8  s;   //0..255
	uint8  v;   //0..255
	GYcolorpicker_cb cb;
	//缓存的子区域(创建/尺寸决定),draw/命中共用。相对控件左上
	GYrect sv;   //SV 方块
	GYrect hue;  //色相条
}GYcp_data;

//===========================================================================
// 整数 HSV<->RGB
//===========================================================================
GYcolor YMGUI_ColorPicker_HSVtoRGB(uint16 h, uint8 s, uint8 v)
{
	h %= 360;
	uint32 region = h / 60;          //0..5
	uint32 rem = (h % 60) * 255 / 60; //该区间内 0..254
	uint32 p = (uint32)v * (255 - s) / 255;
	uint32 q = (uint32)v * (255 - (uint32)s * rem / 255) / 255;
	uint32 t = (uint32)v * (255 - (uint32)s * (255 - rem) / 255) / 255;
	uint32 r, g, b;
	switch (region)
	{
	case 0:  r = v; g = t; b = p; break;
	case 1:  r = q; g = v; b = p; break;
	case 2:  r = p; g = v; b = t; break;
	case 3:  r = p; g = q; b = v; break;
	case 4:  r = t; g = p; b = v; break;
	default: r = v; g = p; b = q; break;
	}
	return GY_ARGB(0xFF, r, g, b);
}

void YMGUI_ColorPicker_RGBtoHSV(GYcolor color, uint16* h, uint8* s, uint8* v)
{
	int32 r = (int32)GY_COLOR_R(color);
	int32 g = (int32)GY_COLOR_G(color);
	int32 b = (int32)GY_COLOR_B(color);
	int32 mx = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
	int32 mn = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
	int32 delta = mx - mn;
	int32 hh = 0;
	if (delta != 0)
	{
		if (mx == r)      hh = 60 * (((g - b) * 1000 / delta)) / 1000;
		else if (mx == g) hh = 60 * (2000 + ((b - r) * 1000 / delta)) / 1000;
		else              hh = 60 * (4000 + ((r - g) * 1000 / delta)) / 1000;
		hh %= 360;
		if (hh < 0) hh += 360;
	}
	int32 ss = (mx == 0) ? 0 : (delta * 255 / mx);
	if (h) *h = (uint16)hh;
	if (s) *s = (uint8)ss;
	if (v) *v = (uint8)mx;
}
//===========================================================================
// 绘制
//===========================================================================
//实心小方块标记(带对比描边),屏幕坐标 cx,cy 为中心
static void drawMark(GYSURFACE s, GYcoord cx, GYcoord cy, GYcolor inner)
{
	GYrect out = { cx - MARK_R, cy - MARK_R, MARK_R * 2 + 1, MARK_R * 2 + 1 };
	GYrect in  = { cx - MARK_R + 1, cy - MARK_R + 1, MARK_R * 2 - 1, MARK_R * 2 - 1 };
	YMGUI_Draw_Fill(s, &out, GY_ARGB(0xFF, 0x00, 0x00, 0x00), GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &in, GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF), GY_OPA_COVER);
	GYrect c = { cx - MARK_R + 2, cy - MARK_R + 2, MARK_R * 2 - 3, MARK_R * 2 - 3 };
	YMGUI_Draw_Fill(s, &c, inner, GY_OPA_COVER);
}

static void cpDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYcp_data* d = (GYcp_data*)obj->user_data;
	//SV 方块屏幕矩形
	GYrect svr = { abs->x + d->sv.x, abs->y + d->sv.y, d->sv.w, d->sv.h };
	GYrect hur = { abs->x + d->hue.x, abs->y + d->hue.y, d->hue.w, d->hue.h };

	//SV 方块:x=饱和度(0..255) y=明度(255..0),色相取当前 H。逐像素(只画可见交集)
	GYrect vis;
	if (GY_Rect_Intersect(&vis, &svr, &s->clip))
	{
		for (GYcoord y = vis.y; y < vis.y + vis.h; y++)
		{
			int32 vv = (svr.h <= 1) ? 255 : (255 - (int32)(y - svr.y) * 255 / (svr.h - 1));
			for (GYcoord x = vis.x; x < vis.x + vis.w; x++)
			{
				int32 ss = (svr.w <= 1) ? 0 : (int32)(x - svr.x) * 255 / (svr.w - 1);
				GY_PutPx(s, x, y, GY_ColorToPx(YMGUI_ColorPicker_HSVtoRGB(d->h, (uint8)ss, (uint8)vv)));
			}
		}
	}
	//色相条:y=色相(0..359)
	if (GY_Rect_Intersect(&vis, &hur, &s->clip))
	{
		for (GYcoord y = vis.y; y < vis.y + vis.h; y++)
		{
			int32 hh = (hur.h <= 1) ? 0 : (int32)(y - hur.y) * 359 / (hur.h - 1);
			GYpx px = GY_ColorToPx(YMGUI_ColorPicker_HSVtoRGB((uint16)hh, 255, 255));
			for (GYcoord x = vis.x; x < vis.x + vis.w; x++)
				GY_PutPx(s, x, y, px);
		}
	}
	//标记:SV 方块内当前 (s,v);色相条内当前 h
	GYcoord mx = svr.x + (svr.w <= 1 ? 0 : (GYcoord)((int32)d->s * (svr.w - 1) / 255));
	GYcoord my = svr.y + (svr.h <= 1 ? 0 : (GYcoord)((int32)(255 - d->v) * (svr.h - 1) / 255));
	drawMark(s, mx, my, YMGUI_ColorPicker_HSVtoRGB(d->h, d->s, d->v));
	GYcoord hy = hur.y + (hur.h <= 1 ? 0 : (GYcoord)((int32)d->h * (hur.h - 1) / 359));
	drawMark(s, hur.x + hur.w / 2, hy, YMGUI_ColorPicker_HSVtoRGB(d->h, 255, 255));
}
//===========================================================================
// 交互:点击/拖动 SV 方块或色相条
//===========================================================================
//把指针位置吸进 SV 方块或色相条,更新 HSV。返回 1 表示确有变化
static uint8 applyPointer(GYOBJ obj, GYcp_data* d)
{
	GYrect abs;
	YMGUI_Obj_GetAbsArea(obj, &abs);
	GYcoord px = obj->ctx->point_x;
	GYcoord py = obj->ctx->point_y;
	GYrect svr = { abs.x + d->sv.x, abs.y + d->sv.y, d->sv.w, d->sv.h };
	GYrect hur = { abs.x + d->hue.x, abs.y + d->hue.y, d->hue.w, d->hue.h };
	uint16 oh = d->h; uint8 os = d->s, ov = d->v;

	//优先判色相条(右侧窄条),否则落 SV 方块(其余区域一律钳进方块,拖出边界仍跟手)
	if (px >= hur.x - 2 && px < hur.x + hur.w + 2)
	{
		GYcoord yy = GYLimitMaxMin(hur.y, py, hur.y + hur.h - 1);
		d->h = (hur.h <= 1) ? 0 : (uint16)((int32)(yy - hur.y) * 359 / (hur.h - 1));
	}
	else
	{
		GYcoord xx = GYLimitMaxMin(svr.x, px, svr.x + svr.w - 1);
		GYcoord yy = GYLimitMaxMin(svr.y, py, svr.y + svr.h - 1);
		d->s = (svr.w <= 1) ? 0 : (uint8)((int32)(xx - svr.x) * 255 / (svr.w - 1));
		d->v = (svr.h <= 1) ? 255 : (uint8)(255 - (int32)(yy - svr.y) * 255 / (svr.h - 1));
	}
	return (d->h != oh) || (d->s != os) || (d->v != ov);
}

static void cpEventCb(GYOBJ obj, GYEvent e)
{
	GYcp_data* d = (GYcp_data*)obj->user_data;
	switch (e)
	{
	case GY_EVENT_Pressed:
	case GY_EVENT_Pressing:
	{
		uint8 changed = applyPointer(obj, d);
		//首次按下即便未跨阈值也可能选中同色,一律标脏刷新标记
		YMGUI_Obj_Invalidate(obj);
		if (changed && d->cb != NULL)
			d->cb(obj, YMGUI_ColorPicker_HSVtoRGB(d->h, d->s, d->v));
		break;
	}
	default:
		break;
	}
}

static void cpFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

//===========================================================================
// 公共 API
//===========================================================================
GYOBJ YMGUI_Creat_ColorPicker_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ obj = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(obj);
	if (obj == NULL)
		return NULL;
	GYcp_data* d = (GYcp_data*)GY_malloc0(sizeof(GYcp_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "取色器数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(obj); return NULL; }
	GY_memset(d, 0, sizeof(GYcp_data));

	d->h = 0; d->s = 255; d->v = 255; d->cb = NULL;
	//切区:右侧 HUE_BAR_W 给色相条,其余给 SV 方块(相对控件左上)
	GYcoord svw = w - HUE_BAR_W - GAP;
	if (svw < 1) svw = 1;
	d->sv.x = 0;  d->sv.y = 0;  d->sv.w = svw; d->sv.h = h;
	d->hue.x = svw + GAP; d->hue.y = 0; d->hue.w = HUE_BAR_W; d->hue.h = h;

	obj->type = GY_OBJ_Base;
	obj->user_data = d;
	obj->draw_cb = cpDrawCb;
	obj->event_cb = cpEventCb;
	obj->free_cb = cpFreeCb;
	obj->bg_color = GY_ARGB(0xFF, 0x18, 0x18, 0x20);
	YMGUI_Obj_Invalidate(obj);
	return obj;
}

void YMGUI_ColorPicker_SetColor(GYOBJ picker, GYcolor color)
{
	gy_assert(picker && picker->user_data);
	if (picker == NULL || picker->user_data == NULL) return;
	GYcp_data* d = (GYcp_data*)picker->user_data;
	YMGUI_ColorPicker_RGBtoHSV(color, &d->h, &d->s, &d->v);
	YMGUI_Obj_Invalidate(picker);
}

GYcolor YMGUI_ColorPicker_GetColor(GYOBJ picker)
{
	gy_assert(picker && picker->user_data);
	if (picker == NULL || picker->user_data == NULL) return GY_ARGB(0xFF, 0, 0, 0);
	GYcp_data* d = (GYcp_data*)picker->user_data;
	return YMGUI_ColorPicker_HSVtoRGB(d->h, d->s, d->v);
}

void YMGUI_ColorPicker_SetHSV(GYOBJ picker, uint16 h, uint8 s, uint8 v)
{
	gy_assert(picker && picker->user_data);
	if (picker == NULL || picker->user_data == NULL) return;
	GYcp_data* d = (GYcp_data*)picker->user_data;
	d->h = h % 360; d->s = s; d->v = v;
	YMGUI_Obj_Invalidate(picker);
}

void YMGUI_ColorPicker_GetHSV(GYOBJ picker, uint16* h, uint8* s, uint8* v)
{
	gy_assert(picker && picker->user_data);
	if (picker == NULL || picker->user_data == NULL) return;
	GYcp_data* d = (GYcp_data*)picker->user_data;
	if (h) *h = d->h;
	if (s) *s = d->s;
	if (v) *v = d->v;
}

void YMGUI_ColorPicker_SetChangedCb(GYOBJ picker, GYcolorpicker_cb cb)
{
	gy_assert(picker && picker->user_data);
	if (picker == NULL || picker->user_data == NULL) return;
	((GYcp_data*)picker->user_data)->cb = cb;
}

#endif // YMGUI_COLORPICKER
