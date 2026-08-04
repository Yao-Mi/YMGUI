#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_ColorPicker.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_colorpicker.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 取色器单测:HSV<->RGB 整数换算(纯红/绿/蓝/白/黑/灰)、SetColor/GetColor 往返、
  *	              SetHSV/GetHSV、点击 SV 方块/色相条更新颜色 + 触发回调、渲染不崩、析构不崩。
  *	              判成败以 exit code 为准。
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

static int g_cb_calls = 0;
static GYcolor g_cb_color = 0;
static void onChanged(GYOBJ p, GYcolor c) { (void)p; g_cb_calls++; g_cb_color = c; }

//近似比较(整数 HSV 往返有量化误差)
static int near(int a, int b, int tol) { int d = a - b; if (d < 0) d = -d; return d <= tol; }

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = fbFlush; disp.user_data = NULL;

	//---- 纯函数 HSV->RGB(实例无关)----
	GYcolor red = YMGUI_ColorPicker_HSVtoRGB(0, 255, 255);
	CHECK(near(GY_COLOR_R(red), 255, 2) && GY_COLOR_G(red) < 4 && GY_COLOR_B(red) < 4, "H0 S255 V255 = 纯红");
	GYcolor grn = YMGUI_ColorPicker_HSVtoRGB(120, 255, 255);
	CHECK(GY_COLOR_R(grn) < 4 && near(GY_COLOR_G(grn), 255, 2) && GY_COLOR_B(grn) < 4, "H120 = 纯绿");
	GYcolor blu = YMGUI_ColorPicker_HSVtoRGB(240, 255, 255);
	CHECK(GY_COLOR_R(blu) < 4 && GY_COLOR_G(blu) < 4 && near(GY_COLOR_B(blu), 255, 2), "H240 = 纯蓝");
	GYcolor wht = YMGUI_ColorPicker_HSVtoRGB(0, 0, 255);
	CHECK(near(GY_COLOR_R(wht), 255, 2) && near(GY_COLOR_G(wht), 255, 2) && near(GY_COLOR_B(wht), 255, 2), "S0 V255 = 白");
	GYcolor blk = YMGUI_ColorPicker_HSVtoRGB(0, 0, 0);
	CHECK(GY_COLOR_R(blk) == 0 && GY_COLOR_G(blk) == 0 && GY_COLOR_B(blk) == 0, "V0 = 黑");
	GYcolor gry = YMGUI_ColorPicker_HSVtoRGB(0, 0, 128);
	CHECK(near(GY_COLOR_R(gry), 128, 2) && near(GY_COLOR_G(gry), 128, 2) && near(GY_COLOR_B(gry), 128, 2), "S0 V128 = 中灰");

	//---- RGB->HSV 往返 ----
	uint16 h; uint8 s, v;
	YMGUI_ColorPicker_RGBtoHSV(GY_ARGB(0xFF, 255, 0, 0), &h, &s, &v);
	CHECK(h == 0 && s == 255 && v == 255, "纯红->HSV(0,255,255)");
	YMGUI_ColorPicker_RGBtoHSV(GY_ARGB(0xFF, 0, 255, 0), &h, &s, &v);
	CHECK(near(h, 120, 1) && s == 255 && v == 255, "纯绿->H120");
	YMGUI_ColorPicker_RGBtoHSV(GY_ARGB(0xFF, 0, 0, 255), &h, &s, &v);
	CHECK(near(h, 240, 1) && s == 255 && v == 255, "纯蓝->H240");
	YMGUI_ColorPicker_RGBtoHSV(GY_ARGB(0xFF, 128, 128, 128), &h, &s, &v);
	CHECK(s == 0 && near(v, 128, 1), "灰->S0");

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0, 0, 0));
	YMGUI_Inject_SetCtx(ctx);

	//取色器 (10,10) 200x180 → SV 方块 176 宽,色相条 18 宽(GAP 6)
	GYOBJ cp = YMGUI_Creat_ColorPicker_Creat(ctx->root, 10, 10, 200, 180);
	CHECK(cp != NULL, "Creat 返回非空");
	YMGUI_ColorPicker_SetChangedCb(cp, onChanged);

	//---- SetColor/GetColor 往返 ----
	YMGUI_ColorPicker_SetColor(cp, GY_ARGB(0xFF, 200, 60, 30));
	GYcolor got = YMGUI_ColorPicker_GetColor(cp);
	CHECK(near(GY_COLOR_R(got), 200, 6) && near(GY_COLOR_G(got), 60, 6) && near(GY_COLOR_B(got), 30, 6), "SetColor/GetColor 往返近似");

	//---- SetHSV/GetHSV ----
	YMGUI_ColorPicker_SetHSV(cp, 400, 200, 100);//400%360=40
	YMGUI_ColorPicker_GetHSV(cp, &h, &s, &v);
	CHECK(h == 40 && s == 200 && v == 100, "SetHSV 取模+存取");

	//---- SetColor 不触发回调 ----
	CHECK(g_cb_calls == 0, "SetColor/SetHSV 不触发回调");

	//---- 点击 SV 方块左上角 → 高 V 低 S(接近纯色相白端);触发回调 ----
	//SV 方块屏幕: x 11..186, y 11..189。左上(11,11)=S0 V255=白
	YMGUI_ColorPicker_SetHSV(cp, 0, 255, 255);
	g_cb_calls = 0;
	YMGUI_Inject_Pointer(12, 12, 1);
	YMGUI_ColorPicker_GetHSV(cp, &h, &s, &v);
	CHECK(s < 20 && v > 235, "点 SV 左上 → 低S高V");
	CHECK(g_cb_calls >= 1, "点 SV 触发回调");
	YMGUI_Inject_Pointer(12, 12, 0);

	//---- 拖到 SV 右下角 → 高 S 低 V ----
	YMGUI_Inject_Pointer(184, 187, 1);
	YMGUI_ColorPicker_GetHSV(cp, &h, &s, &v);
	CHECK(s > 235 && v < 20, "点 SV 右下 → 高S低V");
	YMGUI_Inject_Pointer(184, 187, 0);

	//---- 点色相条底部 → 高 H ----
	//色相条屏幕 x: 10+176+6=192 .. 210,y 11..189。底部 y=188 → H≈359
	YMGUI_Inject_Pointer(200, 188, 1);
	YMGUI_ColorPicker_GetHSV(cp, &h, &s, &v);
	CHECK(h > 340, "点色相条底部 → 高H");
	YMGUI_Inject_Pointer(200, 188, 0);
	//点色相条顶部 → 低 H
	YMGUI_Inject_Pointer(200, 12, 1);
	YMGUI_ColorPicker_GetHSV(cp, &h, &s, &v);
	CHECK(h < 20, "点色相条顶部 → 低H");
	YMGUI_Inject_Pointer(200, 12, 0);

	//---- 渲染不崩 ----
	YMGUI_Refresh(ctx);

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_colorpicker: ALL PASS\n");
	else
		printf("test_colorpicker: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
