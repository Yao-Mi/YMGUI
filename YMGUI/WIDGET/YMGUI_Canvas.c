#include "YMGUI_Canvas.h"

#if YMGUI_CANVAS

#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawPx.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Canvas.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 可写像素缓冲的缩放位图视口。整数缩放/平移 + 屏->画布坐标映射 + 绘制意图回调。
  *	              图层栈/融合/合成在 app 侧(见 project_Demo/image_edit)。
  *	@Version:     1.0
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define DRAG_THRESH 2  //平移模式下超此像素才算拖动(小,画布交互要跟手)

typedef struct
{
	GYpx*  buf;      //显示缓冲 cw*ch(拥有)
	uint16 cw, ch;   //画布像素分辨率
	uint8  zoom;     //整数缩放
	int32  pan_x, pan_y;//画布左上相对视口左上的像素偏移
	uint8  pan_mode; //1=拖动平移,0=拖动绘制

	GYcanvas_paint_cb paint_cb;
	//交互态
	uint8  drawing;  //正在绘制(按下且非平移)
	int32  last_cx, last_cy;//最近画布坐标(供 IsDrawing / 喷枪逐帧)
	//平移拖动锚点
	GYcoord drag_x, drag_y;
	int32  drag_pan_x, drag_pan_y;
}GYcanvas_data;

//视口内容盒(去掉 1px 边框,画布 blit 与命中都在此内)
static void contentBox(const GYrect* abs, GYrect* out)
{
	out->x = abs->x + 1;
	out->y = abs->y + 1;
	out->w = (abs->w > 2) ? abs->w - 2 : 1;
	out->h = (abs->h > 2) ? abs->h - 2 : 1;
}
//===========================================================================
// 绘制:棋盘中性底 + 画布内容按 zoom 最近邻放大 blit(只画可见交集)
//===========================================================================
static void canvasDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYcanvas_data* d = (GYcanvas_data*)obj->user_data;
	//视口边框 + 背景
	YMGUI_Draw_Fill(s, abs, obj->bg_color, GY_OPA_COVER);

	GYrect box;
	contentBox(abs, &box);
	GYrect vis;
	if (!GY_Rect_Intersect(&vis, &box, &s->clip))
		return;

	//画布在屏幕上的矩形(左上 = 内容盒左上 + pan,尺寸 = 画布像素×zoom)
	int32 cx0 = box.x + d->pan_x;
	int32 cy0 = box.y + d->pan_y;
	int32 z = d->zoom;

	for (GYcoord sy = vis.y; sy < vis.y + vis.h; sy++)
	{
		//屏幕行 → 画布行
		int32 cyf = (int32)sy - cy0;
		int32 cyp = (cyf >= 0) ? (cyf / z) : -1;
		for (GYcoord sx = vis.x; sx < vis.x + vis.w; sx++)
		{
			int32 cxf = (int32)sx - cx0;
			int32 cxp = (cxf >= 0) ? (cxf / z) : -1;
			GYpx px;
			if (cxp >= 0 && cxp < d->cw && cyp >= 0 && cyp < d->ch)
			{
				px = d->buf[(int32)cyp * d->cw + cxp];
			}
			else
			{
				//画布外:中性底(纯灰,不做棋盘避免与内容混淆;透明棋盘由 app 合进 buf)
				px = GY_ColorToPx(GY_ARGB(0xFF, 0x2A, 0x2A, 0x30));
			}
			GY_PutPx(s, sx, sy, px);
		}
	}
}

//===========================================================================
// 坐标映射 + 交互
//===========================================================================
uint8 YMGUI_Canvas_ScreenToCanvas(GYOBJ canvas, GYcoord sx, GYcoord sy, int32* cx, int32* cy)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return 0;
	GYcanvas_data* d = (GYcanvas_data*)canvas->user_data;
	GYrect abs, box;
	YMGUI_Obj_GetAbsArea(canvas, &abs);
	contentBox(&abs, &box);
	int32 cx0 = box.x + d->pan_x;
	int32 cy0 = box.y + d->pan_y;
	int32 fx = (int32)sx - cx0;
	int32 fy = (int32)sy - cy0;
	//负坐标 floor 除法(避免 -1/z==0 把画布外错判成第0像素)
	int32 pcx = (fx >= 0) ? (fx / d->zoom) : ((fx - d->zoom + 1) / d->zoom);
	int32 pcy = (fy >= 0) ? (fy / d->zoom) : ((fy - d->zoom + 1) / d->zoom);
	if (cx) *cx = pcx;
	if (cy) *cy = pcy;
	return (pcx >= 0 && pcx < d->cw && pcy >= 0 && pcy < d->ch) ? 1 : 0;
}

static void emitPaint(GYOBJ obj, GYcanvas_data* d, GYcanvas_phase phase)
{
	int32 cx, cy;
	uint8 in = YMGUI_Canvas_ScreenToCanvas(obj, obj->ctx->point_x, obj->ctx->point_y, &cx, &cy);
	d->last_cx = cx; d->last_cy = cy;
	if (d->paint_cb != NULL)
		d->paint_cb(obj, phase, cx, cy, in);
}

static void canvasEventCb(GYOBJ obj, GYEvent e)
{
	GYcanvas_data* d = (GYcanvas_data*)obj->user_data;
	switch (e)
	{
	case GY_EVENT_Pressed:
		if (d->pan_mode)
		{
			d->drag_x = obj->ctx->point_x;
			d->drag_y = obj->ctx->point_y;
			d->drag_pan_x = d->pan_x;
			d->drag_pan_y = d->pan_y;
		}
		else
		{
			d->drawing = 1;
			emitPaint(obj, d, GY_CANVAS_DOWN);
		}
		break;
	case GY_EVENT_Pressing:
		if (d->pan_mode)
		{
			int32 dx = (int32)obj->ctx->point_x - d->drag_x;
			int32 dy = (int32)obj->ctx->point_y - d->drag_y;
			YMGUI_Canvas_SetPan(obj, d->drag_pan_x + dx, d->drag_pan_y + dy);
		}
		else if (d->drawing)
		{
			emitPaint(obj, d, GY_CANVAS_MOVE);
		}
		break;
	case GY_EVENT_Released:
	case GY_EVENT_ReleasedOff:
		if (d->drawing)
		{
			emitPaint(obj, d, GY_CANVAS_UP);
			d->drawing = 0;
		}
		break;
	default:
		break;
	}
}

static void canvasFreeCb(GYOBJ obj)
{
	GYcanvas_data* d = (GYcanvas_data*)obj->user_data;
	if (d != NULL)
	{
		if (d->buf != NULL)
			GY_free1(d->buf);
		GY_free0(d);
		obj->user_data = NULL;
	}
}
//===========================================================================
// 公共 API
//===========================================================================
GYOBJ YMGUI_Creat_Canvas_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h,
                               uint16 cw, uint16 ch)
{
	GYOBJ obj = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(obj);
	if (obj == NULL)
		return NULL;
	GYcanvas_data* d = (GYcanvas_data*)GY_malloc0(sizeof(GYcanvas_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "画布数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(obj); return NULL; }
	GY_memset(d, 0, sizeof(GYcanvas_data));

	if (cw == 0) cw = 1;
	if (ch == 0) ch = 1;
	d->cw = cw; d->ch = ch;
	d->buf = (GYpx*)GY_malloc1((size_t)cw * ch * sizeof(GYpx));
	gy_assert(d->buf);
	gy_log_explain(d->buf == NULL, GY_LOG_Mem1, "画布像素缓冲申请失败");
	if (d->buf == NULL) { GY_free0(d); YMGUI_Free_ObjFree(obj); return NULL; }
	//初值:白(app 通常随后合成覆盖)
	GYpx white = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	for (size_t i = 0; i < (size_t)cw * ch; i++)
		d->buf[i] = white;

	d->zoom = 1;
	d->pan_x = 0; d->pan_y = 0;
	d->pan_mode = 0;
	d->paint_cb = NULL;
	d->drawing = 0;
	d->last_cx = 0; d->last_cy = 0;

	obj->type = GY_OBJ_Base;
	obj->user_data = d;
	obj->draw_cb = canvasDrawCb;
	obj->event_cb = canvasEventCb;
	obj->free_cb = canvasFreeCb;
	obj->bg_color = GY_ARGB(0xFF, 0x10, 0x10, 0x14);
	YMGUI_Obj_Invalidate(obj);
	return obj;
}

uint16 YMGUI_Canvas_GetW(GYOBJ canvas)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return 0;
	return ((GYcanvas_data*)canvas->user_data)->cw;
}

uint16 YMGUI_Canvas_GetH(GYOBJ canvas)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return 0;
	return ((GYcanvas_data*)canvas->user_data)->ch;
}

GYpx* YMGUI_Canvas_GetBuffer(GYOBJ canvas)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return NULL;
	return ((GYcanvas_data*)canvas->user_data)->buf;
}

void YMGUI_Canvas_Invalidate(GYOBJ canvas)
{
	YMGUI_Obj_Invalidate(canvas);
}

void YMGUI_Canvas_SetZoom(GYOBJ canvas, uint8 zoom)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return;
	GYcanvas_data* d = (GYcanvas_data*)canvas->user_data;
	if (zoom < 1) zoom = 1;
	if (zoom > GY_CANVAS_ZOOM_MAX) zoom = GY_CANVAS_ZOOM_MAX;
	d->zoom = zoom;
	YMGUI_Obj_Invalidate(canvas);
}

uint8 YMGUI_Canvas_GetZoom(GYOBJ canvas)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return 1;
	return ((GYcanvas_data*)canvas->user_data)->zoom;
}

void YMGUI_Canvas_SetPan(GYOBJ canvas, int32 pan_x, int32 pan_y)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return;
	GYcanvas_data* d = (GYcanvas_data*)canvas->user_data;
	//钳制:至少留 16px 画布可见(防止把画布拖出视口彻底看不见)
	GYrect abs, box;
	YMGUI_Obj_GetAbsArea(canvas, &abs);
	contentBox(&abs, &box);
	int32 span_w = (int32)d->cw * d->zoom;
	int32 span_h = (int32)d->ch * d->zoom;
	int32 margin = 16;
	int32 min_x = -(span_w - margin);
	int32 max_x = box.w - margin;
	int32 min_y = -(span_h - margin);
	int32 max_y = box.h - margin;
	if (min_x > max_x) { min_x = max_x = (box.w - span_w) / 2; }//画布比视口小时居中范围
	if (min_y > max_y) { min_y = max_y = (box.h - span_h) / 2; }
	d->pan_x = GYLimitMaxMin(min_x, pan_x, max_x);
	d->pan_y = GYLimitMaxMin(min_y, pan_y, max_y);
	YMGUI_Obj_Invalidate(canvas);
}

void YMGUI_Canvas_GetPan(GYOBJ canvas, int32* pan_x, int32* pan_y)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return;
	GYcanvas_data* d = (GYcanvas_data*)canvas->user_data;
	if (pan_x) *pan_x = d->pan_x;
	if (pan_y) *pan_y = d->pan_y;
}

void YMGUI_Canvas_SetPanMode(GYOBJ canvas, uint8 on)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return;
	((GYcanvas_data*)canvas->user_data)->pan_mode = on ? 1 : 0;
}

uint8 YMGUI_Canvas_GetPanMode(GYOBJ canvas)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return 0;
	return ((GYcanvas_data*)canvas->user_data)->pan_mode;
}

uint8 YMGUI_Canvas_IsDrawing(GYOBJ canvas, int32* cx, int32* cy)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return 0;
	GYcanvas_data* d = (GYcanvas_data*)canvas->user_data;
	if (cx) *cx = d->last_cx;
	if (cy) *cy = d->last_cy;
	return d->drawing;
}

void YMGUI_Canvas_SetPaintCb(GYOBJ canvas, GYcanvas_paint_cb cb)
{
	gy_assert(canvas && canvas->user_data);
	if (canvas == NULL || canvas->user_data == NULL) return;
	((GYcanvas_data*)canvas->user_data)->paint_cb = cb;
}

#endif // YMGUI_CANVAS
