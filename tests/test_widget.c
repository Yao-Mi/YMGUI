#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_widget.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 控件文字单测:label/button 设文字后渲染出像素,SetText 标脏,级联释放不泄漏
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static long g_flush_set;//本帧 flush 出的非零像素累计
static int g_check_btn_clip;
static int g_btn_clip_leak;
static void countFlushCb(GYdisp* d, const GYrect* area, const GYpx* buf)
{
	(void)d;
	long n = (long)area->w * area->h;
	for (long i = 0; i < n; i++)
	{
		if (buf[i]) g_flush_set++;
		if (g_check_btn_clip && buf[i])
		{
			GYcoord x = area->x + (GYcoord)(i % area->w);
			GYcoord y = area->y + (GYcoord)(i / area->w);
			if (y >= 180 && y < 204 && (x < 100 || x >= 112)) g_btn_clip_leak = 1;
		}
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
	disp.flush_cb = countFlushCb;
	disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x00, 0x00, 0x00));

	//---- Label 设文字 → 渲染应产生文字像素 ----
	GYOBJ lb = YMGUI_Creat_Label_Creat(ctx->root, 10, 10, 200, 20);
	YMGUI_Label_SetTextColor(lb, GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	YMGUI_Label_SetText(lb, "Hello");
	CHECK(ctx->inv_cnt > 0, "SetText invalidates");

	g_flush_set = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_flush_set > 20, "label text rendered visible pixels");

	//空文本:刷新后基本无字形像素(只黑底)
	YMGUI_Label_SetText(lb, "");
	g_flush_set = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_flush_set == 0, "empty label → no text pixels (black bg)");

	//---- Button 设标题 → 渲染应有像素(按钮体+文字) ----
	GYOBJ btn = YMGUI_Creat_Button_Creat(ctx->root, 100, 90, 120, 60);
	YMGUI_Button_SetText(btn, "OK");
	g_flush_set = 0;
	YMGUI_Refresh(ctx);
	CHECK(g_flush_set > 100, "button body+text rendered");

	//过长标题在首帧/整屏重绘时也不能画出按钮矩形。
	GYOBJ narrow = YMGUI_Creat_Button_Creat(ctx->root, 100, 180, 12, 24);
	YMGUI_Button_SetText(narrow, "LONG");
	g_check_btn_clip = 1; g_btn_clip_leak = 0;
	YMGUI_Obj_Invalidate(ctx->root);
	YMGUI_Refresh(ctx);
	g_check_btn_clip = 0;
	CHECK(!g_btn_clip_leak, "long button title clipped to button bounds");

	//---- 级联释放:释放 root 子树不崩,引用清理 ----
	YMGUI_Free_ObjFree(btn);
	YMGUI_Free_ObjFree(narrow);
	YMGUI_Free_ObjFree(lb);
	CHECK(ctx->root->child_head == NULL, "children detached after free");

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_widget: ALL PASS\n");
	else
		printf("test_widget: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
