#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_Font.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_font_gb2312.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 方案3 + 外部 flash 演示:GB2312 全集(汉字+符号,7448 项)字模不进固件,放"外部 flash"
  *	              (此处用磁盘 blob gb2312_glyphs.bin 模拟)。固件内只留码点索引 YMGUI_GB2312_cps[]
  *	              (~13.5KB)。app 自造 GYfont s_gb_font,填 glyph_read=flashRead 回调:第 i 字
  *	              在 blob 的 i*128 偏移。YMGUI_Font_SetFallback(&s_gb_font) 把它挂进全局回退链,
  *	              所有控件(仍用默认 ASCII 字体)遇任意 GB2312 汉字自动走外部路径,零控件改动。
  *	              移植真机:只改 flashRead 函数体为 W25Q_Read(BASE+off, buf, len) 即可。
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
#define BAND_H 40

//固件内只驻留的索引表(无 bitmap),见 CORE/YMGUI_FontDataGB2312.c
extern const uint16 YMGUI_GB2312_cps[];
extern const uint16 YMGUI_GB2312_glyph_count;

static long s_flash_reads = 0;//回调命中计数(证明外部路径真被走)

//外部 flash 读回调:从 blob 的 off 处拷 len 字节进 buf。真机换成 SPI flash 读即可。
static uint32 flashRead(const GYfont* font, uint32 off, uint32 len, uint8* buf)
{
	(void)font;
	FILE* f = fopen(GB2312_BIN_PATH, "rb");
	if (!f)
		return 0;
	if (fseek(f, (long)off, SEEK_SET) != 0)
	{
		fclose(f);
		return 0;
	}
	size_t got = fread(buf, 1, len, f);
	fclose(f);
	s_flash_reads++;
	return (uint32)got;
}

//app 侧自造的 GYfont:bitmap=NULL(字形不在内存),靠 glyph_read 回调取字节。
//字段顺序须与 YMGUI_font 定义一致(位置初始化)。
static GYfont s_gb_font =
{
	NULL,                 //bitmap:外部 flash,不用指针
	YMGUI_GB2312_cps,     //codepoints:排序码点索引表(含符号区,7448 项)
	0,                    //glyph_count:非编译期常量,main 运行期填 YMGUI_GB2312_glyph_count
	0, 0,                 //first_char/last_char:稀疏模式不用
	16, 16,               //cell_w/cell_h
	8,                    //bytes_per_row:16*4bpp/8
	4,                    //bpp
	NULL,                 //fallback:链尾
	flashRead             //glyph_read:外部 flash 读回调
};

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;

	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL;
	disp.user_data = NULL;

	SDL_LCD_Init(&disp, 2);

	//把 GB2312 全集挂进全局回退链:控件仍用默认 ASCII 字体,遇汉字自动下探到这
	s_gb_font.glyph_count = YMGUI_GB2312_glyph_count;
	YMGUI_Font_SetFallback(&s_gb_font);

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));

	//全用 PRESET_CJK 之外的字(方案1 精简集不含),证明走的是外部 GB2312 全集
	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 8, SCR_W, 16);
	YMGUI_Label_SetText(title, "欢迎光临智能仪表盘");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	GYOBJ line1 = YMGUI_Creat_Label_Creat(ctx->root, 20, 40, 280, 16);
	YMGUI_Label_SetText(line1, "谨慎驾驶 蓝牙已连接");
	YMGUI_Label_SetTextColor(line1, GY_ARGB(0xFF, 0xC0, 0xC0, 0xC0));

	GYOBJ line2 = YMGUI_Creat_Label_Creat(ctx->root, 20, 68, 280, 16);
	YMGUI_Label_SetText(line2, "剩余里程 续航估算");
	YMGUI_Label_SetTextColor(line2, GY_ARGB(0xFF, 0x80, 0xE0, 0xB0));

	GYOBJ save = YMGUI_Creat_Button_Creat(ctx->root, 40, 120, 100, 40);
	YMGUI_Button_SetText(save, "保存配置");
	GYOBJ quit = YMGUI_Creat_Button_Creat(ctx->root, 180, 120, 100, 40);
	YMGUI_Button_SetText(quit, "退出登录");

	GYOBJ hint = YMGUI_Creat_Label_Creat(ctx->root, 0, 200, SCR_W, 16);
	YMGUI_Label_SetText(hint, "GB2312 full set (hanzi+symbols) in ext flash");
	YMGUI_Label_SetTextColor(hint, GY_ARGB(0xFF, 0x70, 0x70, 0x78));

	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
		else if (max_frames <= 0)
			frame++;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_font_gb2312 exit ok, flash reads=%ld, indexed=%u\n",
	             s_flash_reads, (unsigned)YMGUI_GB2312_glyph_count);
	return 0;
}
