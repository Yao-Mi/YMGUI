#include "YMGUI_PubDefine.h"
#include "YMGUI_Surface.h"
#include "YMGUI_DrawArc.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_arc.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 粗弧无缝隙单测:整环 ArcThick 后,严格落在环带内的每个像素都必须被填充(无黑洞)
  ***************************************************************************************************************************/

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

#define N 100
static GYpx buf[N * N];
static GYsurface s;

int main(void)
{
	GYcolor white = GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
	for (int i = 0; i < N * N; i++) buf[i] = 0;
	s.buf = buf; s.stride = N;
	s.buf_area = (GYrect){0, 0, N, N};
	s.clip = s.buf_area;

	GYcoord cx = 50, cy = 50, r_in = 25, r_out = 35;
	//整环(0..359)粗弧
	YMGUI_Draw_ArcThick(&s, cx, cy, r_in, r_out, 0, 359, white);

	//检查:严格落在环带 [r_in+1, r_out-1] 内的像素必须全部被填充(否则有洞)
	int holes = 0, checked = 0;
	int32 lo2 = (int32)(r_in + 1) * (r_in + 1);
	int32 hi2 = (int32)(r_out - 1) * (r_out - 1);
	for (int y = 0; y < N; y++)
	{
		for (int x = 0; x < N; x++)
		{
			int32 dx = x - cx, dy = y - cy;
			int32 d2 = dx * dx + dy * dy;
			if (d2 >= lo2 && d2 <= hi2)
			{
				checked++;
				if (buf[y * N + x] == 0)
					holes++;
			}
		}
	}
	CHECK(checked > 100, "annulus band has enough pixels to test");
	CHECK(holes == 0, "no holes strictly inside thick ring band");
	if (holes)
		printf("  (holes=%d of checked=%d)\n", holes, checked);

	//半弧(0..180)也不应有洞:检查下半环带
	for (int i = 0; i < N * N; i++) buf[i] = 0;
	YMGUI_Draw_ArcThick(&s, cx, cy, r_in, r_out, 0, 180, white);
	int holes2 = 0;
	for (int y = cy + 2; y < N; y++)//严格下半(y>cy),避开端点
	{
		for (int x = 0; x < N; x++)
		{
			int32 dx = x - cx, dy = y - cy;
			int32 d2 = dx * dx + dy * dy;
			if (d2 >= lo2 && d2 <= hi2)
				if (buf[y * N + x] == 0)
					holes2++;
		}
	}
	CHECK(holes2 == 0, "no holes in lower half-ring band");
	if (holes2)
		printf("  (half-ring holes=%d)\n", holes2);

	if (fails == 0)
		printf("test_arc: ALL PASS\n");
	else
		printf("test_arc: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
