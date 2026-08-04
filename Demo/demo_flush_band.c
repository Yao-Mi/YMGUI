#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Surface.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Mem.h"
#include "SDL_LCD.h"
#include <stdlib.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_flush_band.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 地基验证 demo:分块(band)刷新最小闭环
  *	             draw buffer 远小于整屏 → 逐 band 渲染 → flush 到假 LCD
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40   //draw buffer 只有 40 行(证明 buffer 可远小于整屏)

/**
  * @brief 把整屏按 BAND_H 切成若干 band,逐条渲染+flush
  *        画三个重叠矩形(含一个半透明)验证填充+裁剪+混合
  */
static void renderFrame(GYDISP disp)
{
	GYsurface s;
	GYcoord   y;
	//三个测试矩形(屏幕坐标)
	GYrect r1 = {30, 30, 160, 120};
	GYrect r2 = {120, 90, 160, 120};
	GYrect r3 = {70, 60, 180, 100};

	s.buf    = disp->buf1;
	s.stride = disp->hor_res;//每条 band 宽度=整屏宽

	//逐 band 扫描
	for (y = 0; y < disp->ver_res; y += BAND_H)
	{
		GYcoord bh = (y + BAND_H <= disp->ver_res) ? BAND_H : (disp->ver_res - y);
		//本条 band 映射到屏幕的矩形
		s.buf_area.x = 0;
		s.buf_area.y = y;
		s.buf_area.w = disp->hor_res;
		s.buf_area.h = bh;
		//裁剪区先设为整条 band(绘制函数会再与图元求交)
		s.clip = s.buf_area;

		//背景先铺深灰(不透明直写)
		YMGUI_Draw_Fill(&s, &s.buf_area, GY_ARGB(0xFF, 0x20, 0x20, 0x28), GY_OPA_COVER);
		//三个矩形:红、绿(不透明) + 蓝(半透明,验证混合)
		YMGUI_Draw_Fill(&s, &r1, GY_ARGB(0xFF, 0xE0, 0x40, 0x40), GY_OPA_COVER);
		YMGUI_Draw_Fill(&s, &r2, GY_ARGB(0xFF, 0x40, 0xC0, 0x50), GY_OPA_COVER);
		YMGUI_Draw_Fill(&s, &r3, GY_ARGB(0xFF, 0x50, 0x80, 0xF0), 128);

		//这条 band 渲染完,推给面板(唯一碰硬件处)
		disp->flush_cb(disp, &s.buf_area, s.buf);
	}
}

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;//draw buffer 大小 = 一条 band
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;//>0 时跑够帧数自动退出(用于无头测试)
	int frame = 0;

	//配置显示:draw buffer 只有 BAND_H 行,远小于整屏
	disp.hor_res    = SCR_W;
	disp.ver_res    = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1       = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2       = NULL;
	disp.user_data  = NULL;

	SDL_LCD_Init(&disp, 2);//窗口放大 2 倍便于观察

	//主循环:渲染一帧 → 抽事件 → 退出则结束
	while (SDL_LCD_PumpEvents())
	{
		renderFrame(&disp);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo exit ok\n");
	return 0;
}
