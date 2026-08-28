#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_TextView.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_textview.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 文本视图单测:空文本占位、'\n' 断行、Multiline 开关、Wrap 折行(两态)、
  *	              长行折行行数、滚动钳制、宽度变化重排行表、拖动滚动、文字实际绘制(像素非背景)。
  ***************************************************************************************************************************/

#define SCR_W 240
#define SCR_H 160
#define BAND_H 160

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
static GYpx px_at(GYcoord x, GYcoord y) { return g_fb[y * SCR_W + x]; }

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = fbFlush; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0, 0, 0));

	//文本视图 (10,10) 100x40;默认字体 cell_h=16 → line_h=20 → 2 行可见,内容宽=100-8=92(ASCII 8px/字 → 11 字/行)
	GYOBJ tv = YMGUI_Creat_TextView_Creat(ctx->root, 10, 10, 100, 40);

	//---- 空文本:一行占位,GetText 空串 ----
	CHECK(YMGUI_TextView_GetLineCount(tv) == 1, "empty text → 1 placeholder line");
	CHECK(strcmp(YMGUI_TextView_GetText(tv), "") == 0, "empty GetText → \"\"");

	//---- '\n' 断行(Multiline 默认开,Wrap 关)----
	YMGUI_TextView_SetText(tv, "line1\nline2\nline3");
	CHECK(strcmp(YMGUI_TextView_GetText(tv), "line1\nline2\nline3") == 0, "GetText round-trips");
	CHECK(YMGUI_TextView_GetLineCount(tv) == 3, "3 newline-split lines");

	//---- Multiline 关:忽略 '\n',整段一逻辑行(Wrap 仍关 → 1 行)----
	YMGUI_TextView_SetMultiline(tv, 0);
	CHECK(YMGUI_TextView_GetLineCount(tv) == 1, "multiline off → single logical line");

	//---- Wrap 开(Multiline 仍关):17 字符含 '\n' 当普通字 → 11 字/行 → 2 行 ----
	YMGUI_TextView_SetWrap(tv, 1);
	CHECK(YMGUI_TextView_GetLineCount(tv) == 2, "wrap on, single logical 17-char line → 2 wrapped lines");

	//---- Multiline 开 + Wrap 开:3 段各 5 字符,均 < 内容宽 → 各 1 行 → 3 行 ----
	YMGUI_TextView_SetMultiline(tv, 1);
	CHECK(YMGUI_TextView_GetLineCount(tv) == 3, "multiline+wrap: 3 short segments → 3 lines");

	//---- 单长行折行(无换行,30 字符):30/11 → 3 行 ----
	YMGUI_TextView_SetText(tv, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123");//30 chars
	CHECK(YMGUI_TextView_GetLineCount(tv) == 3, "30-char line wraps to 3 lines at width 92");

	//---- Wrap 关:同一长行不折,1 行(右侧裁掉)----
	YMGUI_TextView_SetWrap(tv, 0);
	CHECK(YMGUI_TextView_GetLineCount(tv) == 1, "wrap off → long line stays 1 line (clipped)");

	//---- 滚动钳制:3 行(先切回 wrap 开得 3 行)line_h=20 → 内容高=60,视口=40,maxs=20 ----
	YMGUI_TextView_SetWrap(tv, 1);
	CHECK(YMGUI_TextView_GetLineCount(tv) == 3, "back to wrap → 3 lines");
	YMGUI_TextView_SetScroll(tv, -10);
	CHECK(YMGUI_TextView_GetScroll(tv) == 0, "scroll clamps at 0");
	YMGUI_TextView_SetScroll(tv, 9999);
	CHECK(YMGUI_TextView_GetScroll(tv) == 20, "scroll clamps at content-viewport (60-40=20)");
	YMGUI_TextView_SetScroll(tv, 0);

	//---- 宽度变化触发行表重排:缩到 60(内容宽 52 → 6 字/行)→ 30 字 → 5 行 ----
	//  (真实用法里布局助手改完尺寸会 Invalidate;这里手动模拟,再靠 draw_cb 检测 last_w 变化重排)
	tv->area.w = 60;
	YMGUI_Obj_Invalidate(tv);
	YMGUI_Refresh(ctx);//draw_cb 检测到 last_w != area.w → rebuildLines
	CHECK(YMGUI_TextView_GetLineCount(tv) == 5, "width shrink reflows: 30 chars / 6 per line = 5 lines");
	tv->area.w = 100;
	YMGUI_Obj_Invalidate(tv);
	YMGUI_Refresh(ctx);
	CHECK(YMGUI_TextView_GetLineCount(tv) == 3, "width restore reflows back to 3 lines");

	//---- 拖动滚动:按下→按住上移超阈值 → scroll 增加 ----
	YMGUI_TextView_SetScroll(tv, 0);
	YMGUI_Event_Pointer(ctx, 50, 30, 1);//按下于视图内
	YMGUI_Event_Pointer(ctx, 50, 15, 1);//上移 15px
	YMGUI_Event_Pointer(ctx, 50, 15, 0);//抬起
	CHECK(YMGUI_TextView_GetScroll(tv) == 15, "drag scrolls content by delta (15)");

	//---- 文字实际绘制:短文本首行区域应出现非背景像素 ----
	YMGUI_TextView_SetWrap(tv, 0);
	YMGUI_TextView_SetScroll(tv, 0);
	YMGUI_TextView_SetText(tv, "ABC");
	YMGUI_Refresh(ctx);
	GYpx bg = GY_ColorToPx(GY_ARGB(0xFF, 0x1C, 0x1C, 0x24));
	int nonbg = 0;
	//首行 y≈10..26,文字自 x=14 起
	for (GYcoord yy = 10; yy < 26; yy++)
		for (GYcoord xx = 14; xx < 40; xx++)
			if (!GY_PxEqual(px_at(xx, yy), bg))
				nonbg++;
	CHECK(nonbg > 0, "text glyph pixels drawn in first line");

	//---- 析构不崩(文本副本 + 行表释放)----
	YMGUI_Free_ObjFree(tv);

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_textview: ALL PASS\n");
	else
		printf("test_textview: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
