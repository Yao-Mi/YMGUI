#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_button.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 交互闭环 demo:点击按钮 → 事件分发 → 状态改变标脏 → Refresh 只重绘脏区
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static int s_click_count = 0;
static GYOBJ s_counter_label = NULL;//点击计数标签

//---- 图标按钮:程序生成 ▶/⏸ 两张图(colorkey 透明抠形,演示"贴图代替文字") ----
#define ICON_W 28
#define ICON_H 28
static GYpx s_play_px[ICON_W * ICON_H];  //▶ 播放三角
static GYpx s_pause_px[ICON_W * ICON_H]; //⏸ 暂停双竖条
static GYimg s_img_play;
static GYimg s_img_pause;
static GYOBJ s_icon_btn = NULL;
static int   s_playing  = 0;

//生成图标:key 透明底 + 白色前景。返回填好的 GYimg
static void buildIcons(void)
{
	GYcolor key   = GY_ARGB(0xFF, 0xFF, 0x00, 0xFF);//品红当透明色(画面里不会用)
	GYcolor fg    = GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0);//近白前景
	GYpx    keypx = GY_ColorToPx(key);
	GYpx    fgpx  = GY_ColorToPx(fg);
	int x, y;
	for (y = 0; y < ICON_H; y++)
	{
		for (x = 0; x < ICON_W; x++)
		{
			int idx = y * ICON_W + x;
			//播放:实心三角(x 从 6 到 22,高度随 x 收窄)
			int tri_span = (22 - x);           //右侧收口
			int tri_half = (x - 6);            //左宽右窄
			int on_play  = (x >= 6 && x <= 22 && (y - ICON_H / 2) <= tri_half
			                && (ICON_H / 2 - y) <= tri_half && tri_span >= 0);
			s_play_px[idx] = on_play ? fgpx : keypx;
			//暂停:两条竖条(x 在 7..11 或 17..21,y 6..21)
			int on_pause = ((( x >= 7 && x <= 11) || (x >= 17 && x <= 21)) && y >= 6 && y <= 21);
			s_pause_px[idx] = on_pause ? fgpx : keypx;
		}
	}
	s_img_play.data  = s_play_px;  s_img_play.w  = ICON_W; s_img_play.h  = ICON_H;
	s_img_play.use_key = 1;        s_img_play.key = keypx;
	s_img_pause.data = s_pause_px; s_img_pause.w = ICON_W; s_img_pause.h = ICON_H;
	s_img_pause.use_key = 1;       s_img_pause.key = keypx;
}

//图标按钮点击:切换播放/暂停图(状态由 app 换图,同 SetText 换字)
static void onIconClicked(GYOBJ btn)
{
	s_playing = !s_playing;
	YMGUI_Button_SetImage(btn, s_playing ? &s_img_pause : &s_img_play);
	gy_log_print("icon button -> %s\n", s_playing ? "playing(pause icon)" : "paused(play icon)");
}

/**
  * @brief 按钮点击回调:计数 + 换色 + 更新计数标签(证明状态驱动重绘)
  */
static void onBtnClicked(GYOBJ btn)
{
	s_click_count++;
	gy_log_print("button clicked! count=%d\n", s_click_count);
	//每次点击换常态色(在红/绿/蓝之间轮换)
	GYcolor palette[3] = {
		GY_ARGB(0xFF, 0xE0, 0x40, 0x40),
		GY_ARGB(0xFF, 0x40, 0xC0, 0x50),
		GY_ARGB(0xFF, 0x40, 0x80, 0xE0),
	};
	YMGUI_Button_SetColors(btn, palette[s_click_count % 3], GY_ARGB(0xFF, 0x20, 0x20, 0x20));
	//更新计数标签文字
	if (s_counter_label != NULL)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "clicks: %d", s_click_count);
		YMGUI_Label_SetText(s_counter_label, buf);
	}
}

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;

	//显示配置(draw buffer 只有 BAND_H 行)
	disp.hor_res    = SCR_W;
	disp.ver_res    = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1       = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2       = NULL;
	disp.user_data  = NULL;

	SDL_LCD_Init(&disp, 2);

	//建上下文 + 标题标签 + 按钮 + 计数标签
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));//深色背景

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 20, SCR_W, 20);
	YMGUI_Label_SetText(title, "YMGUI Demo");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	GYOBJ btn = YMGUI_Creat_Button_Creat(ctx->root, 100, 90, 120, 60);
	YMGUI_Button_SetText(btn, "Click Me");
	YMGUI_Button_SetClicked(btn, onBtnClicked);

	s_counter_label = YMGUI_Creat_Label_Creat(ctx->root, 0, 180, SCR_W, 20);
	YMGUI_Label_SetText(s_counter_label, "clicks: 0");
	YMGUI_Label_SetTextColor(s_counter_label, GY_ARGB(0xFF, 0xC0, 0xC0, 0xC0));

	//图标按钮:纯图标(关底色边框),点击切 ▶/⏸ —— 演示"贴图代替播放/暂停文字"
	buildIcons();
	s_icon_btn = YMGUI_Creat_Button_Creat(ctx->root, 146, 40, ICON_W, ICON_H);
	YMGUI_Button_SetBgVisible(s_icon_btn, 0);       //纯图标,无底色边框
	YMGUI_Button_SetImage(s_icon_btn, &s_img_play);
	YMGUI_Button_SetClicked(s_icon_btn, onIconClicked);

	//注册注入上下文:SDL 鼠标事件将派发到这里
	YMGUI_Inject_SetCtx(ctx);

	//主循环:抽事件(可能注入→标脏) → Refresh(仅脏区重绘)
	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_button exit ok (clicks=%d)\n", s_click_count);
	return 0;
}
