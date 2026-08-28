#include "YMGUI_PubDefine.h"
#include "YMGUI_PubType.h"
#include "YMGUI_Surface.h"
#include "YMGUI_DrawPx.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_DrawArc.h"
#include "YMGUI_Font.h"
#include "YMGUI_Trig.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_aa.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 抗锯齿单测:混合基石 GY_MixPx/GY_BlendPx、Wu 斜线、圆/弧 coverage、4bpp 字体。
  *	              核心判据——AA 图元边缘应出现"既非背景也非满前景"的中间灰度像素(证明混合生效);
  *	              端点/实心核仍是精确色(证明满覆盖保持锐利)。判定以退出码(fails)为准。
  ***************************************************************************************************************************/

#define W 64
#define H 64

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static GYpx g_fb[W * H];
static GYsurface g_s;

static void clearFb(void)
{
	for (int i = 0; i < W * H; i++) g_fb[i] = GY_PX_ZERO;//背景=0(黑)
}
static void initSurf(void)
{
	g_s.buf = g_fb;
	g_s.buf_area = (GYrect){0, 0, W, H};
	g_s.clip = g_s.buf_area;
	g_s.stride = W;
}
static GYpx at(GYcoord x, GYcoord y) { return g_fb[y * W + x]; }

//统计一块区域内"中间像素"(既非 bg 也非 fg)与精确 fg 像素
static void tally(GYpx bg, GYpx fg, int* mid, int* exact)
{
	*mid = 0; *exact = 0;
	for (int i = 0; i < W * H; i++)
	{
		if (GY_PxEqual(g_fb[i], fg)) (*exact)++;
		else if (!GY_PxEqual(g_fb[i], bg)) (*mid)++;
	}
}

int main(void)
{
	initSurf();
	GYcolor white = GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
	GYpx    whitepx = GY_ColorToPx(white);
	GYpx    bg = GY_PX_ZERO;

	//---- A. 混合基石 ----
	//GY_MixPx:半覆盖度应出中间值(既非 bg 也非 fg)
	GYpx half = GY_MixPx(bg, white, 128);
	CHECK(!GY_PxEqual(half, bg) && !GY_PxEqual(half, whitepx), "GY_MixPx half coverage -> intermediate");
	CHECK(GY_PxEqual(GY_MixPx(bg, white, 0), bg), "GY_MixPx opa=0 keeps dst");
	CHECK(GY_PxEqual(GY_MixPx(bg, white, 255), whitepx), "GY_MixPx opa=255 -> full src");
	//GY_BlendPx:裁剪外不写
	clearFb();
	g_s.clip = (GYrect){0, 0, 10, 10};
	GY_BlendPx(&g_s, 50, 50, white, 200);//裁剪区外
	CHECK(GY_PxEqual(at(50, 50), bg), "GY_BlendPx outside clip does not write");
	g_s.clip = g_s.buf_area;
	GY_BlendPx(&g_s, 5, 5, white, 128);
	CHECK(!GY_PxEqual(at(5, 5), bg) && !GY_PxEqual(at(5, 5), whitepx), "GY_BlendPx writes blended pixel");

#if YMGUI_ANTIALIAS
	//---- B1. Wu 斜线:边缘应有中间灰度;端点精确色 ----
	clearFb();
	YMGUI_Draw_Line(&g_s, 4, 4, 40, 20, white);//缓斜线
	int mid, exact;
	tally(bg, whitepx, &mid, &exact);
	CHECK(mid > 0, "Wu diagonal line has intermediate (AA) pixels");
	CHECK(exact > 0, "Wu diagonal line has exact-color pixels (sharp endpoints)");

	//水平线仍是纯前景色(走快路径,无中间灰度)
	clearFb();
	YMGUI_Draw_Line(&g_s, 4, 30, 40, 30, white);
	tally(bg, whitepx, &mid, &exact);
	CHECK(exact > 0 && mid == 0, "horizontal line is solid (no AA needed)");

	//---- B2. 圆:边界有中间灰度 ----
	clearFb();
	YMGUI_Draw_Circle(&g_s, 32, 32, 20, white);
	tally(bg, whitepx, &mid, &exact);
	CHECK(mid > 0, "circle outline has AA pixels");

	//---- B3. 实心圆:内核精确色 + 边界中间灰度 ----
	clearFb();
	YMGUI_Draw_CircleFill(&g_s, 32, 32, 18, white);
	tally(bg, whitepx, &mid, &exact);
	CHECK(exact > 0, "filled circle has solid core");
	CHECK(mid > 0, "filled circle has AA boundary");
	CHECK(GY_PxEqual(at(32, 32), whitepx), "filled circle center is solid fg");

	//---- B4. 弧:有中间灰度 ----
	clearFb();
	YMGUI_Draw_Arc(&g_s, 32, 32, 20, 0, 90, white);
	tally(bg, whitepx, &mid, &exact);
	CHECK(mid > 0, "arc has AA pixels");
#endif

	//---- C. 4bpp 字体:'A' 边缘出现中间灰度;空格全零;推进=cell_w ----
	GYFONT f = &YMGUI_Font_Default;
#if YMGUI_ANTIALIAS
	CHECK(f->bpp == 4, "default font is 4bpp (AA build)");
#endif
	clearFb();
	GYcoord adv = YMGUI_Draw_Char(&g_s, f, 4, 4, 'A', white);
	CHECK(adv == f->cell_w, "char advance = cell_w");
	int mid2, exact2;
	tally(bg, whitepx, &mid2, &exact2);
	CHECK(exact2 + mid2 > 0, "'A' produced pixels");
#if YMGUI_ANTIALIAS
	CHECK(mid2 > 0, "4bpp 'A' has grayscale (AA) edge pixels");
#endif
	clearFb();
	YMGUI_Draw_Char(&g_s, f, 4, 4, ' ', white);
	int mid3, exact3;
	tally(bg, whitepx, &mid3, &exact3);
	CHECK(mid3 == 0 && exact3 == 0, "space glyph is empty");

	if (fails == 0)
		printf("test_aa: ALL PASS\n");
	else
		printf("test_aa: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
