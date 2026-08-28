#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Image.h"
#include "YMGUI_Label.h"
#include "SDL_LCD.h"
#include <stdlib.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_image.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 图片控件缩放模式 demo:同一张宽图分别以 NONE(居中原尺寸)/FIT(等比留黑边)/
  *	              FILL(拉伸铺满)三种模式显示在同尺寸方框里,每隔若干帧轮换,直观对比。
  *	              背后是库缺口 Draw_ImgScaled(定点最近邻缩放图元)落地。
  *	@Version:     1.0
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240

//一张 32x18(16:9 宽图):横向青→洋红渐变 + 边框,便于看清缩放/宽高比
#define IMG_W 32
#define IMG_H 18
static GYpx s_img[IMG_W * IMG_H];
static void makeImg(void)
{
	for (int y = 0; y < IMG_H; y++)
	{
		for (int x = 0; x < IMG_W; x++)
		{
			//横向渐变
			int r = 0x20 + x * 0xC0 / IMG_W;
			int g = 0xE0 - x * 0xA0 / IMG_W;
			int b = 0x40 + x * 0xA0 / IMG_W;
			//边框画白,看清图片实际边界(FILL 会拉扁,FIT 保持比例)
			if (x == 0 || y == 0 || x == IMG_W - 1 || y == IMG_H - 1)
				s_img[y * IMG_W + x] = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
			else
				s_img[y * IMG_W + x] = GY_ColorToPx(GY_ARGB(0xFF, r, g, b));
		}
	}
}

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * SCR_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;

	makeImg();
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL;
	disp.user_data = NULL;

	SDL_LCD_Init(&disp, 2);
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x0E, 0x0E, 0x14));

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 10, 8, SCR_W - 20, 16);
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0x60, 0xD0, 0xF0));
	YMGUI_Label_SetText(title, "Image scale modes: NONE / FIT / FILL");

	static GYimg img = {s_img, IMG_W, IMG_H, 0, GY_PX_ZERO};
	//一个方框(160x120),背景近黑以看清 FIT 的黑边
	GYOBJ imw = YMGUI_Creat_Image_Creat(ctx->root, 80, 40, 160, 120);
	YMGUI_Obj_SetBgColor(imw, GY_ARGB(0xFF, 0x00, 0x00, 0x00));
	YMGUI_Image_SetSrc(imw, &img);

	GYOBJ lbl = YMGUI_Creat_Label_Creat(ctx->root, 10, SCR_H - 24, SCR_W - 20, 16);
	YMGUI_Label_SetTextColor(lbl, GY_ARGB(0xFF, 0xD0, 0xD0, 0xE0));

	const GYimg_scale_mode modes[] = { GY_IMG_NONE, GY_IMG_FIT, GY_IMG_FILL };
	const char* names[] = {
		"NONE: 32x18 居中原尺寸(小图不放大)",
		"FIT:  等比缩放放进方框,上下留黑边",
		"FILL: 拉伸铺满方框(宽高比被改变)",
	};
	int cur = -1;

	while (SDL_LCD_PumpEvents())
	{
		//每 60 帧(约 1s)换一种模式
		int idx = (frame / 60) % 3;
		if (idx != cur)
		{
			cur = idx;
			YMGUI_Image_SetScaleMode(imw, modes[idx]);
			YMGUI_Label_SetText(lbl, names[idx]);
		}
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		frame++;
		if (max_frames > 0 && frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_image exit ok\n");
	return 0;
}
