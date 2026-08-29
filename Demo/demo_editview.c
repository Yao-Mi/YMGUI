#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_EditView.h"
#include "YMGUI_Font.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_editview.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 可编辑多行文本框 demo:点击聚焦后用真键盘编辑(含中文输入法),ENTER 换行,退格/方向键,
  *	              超出可视区自动滚动。按钮切换 Wrap,另一按钮清空。验证中文输入 + 多行编辑。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#ifndef GB2312_BIN_PATH
#define GB2312_BIN_PATH "gb2312_glyphs.bin"
#endif

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60

static GYOBJ g_ev;
static uint8 g_wrap = 1;

//---- GB2312 汉字+符号全集(7448 字形)挂全局回退链,让编辑框能打任意中文 ----
//固件内只驻留的排序码点索引表(无 bitmap),字模在外部 blob(见 CORE/YMGUI_FontDataGB2312.c)
extern const uint16 YMGUI_GB2312_cps[];
extern const uint16 YMGUI_GB2312_glyph_count;

//blob 文件句柄:打开一次并缓存(编辑器每帧要读很多字,免反复 fopen)。
//真机上这对应"SPI flash 已初始化就绪",flashRead 直接 W25Q_Read(BASE+off,...) 即可。
static FILE* s_blob = NULL;

//外部 flash 读回调:从 blob 的 off 处拷 len 字节进 buf
static uint32 flashRead(const GYfont* font, uint32 off, uint32 len, uint8* buf)
{
	(void)font;
	if (s_blob == NULL)
		return 0;
	if (fseek(s_blob, (long)off, SEEK_SET) != 0)
		return 0;
	return (uint32)fread(buf, 1, len, s_blob);
}

//app 侧自造 GYfont:bitmap=NULL(字形不在内存),靠 glyph_read 取字节。字段顺序须与 YMGUI_font 一致
//glyph_count 在 main() 里用 YMGUI_GB2312_glyph_count 运行期填(C 静态初始化不能用 const 变量,
//留 0 占位;字数随字集增减不用再改这里)
static GYfont s_gb_font =
{
	NULL,                 //bitmap:外部 flash,不用指针
	YMGUI_GB2312_cps,     //codepoints:排序码点索引(符号标点 + 汉字)
	0,                    //glyph_count:main() 运行期填
	0, 0,                 //first_char/last_char:稀疏模式不用
	16, 16,               //cell_w/cell_h
	8,                    //bytes_per_row:16*4bpp/8
	4,                    //bpp
	NULL,                 //fallback:链尾
	flashRead             //glyph_read:外部 flash 读回调
};

static void onToggle(GYOBJ btn)
{
	g_wrap = g_wrap ? 0 : 1;
	YMGUI_EditView_SetWrap(g_ev, g_wrap);
	YMGUI_Button_SetText(btn, g_wrap ? "Wrap: ON" : "Wrap: OFF");
}

static void onClear(GYOBJ btn)
{
	(void)btn;
	YMGUI_EditView_SetText(g_ev, "");
}

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;

	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;

	SDL_LCD_Init(&disp, 2);

	//打开 GB2312 字模 blob 并挂进全局回退链:控件仍用默认 ASCII 字体,遇任意汉字自动下探到这。
	//打不开则退回内嵌 75 字精简集(仍能显示预置字,只是打生僻字会是空格)。
	s_blob = fopen(GB2312_BIN_PATH, "rb");
	if (s_blob != NULL)
	{
		s_gb_font.glyph_count = YMGUI_GB2312_glyph_count;//运行期填真实字数
		YMGUI_Font_SetFallback(&s_gb_font);
	}
	else
		gy_log_print("warn: GB2312 blob not found (%s), CJK limited to preset set\n", GB2312_BIN_PATH);

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 6, SCR_W, 16);
	YMGUI_Label_SetText(title, "多行可编辑(点击输入中文)");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	g_ev = YMGUI_Creat_EditView_Creat(ctx->root, 20, 28, 280, 150, 4096);
	YMGUI_EditView_SetText(g_ev, "点击这里编辑，随便打字试试。\nType here, Enter 换行。\n现在支持任意中文了。");
	YMGUI_EditView_SetWrap(g_ev, g_wrap);

	GYOBJ btnWrap = YMGUI_Creat_Button_Creat(ctx->root, 20, 186, 90, 26);
	YMGUI_Button_SetText(btnWrap, "Wrap: ON");
	YMGUI_Button_SetClicked(btnWrap, onToggle);

	GYOBJ btnClear = YMGUI_Creat_Button_Creat(ctx->root, 120, 186, 90, 26);
	YMGUI_Button_SetText(btnClear, "Clear");
	YMGUI_Button_SetClicked(btnClear, onClear);

	GYOBJ hint = YMGUI_Creat_Label_Creat(ctx->root, 20, 216, 280, 16);
	YMGUI_Label_SetText(hint, "Enter=newline  Backspace  Arrows  drag=scroll");
	YMGUI_Label_SetTextColor(hint, GY_ARGB(0xFF, 0xA0, 0xE0, 0xA0));

	YMGUI_Inject_SetCtx(ctx);

	//无头模式(argv[1] 给帧数上限):点一下聚焦,注入几个字节验证不崩
	if (max_frames > 0)
	{
		YMGUI_Inject_Pointer(60, 60, 1);
		YMGUI_Inject_Pointer(60, 60, 0);
		YMGUI_Inject_Key((uint32)(uint8)'X', 1);
		YMGUI_Inject_Key(GY_KEY_ENTER, 1);
	}

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
	if (s_blob != NULL)
		fclose(s_blob);
	gy_log_print("demo_editview exit ok\n");
	return 0;
}
