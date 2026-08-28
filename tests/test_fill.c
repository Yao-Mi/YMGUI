#include "YMGUI_PubDefine.h"
#include "YMGUI_Surface.h"
#include "YMGUI_DrawFill.h"
#include <stdio.h>
#include <stdlib.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_fill.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 无 SDL 的像素正确性单测:验证 fill 直写、裁剪、band 偏移、alpha 混合
  ***************************************************************************************************************************/

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

int main(void)
{
	//8x8 屏,分两条 band(每条 4 行),draw buffer 只有一条 band 大小
	GYpx buf[8 * 4];
	GYsurface s;
	GYrect full = {0, 0, 8, 8};

	//---- band0: 屏幕 y=0..3 ----
	for (int i = 0; i < 8 * 4; i++) buf[i] = GY_PX_ZERO;
	s.buf = buf; s.stride = 8;
	s.buf_area = (GYrect){0, 0, 8, 4};
	s.clip = s.buf_area;
	//全屏铺红(不透明);band0 只应写到本条 band
	YMGUI_Draw_Fill(&s, &full, GY_ARGB(0xFF, 0xFF, 0x00, 0x00), GY_OPA_COVER);
	GYpx redpx = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0x00, 0x00));
	CHECK(GY_PxEqual(buf[0], redpx), "band0 (0,0) should be red");
	CHECK(GY_PxEqual(buf[8 * 3 + 7], redpx), "band0 (7,3) last row should be red");

	//---- band1: 屏幕 y=4..7,验证 band 偏移(屏幕坐标→buffer 偏移) ----
	for (int i = 0; i < 8 * 4; i++) buf[i] = GY_PX_ZERO;
	s.buf_area = (GYrect){0, 4, 8, 4};
	s.clip = s.buf_area;
	//只在屏幕 (2,5) 起 2x2 填绿
	GYrect g = {2, 5, 2, 2};
	YMGUI_Draw_Fill(&s, &g, GY_ARGB(0xFF, 0x00, 0xFF, 0x00), GY_OPA_COVER);
	GYpx greenpx = GY_ColorToPx(GY_ARGB(0xFF, 0x00, 0xFF, 0x00));
	//屏幕(2,5) → buffer 行 (5-4)=1, 列 2 → 索引 1*8+2=10
	CHECK(GY_PxEqual(buf[1 * 8 + 2], greenpx), "band1 screen(2,5) maps to buf[10]");
	CHECK(GY_PxIsZero(buf[0]), "band1 (0,0) untouched (outside rect)");
	CHECK(GY_PxIsZero(buf[3 * 8 + 0]), "band1 row3 untouched");

	//---- 裁剪:填充区超出 clip,只落在 clip 内 ----
	for (int i = 0; i < 8 * 4; i++) buf[i] = GY_PX_ZERO;
	s.buf_area = (GYrect){0, 0, 8, 4};
	s.clip = (GYrect){0, 0, 4, 4};//裁剪到左半
	YMGUI_Draw_Fill(&s, &full, GY_ARGB(0xFF, 0xFF, 0x00, 0x00), GY_OPA_COVER);
	CHECK(GY_PxEqual(buf[0], redpx), "clip: (0,0) inside clip written");
	CHECK(GY_PxIsZero(buf[4]), "clip: (4,0) outside clip NOT written");

	//---- alpha 混合:白底上叠 50% 黑,应得灰 ----
	for (int i = 0; i < 8 * 4; i++) buf[i] = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));//白底
	s.buf_area = (GYrect){0, 0, 8, 4};
	s.clip = s.buf_area;
	YMGUI_Draw_Fill(&s, &full, GY_ARGB(0xFF, 0x00, 0x00, 0x00), 128);//50% 黑
	//结果应比白暗、比黑亮(取 R 分量粗判)
	GYcolor mixed = GY_PxToColor(buf[0]);
	CHECK(GY_COLOR_R(mixed) > 100 && GY_COLOR_R(mixed) < 160, "blend: white+50%black ~ mid gray");

#if YMGUI_COLOR_DEPTH == 24
	GYpx order = GY_ColorToPx(GY_ARGB(0xFF, 0x12, 0x34, 0x56));
	const uint8* raw = (const uint8*)&order;
	CHECK(sizeof(GYpx) == 3, "RGB888 pixel occupies exactly 3 bytes");
	CHECK(raw[0] == 0x12 && raw[1] == 0x34 && raw[2] == 0x56,
	      "RGB888 framebuffer byte order is R,G,B");
#endif

	if (fails == 0)
		printf("test_fill: ALL PASS\n");
	else
		printf("test_fill: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
