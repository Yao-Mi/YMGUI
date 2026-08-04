#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Image.h"
#include "YMGUI_Button.h"
#include "YMGUI_ArcWidget.h"
#include "YMGUI_Spinner.h"
#include "YMGUI_Meter.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_display.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 显示控件单测:image blit/arc 值渲染/spinner tick 旋转/meter 值指针,渲染出像素
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static long g_set;//本帧 flush 非零像素累计
static GYpx g_match;//要匹配统计的目标像素(0=统计所有非零)
static long g_match_cnt;
static void countFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d;
	long n = (long)a->w * a->h;
	for (long i = 0; i < n; i++)
	{
		if (b[i]) g_set++;
		if (g_match != 0 && b[i] == g_match) g_match_cnt++;
	}
}

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL;
	disp.flush_cb = countFlush;
	disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x00, 0x00, 0x00));

	//---- Image:设图源后渲染出像素 ----
	static GYpx idata[4 * 4];
	for (int i = 0; i < 16; i++) idata[i] = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	GYimg img = {idata, 4, 4, 0, 0};
	GYOBJ imw = YMGUI_Creat_Image_Creat(ctx->root, 10, 10, 40, 40);
	YMGUI_Image_SetSrc(imw, &img);
	g_set = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_set >= 16, "image widget blitted (>=16 px)");

	//---- Image 缩放模式:NONE(默认居中原尺寸,旧行为回归)/FIT(等比留黑边)/FILL(铺满) ----
	{
		//用一张 4x4 纯色图放进 40x40 控件,统计该色像素数区分模式
		GYcolor icol = GY_ARGB(0xFF, 0x20, 0xC0, 0x40);
		static GYpx cdata[4 * 4];
		for (int i = 0; i < 16; i++) cdata[i] = GY_ColorToPx(icol);
		GYimg cimg = {cdata, 4, 4, 0, 0};
		GYOBJ sw = YMGUI_Creat_Image_Creat(ctx->root, 100, 100, 40, 40);
		YMGUI_Image_SetSrc(sw, &cimg);
		g_match = GY_ColorToPx(icol);

		//默认 NONE:4x4 原尺寸居中 → 16 px
		CHECK(YMGUI_Image_GetScaleMode(sw) == GY_IMG_NONE, "image default mode = NONE");
		g_match_cnt = 0; YMGUI_Refresh(ctx);
		CHECK(g_match_cnt == 16, "image NONE: 4x4 stays 16 px (backward compat)");

		//FILL:拉满 40x40 内容盒(减 0 边框,Image 无边框)→ 远多于 16
		YMGUI_Image_SetScaleMode(sw, GY_IMG_FILL);
		CHECK(YMGUI_Image_GetScaleMode(sw) == GY_IMG_FILL, "image mode set to FILL");
		g_match_cnt = 0; YMGUI_Refresh(ctx);
		CHECK(g_match_cnt > 16, "image FILL: stretched well beyond 16 px");
		long fill_cnt = g_match_cnt;

		//FIT:方图放进方框应铺满(4:4 → 40:40 等比即铺满),内容像素数 == FILL
		YMGUI_Image_SetScaleMode(sw, GY_IMG_FIT);
		g_match_cnt = 0; YMGUI_Refresh(ctx);
		CHECK(g_match_cnt > 16, "image FIT: square img scaled up");
		CHECK(g_match_cnt == fill_cnt, "image FIT square-in-square == FILL (no bars)");

		//宽图(8x2)进方框(40x40):FIT 应等比缩放留上下黑边(内容 < FILL 铺满)
		GYcolor wcol = GY_ARGB(0xFF, 0xE0, 0x40, 0x80);
		static GYpx wdata[8 * 2];
		for (int i = 0; i < 16; i++) wdata[i] = GY_ColorToPx(wcol);
		GYimg wimg = {wdata, 8, 2, 0, 0};
		YMGUI_Image_SetSrc(sw, &wimg);
		g_match = GY_ColorToPx(wcol);

		YMGUI_Image_SetScaleMode(sw, GY_IMG_FILL);
		g_match_cnt = 0; YMGUI_Refresh(ctx);
		long wfill = g_match_cnt;

		YMGUI_Image_SetScaleMode(sw, GY_IMG_FIT);
		g_match_cnt = 0; YMGUI_Refresh(ctx);
		CHECK(g_match_cnt > 0 && g_match_cnt < wfill, "image FIT wide-in-square leaves bars (< FILL)");

		g_match = 0;//复位,后续 arc 统计不受干扰
	}

	//---- Arc:前景色像素数应随值增大(用独立前景色计数,避免被同位置背景干扰) ----
	GYcolor arc_fg = GY_ARGB(0xFF, 0x30, 0x90, 0xE0);
	GYOBJ arc = YMGUI_Creat_Arc_Creat(ctx->root, 60, 60, 80, 80);
	YMGUI_Arc_SetRange(arc, 0, 100);
	YMGUI_Arc_SetColors(arc, GY_ARGB(0xFF, 0x40, 0x40, 0x48), arc_fg);
	g_match = GY_ColorToPx(arc_fg);

	YMGUI_Arc_SetValue(arc, 0);
	g_match_cnt = 0; YMGUI_Refresh(ctx);
	long arc_lo = g_match_cnt;
	YMGUI_Arc_SetValue(arc, 100);
	CHECK(YMGUI_Arc_GetValue(arc) == 100, "arc value set");
	g_match_cnt = 0; YMGUI_Refresh(ctx);
	long arc_hi = g_match_cnt;
	CHECK(arc_hi > arc_lo, "arc value=100 draws more fg pixels than value=0");
	g_match = 0;//关闭匹配统计
	//超范围钳制
	YMGUI_Arc_SetValue(arc, 500);
	CHECK(YMGUI_Arc_GetValue(arc) == 100, "arc value clamped");

	//---- Spinner:tick 改变角度(渲染仍有像素) ----
	GYOBJ sp = YMGUI_Creat_Spinner_Creat(ctx->root, 160, 60, 60, 60);
	g_set = 0; YMGUI_Refresh(ctx);
	CHECK(g_set > 0, "spinner renders arc pixels");
	//tick 多次不崩,仍渲染
	for (int i = 0; i < 20; i++) YMGUI_Spinner_Tick(sp);
	g_set = 0; YMGUI_Refresh(ctx);
	CHECK(g_set > 0, "spinner after ticks still renders");

	//---- Meter:值变指针角度变(渲染有像素) ----
	GYOBJ m = YMGUI_Creat_Meter_Creat(ctx->root, 220, 140, 90, 90);
	YMGUI_Meter_SetRange(m, 0, 100);
	YMGUI_Meter_SetValue(m, 50);
	CHECK(YMGUI_Meter_GetValue(m) == 50, "meter value set");
	g_set = 0; YMGUI_Refresh(ctx);
	CHECK(g_set > 0, "meter renders (arc+ticks+needle)");
	YMGUI_Meter_SetValue(m, 200);//钳制
	CHECK(YMGUI_Meter_GetValue(m) == 100, "meter value clamped");
	//刻度值标签:开启后渲染像素应比关闭时多(多画了文字);默认关是向后兼容
	YMGUI_Meter_SetTicks(m, 6);
	g_set = 0; YMGUI_Refresh(ctx);
	int meter_no_labels = g_set;
	YMGUI_Meter_SetShowLabels(m, 1);
	g_set = 0; YMGUI_Refresh(ctx);
	CHECK(g_set > meter_no_labels, "meter tick labels add pixels when enabled");

	//---- Button 贴图:图优先 / 底色开关 / 清图回退文字 ----
	{
		//8x8 纯红图块(独立色便于计数,不与其它控件重叠区)
		static GYpx bimg[8 * 8];
		GYcolor red = GY_ARGB(0xFF, 0xE0, 0x20, 0x20);
		GYpx redpx = GY_ColorToPx(red);
		for (int i = 0; i < 64; i++) bimg[i] = redpx;
		GYimg img = {bimg, 8, 8, 0, 0};
		GYOBJ btn = YMGUI_Creat_Button_Creat(ctx->root, 10, 180, 40, 40);

		//设图后应 blit 出红色像素(图居中,底色仍在但红色只来自图)
		YMGUI_Button_SetImage(btn, &img);
		g_match = redpx; g_match_cnt = 0; g_set = 0;
		YMGUI_Refresh(ctx);
		CHECK(g_match_cnt >= 64, "button image blitted (>=64 red px)");

		//关底色后图仍在(纯图标按钮:底色边框不画,图照 blit)
		YMGUI_Button_SetBgVisible(btn, 0);
		g_match = redpx; g_match_cnt = 0; g_set = 0;
		YMGUI_Refresh(ctx);
		CHECK(g_match_cnt >= 64, "button image still blitted with bg off");

		//清图回退文字:SetImage(NULL) 后设文字应渲染出非红像素
		YMGUI_Button_SetImage(btn, NULL);
		YMGUI_Button_SetBgVisible(btn, 1);
		YMGUI_Button_SetText(btn, "OK");
		g_match = redpx; g_match_cnt = 0; g_set = 0;
		YMGUI_Refresh(ctx);
		CHECK(g_set > 0, "button renders after image cleared");
		CHECK(g_match_cnt == 0, "no red px after image cleared (fell back to text)");
		g_match = 0;
	}

	//---- 级联释放 ----
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_display: ALL PASS\n");
	else
		printf("test_display: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
