#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Canvas.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_canvas.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 画布控件单测:分辨率、显示缓冲直写、缩放/平移、屏->画布坐标映射(含 zoom/pan/越界/
  *	              负坐标 floor 除)、绘制回调(DOWN/MOVE/UP + in_bounds)、平移模式拖动、IsDrawing 查询、
  *	              渲染不崩、析构不崩。判成败以 exit code 为准。
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 240

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static GYpx g_fb[SCR_W * SCR_H];
static void fbFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d;
	for (GYcoord yy = 0; yy < a->h; yy++)
		for (GYcoord xx = 0; xx < a->w; xx++)
		{
			GYcoord sx = a->x + xx, sy = a->y + yy;
			if (sx >= 0 && sx < SCR_W && sy >= 0 && sy < SCR_H)
				g_fb[sy * SCR_W + sx] = b[yy * a->w + xx];
		}
}

//绘制回调记录
static int g_paint_calls = 0;
static GYcanvas_phase g_last_phase;
static int32 g_last_cx, g_last_cy;
static uint8 g_last_in;
static void onPaint(GYOBJ c, GYcanvas_phase ph, int32 cx, int32 cy, uint8 in)
{
	(void)c;
	g_paint_calls++;
	g_last_phase = ph; g_last_cx = cx; g_last_cy = cy; g_last_in = in;
}

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = fbFlush; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0, 0, 0));
	YMGUI_Inject_SetCtx(ctx);

	//画布视口 (10,10) 200x150,画布像素 64x48。内容盒左上 = (11,11)(去 1px 边框)
	GYOBJ cv = YMGUI_Creat_Canvas_Creat(ctx->root, 10, 10, 200, 150, 64, 48);
	CHECK(cv != NULL, "Creat 返回非空");
	YMGUI_Canvas_SetPaintCb(cv, onPaint);

	//---- 分辨率 ----
	CHECK(YMGUI_Canvas_GetW(cv) == 64, "画布宽 64");
	CHECK(YMGUI_Canvas_GetH(cv) == 48, "画布高 48");
	CHECK(YMGUI_Canvas_GetZoom(cv) == 1, "默认 zoom 1");

	//---- 显示缓冲直写 ----
	GYpx* buf = YMGUI_Canvas_GetBuffer(cv);
	CHECK(buf != NULL, "GetBuffer 非空");
	GYpx mark1 = GY_ColorToPx(GY_ARGB(0xFF, 0x12, 0x34, 0x56));
	GYpx mark2 = GY_ColorToPx(GY_ARGB(0xFF, 0x56, 0x78, 0x9A));
	buf[0] = mark1;
	buf[64 * 48 - 1] = mark2;
	CHECK(GY_PxEqual(buf[0], mark1) && GY_PxEqual(buf[64 * 48 - 1], mark2), "缓冲可写");

	//---- 屏->画布映射 zoom=1 pan=0 ----
	int32 cx, cy;
	//内容盒左上 (11,11) → 画布 (0,0)
	CHECK(YMGUI_Canvas_ScreenToCanvas(cv, 11, 11, &cx, &cy) == 1 && cx == 0 && cy == 0, "屏(11,11)->画布(0,0)");
	CHECK(YMGUI_Canvas_ScreenToCanvas(cv, 21, 16, &cx, &cy) == 1 && cx == 10 && cy == 5, "屏(21,16)->画布(10,5)");
	//画布左上外(负方向)→ 越界,floor 除法给负
	CHECK(YMGUI_Canvas_ScreenToCanvas(cv, 10, 10, &cx, &cy) == 0 && cx == -1 && cy == -1, "屏(10,10)画布外->(-1,-1)");

	//---- zoom=4:每画布像素 4x4 ----
	YMGUI_Canvas_SetZoom(cv, 4);
	CHECK(YMGUI_Canvas_GetZoom(cv) == 4, "zoom=4");
	//屏(11,11)..(14,14) 都映射到画布 (0,0)
	CHECK(YMGUI_Canvas_ScreenToCanvas(cv, 11, 11, &cx, &cy) == 1 && cx == 0 && cy == 0, "zoom4 屏(11,11)->(0,0)");
	CHECK(YMGUI_Canvas_ScreenToCanvas(cv, 14, 14, &cx, &cy) == 1 && cx == 0 && cy == 0, "zoom4 屏(14,14)->(0,0)");
	CHECK(YMGUI_Canvas_ScreenToCanvas(cv, 15, 15, &cx, &cy) == 1 && cx == 1 && cy == 1, "zoom4 屏(15,15)->(1,1)");

	//---- pan:画布右移 8px ----
	YMGUI_Canvas_SetZoom(cv, 1);
	YMGUI_Canvas_SetPan(cv, 8, 0);
	int32 px, py;
	YMGUI_Canvas_GetPan(cv, &px, &py);
	CHECK(px == 8 && py == 0, "SetPan(8,0)");
	//屏(11,11) 现在落画布外(画布起点右移到 19);屏(19,11)->画布(0,0)
	CHECK(YMGUI_Canvas_ScreenToCanvas(cv, 19, 11, &cx, &cy) == 1 && cx == 0 && cy == 0, "pan8 屏(19,11)->(0,0)");
	YMGUI_Canvas_SetPan(cv, 0, 0);

	//---- 绘制回调:按下/移动/抬起 ----
	g_paint_calls = 0;
	YMGUI_Inject_Pointer(21, 16, 1);//画布(10,5) 按下
	CHECK(g_paint_calls == 1 && g_last_phase == GY_CANVAS_DOWN && g_last_cx == 10 && g_last_cy == 5 && g_last_in == 1, "DOWN 回调坐标");
	CHECK(YMGUI_Canvas_IsDrawing(cv, &cx, &cy) == 1 && cx == 10 && cy == 5, "IsDrawing 按下中");
	YMGUI_Inject_Pointer(31, 26, 1);//移动到画布(20,15)
	CHECK(g_last_phase == GY_CANVAS_MOVE && g_last_cx == 20 && g_last_cy == 15, "MOVE 回调坐标");
	YMGUI_Inject_Pointer(31, 26, 0);//抬起
	CHECK(g_last_phase == GY_CANVAS_UP, "UP 回调");
	CHECK(YMGUI_Canvas_IsDrawing(cv, NULL, NULL) == 0, "抬起后不再绘制");

	//---- 画布外按下:in_bounds=0 ----
	g_paint_calls = 0;
	YMGUI_Inject_Pointer(10, 10, 1);//左上边框位置,映射画布外
	CHECK(g_last_phase == GY_CANVAS_DOWN && g_last_in == 0, "画布外按下 in_bounds=0");
	YMGUI_Inject_Pointer(10, 10, 0);

	//---- 平移模式:拖动改 pan,不派绘制 ----
	YMGUI_Canvas_SetPanMode(cv, 1);
	CHECK(YMGUI_Canvas_GetPanMode(cv) == 1, "进入平移模式");
	YMGUI_Canvas_SetPan(cv, 0, 0);
	g_paint_calls = 0;
	YMGUI_Inject_Pointer(50, 50, 1);
	YMGUI_Inject_Pointer(60, 55, 1);//拖动 (+10,+5)
	YMGUI_Canvas_GetPan(cv, &px, &py);
	CHECK(px == 10 && py == 5, "平移模式拖动改 pan");
	CHECK(g_paint_calls == 0, "平移模式不派绘制回调");
	YMGUI_Inject_Pointer(60, 55, 0);
	CHECK(YMGUI_Canvas_IsDrawing(cv, NULL, NULL) == 0, "平移模式 IsDrawing=0");
	YMGUI_Canvas_SetPanMode(cv, 0);

	//---- 渲染不崩(zoom>1 放大 blit + 画布外中性底)----
	YMGUI_Canvas_SetZoom(cv, 3);
	YMGUI_Canvas_SetPan(cv, 4, 4);
	buf[0] = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0, 0));
	YMGUI_Canvas_Invalidate(cv);
	YMGUI_Refresh(ctx);

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_canvas: ALL PASS\n");
	else
		printf("test_canvas: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
