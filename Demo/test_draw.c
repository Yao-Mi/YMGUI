#include "YMGUI_PubDefine.h"
#include "YMGUI_Surface.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_DrawImg.h"
#include "YMGUI_DrawArc.h"
#include "YMGUI_Trig.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_draw.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 图元单测:trig 表、line(水平/垂直/斜)、img blit(含colorkey)、circle、arc 的像素落地+裁剪
  ***************************************************************************************************************************/

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

#define W 64
#define H 64
static GYpx buf[W * H];
static GYsurface s;

static void clearBuf(void)
{
	for (int i = 0; i < W * H; i++) buf[i] = 0;
}
static void setupFull(void)
{
	s.buf = buf; s.stride = W;
	s.buf_area = (GYrect){0, 0, W, H};
	s.clip = s.buf_area;
	clearBuf();
}
static int countSet(void)
{
	int c = 0;
	for (int i = 0; i < W * H; i++) if (buf[i]) c++;
	return c;
}
static int px(int x, int y) { return buf[y * W + x] != 0; }

int main(void)
{
	GYcolor white = GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF);

	//---- 定点三角表 ----
	CHECK(GY_Sin(0) == 0, "sin(0)=0");
	CHECK(GY_Cos(0) == GY_SinTab[90], "cos(0)=sin(90)");
	CHECK(GY_Sin(90) > 32000, "sin(90)~=1 (Q15)");
	CHECK(GY_Sin(270) < -32000, "sin(270)~=-1");
	CHECK(GY_Sin(360) == GY_Sin(0), "sin wraps 360");
	CHECK(GY_Sin(-90) == GY_Sin(270), "sin negative wraps");

	//---- 水平线 ----
	setupFull();
	YMGUI_Draw_Line(&s, 10, 20, 40, 20, white);
	CHECK(px(10, 20) && px(25, 20) && px(40, 20), "hline endpoints+mid set");
	CHECK(!px(9, 20) && !px(41, 20), "hline no overshoot");
	CHECK(countSet() == 31, "hline exact pixel count (10..40)");

	//---- 垂直线 ----
	setupFull();
	YMGUI_Draw_Line(&s, 30, 5, 30, 25, white);
	CHECK(px(30, 5) && px(30, 25), "vline endpoints set");
	CHECK(countSet() == 21, "vline exact count (5..25)");

	//---- 斜线(对角)----
	setupFull();
	YMGUI_Draw_Line(&s, 0, 0, 20, 20, white);
	CHECK(px(0, 0) && px(10, 10) && px(20, 20), "diagonal passes through (10,10)");

	//---- 线裁剪:超出 clip 不写 ----
	setupFull();
	s.clip = (GYrect){0, 0, 16, 64};//只留左 16 列
	YMGUI_Draw_Line(&s, 0, 30, 40, 30, white);
	CHECK(px(0, 30) && px(15, 30), "clipped line left part drawn");
	CHECK(!px(16, 30) && !px(40, 30), "clipped line right part suppressed");

	//---- 图片 blit ----
	setupFull();
	GYpx idata[4] = {white, white, white, white};//2x2 全白
	GYimg img = {idata, 2, 2, 0, 0};
	YMGUI_Draw_Img(&s, &img, 5, 5);
	CHECK(px(5, 5) && px(6, 6) && px(6, 5) && px(5, 6), "2x2 img blitted");
	CHECK(countSet() == 4, "img exactly 4 px");

	//---- colorkey 透明 ----
	setupFull();
	GYpx kd[4] = {white, 0, 0, white};//对角白,其余=key(0)
	GYimg kimg = {kd, 2, 2, 1, 0};//use_key, key=0
	YMGUI_Draw_Img(&s, &kimg, 10, 10);
	CHECK(px(10, 10) && px(11, 11), "colorkey: opaque corners drawn");
	CHECK(!px(11, 10) && !px(10, 11), "colorkey: key pixels skipped");

	//---- 缩放 blit:放大 ----
	//源 2x2:每格不同值(左上1 右上2 左下3 右下4),放大 4x → 8x8 块,每源像素占 4x4
	setupFull();
	{
		GYpx sd[4] = {1, 2, 3, 4};
		GYimg simg = {sd, 2, 2, 0, 0};
		GYrect dst = {10, 10, 8, 8};//2x2 -> 8x8,放大 4 倍
		YMGUI_Draw_ImgScaled(&s, &simg, dst);
		CHECK(countSet() == 64, "scale up 2x2->8x8 fills 64 px");
		//左上区(10..13,10..13)= 源[0][0]=1
		CHECK(buf[10 * W + 10] == 1 && buf[13 * W + 13] == 1, "scale up: TL quadrant = src 1");
		//右上区(14..17,10..13)= 源[0][1]=2
		CHECK(buf[10 * W + 14] == 2 && buf[13 * W + 17] == 2, "scale up: TR quadrant = src 2");
		//左下区(10..13,14..17)= 源[1][0]=3
		CHECK(buf[14 * W + 10] == 3 && buf[17 * W + 13] == 3, "scale up: BL quadrant = src 3");
		//右下区(14..17,14..17)= 源[1][1]=4
		CHECK(buf[14 * W + 14] == 4 && buf[17 * W + 17] == 4, "scale up: BR quadrant = src 4");
	}

	//---- 缩放 blit:缩小 ----
	//源 8x8 全白缩到 4x4:应恰好 16 px 且都是白
	setupFull();
	{
		GYpx big[64]; for (int i = 0; i < 64; i++) big[i] = white;
		GYimg bimg = {big, 8, 8, 0, 0};
		GYrect dst = {20, 20, 4, 4};
		YMGUI_Draw_ImgScaled(&s, &bimg, dst);
		CHECK(countSet() == 16, "scale down 8x8->4x4 = 16 px");
		CHECK(px(20, 20) && px(23, 23), "scale down: corners set");
		CHECK(!px(24, 24), "scale down: no overshoot past dst");
	}

	//---- 缩放 blit:裁剪 ----
	//放大目标超出 clip,只画可见部分
	setupFull();
	s.clip = (GYrect){0, 0, 16, 64};//只留左 16 列
	{
		GYpx sd[4] = {white, white, white, white};
		GYimg simg = {sd, 2, 2, 0, 0};
		GYrect dst = {10, 10, 20, 10};//右侧超出 clip(x>=16 被裁)
		YMGUI_Draw_ImgScaled(&s, &simg, dst);
		//x 10..15 可见(6 列)× y 10..19(10 行)= 60 px,右边 16.. 被裁
		CHECK(countSet() == 60, "scale + clip: only visible cols drawn");
		CHECK(px(15, 15) && !px(16, 15), "scale + clip: clipped at x=16");
	}

	//---- 缩放 blit:colorkey ----
	setupFull();
	{
		GYpx kd[4] = {white, 0, 0, white};//对角白,其余 key(0)
		GYimg kimg2 = {kd, 2, 2, 1, 0};
		GYrect dst = {30, 30, 8, 8};//放大 4x
		YMGUI_Draw_ImgScaled(&s, &kimg2, dst);
		//TL/BR 象限白(各 16 px)= 32,TR/BL 象限 key 跳过
		CHECK(countSet() == 32, "scale + colorkey: key quadrants skipped");
		CHECK(px(30, 30) && px(37, 37), "scale + colorkey: white quadrants drawn");
		CHECK(!px(34, 30) && !px(30, 34), "scale + colorkey: key quadrants blank");
	}

	//---- 缩放 blit:退化保护(空 dst 不崩不画)----
	setupFull();
	{
		GYpx sd[4] = {white, white, white, white};
		GYimg simg = {sd, 2, 2, 0, 0};
		GYrect z = {5, 5, 0, 8};//w=0
		YMGUI_Draw_ImgScaled(&s, &simg, z);
		CHECK(countSet() == 0, "scale: zero-width dst draws nothing");
	}

	//---- 画圆环 ----
	setupFull();
	YMGUI_Draw_Circle(&s, 32, 32, 10, white);
	CHECK(px(32 + 10, 32) || px(32 + 9, 32), "circle right point ~ (42,32)");
	CHECK(px(32, 32 + 10) || px(32, 32 + 9), "circle bottom point");
	CHECK(!px(32, 32), "circle center empty (outline only)");
	int ring = countSet();
	CHECK(ring > 20 && ring < 120, "circle outline pixel count sane");

	//---- 实心圆盘 ----
	setupFull();
	YMGUI_Draw_CircleFill(&s, 32, 32, 8, white);
	CHECK(px(32, 32), "disk center filled");
	CHECK(countSet() > ring, "disk has more px than outline");

	//---- 画弧:0..90 度(右→下),右点和下点应有,左/上不应有 ----
	setupFull();
	YMGUI_Draw_Arc(&s, 32, 32, 10, 0, 90, white);
	CHECK(px(32 + 10, 32) || px(32 + 9, 32) || px(32 + 10, 33), "arc has right point (0deg)");
	CHECK(px(32, 32 + 10) || px(32, 32 + 9) || px(33, 32 + 10), "arc has bottom point (90deg)");
	CHECK(!px(32 - 10, 32) && !px(32 - 9, 32), "arc 0..90 excludes left (180deg)");
	CHECK(!px(32, 32 - 10) && !px(32, 32 - 9), "arc 0..90 excludes top (270deg)");

	if (fails == 0)
		printf("test_draw: ALL PASS\n");
	else
		printf("test_draw: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
