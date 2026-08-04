#include "YMGUI_PubDefine.h"
#include "YMGUI_Surface.h"
#include "YMGUI_Font.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_font.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 位图字体绘制单测:字形有像素落地、推进宽度、裁剪、band偏移、范围外字符安全
  ***************************************************************************************************************************/

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

//---- 外部 flash 模拟:一个 8x16 1bpp 单字形字体,字节经回调取回 ----
//"假 flash":字形 'X' 点阵(每行 1 字节,MSB 左),对角线两条
static const uint8 s_fake_flash[16] = {
	0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81,
	0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81,
};
static int s_read_calls = 0;
static uint32 fakeRead(const struct YMGUI_font* font, uint32 off, uint32 len, uint8* buf)
{
	(void)font;
	s_read_calls++;
	for (uint32 i = 0; i < len; i++)
		buf[i] = s_fake_flash[off + i];
	return len;
}
static const GYfont s_extfont =
{
	NULL,        //bitmap 不直接用(走回调)
	NULL, 0,     //连续模式
	0x58, 0x58,  //只含 'X'
	8, 16, 1, 1, //8x16 1bpp
	NULL,        //fallback
	fakeRead,    //glyph_read → 假 flash
};

//统计一块 buffer 里非零像素数
static int countSet(const GYpx* buf, int n)
{
	int c = 0;
	for (int i = 0; i < n; i++) if (buf[i]) c++;
	return c;
}

int main(void)
{
	GYFONT f = &YMGUI_Font_Default;
	CHECK(f->cell_w == 8 && f->cell_h == 16, "default font 8x16");
	CHECK(f->first_char == 32 && f->last_char == 126, "ascii 32..126");

	//---- 32x16 buffer 当一整条 band(屏幕 0,0 起) ----
	GYpx buf[32 * 16];
	GYsurface s;
	s.buf = buf; s.stride = 32;
	s.buf_area = (GYrect){0, 0, 32, 16};
	s.clip = s.buf_area;
	for (int i = 0; i < 32 * 16; i++) buf[i] = 0;

	//画 'A':应有若干像素置位,且推进 8
	GYcoord adv = YMGUI_Draw_Char(&s, f, 0, 0, 'A', GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	CHECK(adv == 8, "char advance = cell_w");
	int naA = countSet(buf, 32 * 16);
	CHECK(naA > 8, "'A' produced pixels");

	//空格应几乎不置位
	for (int i = 0; i < 32 * 16; i++) buf[i] = 0;
	YMGUI_Draw_Char(&s, f, 0, 0, ' ', GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	CHECK(countSet(buf, 32 * 16) == 0, "space has no pixels");

	//文本宽度量算
	CHECK(YMGUI_Font_TextWidth(f, "Hello") == 5 * 8, "text width = len*cell_w");

	//---- 裁剪:clip 只留左 4 列,'A' 右半不应写出界 ----
	for (int i = 0; i < 32 * 16; i++) buf[i] = 0;
	s.clip = (GYrect){0, 0, 4, 16};
	YMGUI_Draw_Char(&s, f, 0, 0, 'A', GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	int clipped_ok = 1;
	for (int y = 0; y < 16; y++)
		for (int x = 4; x < 32; x++)
			if (buf[y * 32 + x]) clipped_ok = 0;
	CHECK(clipped_ok, "clip: no pixels beyond clip.x=4");

	//---- band 偏移:字符画在屏幕 y=20,band 覆盖 y=16..32 ----
	for (int i = 0; i < 32 * 16; i++) buf[i] = 0;
	s.buf_area = (GYrect){0, 16, 32, 16};
	s.clip = s.buf_area;
	YMGUI_Draw_Char(&s, f, 0, 20, 'H', GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	//屏幕 y=20 → buffer 行 (20-16)=4;字形前 4 行(空白上边距)之后应有像素
	//至少 buffer 里应有像素落在行 >=4
	int has_lower = 0;
	for (int y = 4; y < 16; y++)
		for (int x = 0; x < 8; x++)
			if (buf[y * 32 + x]) has_lower = 1;
	CHECK(has_lower, "band offset: 'H' pixels land at buffer row>=4");
	//行 0..3 (屏幕 16..19,在字符 y=20 之上) 应为空
	int upper_empty = 1;
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 8; x++)
			if (buf[y * 32 + x]) upper_empty = 0;
	CHECK(upper_empty, "band offset: rows above char are empty");

	//---- 范围外字符安全(不崩,推进正常) ----
	s.buf_area = (GYrect){0, 0, 32, 16};
	s.clip = s.buf_area;
	CHECK(YMGUI_Draw_Char(&s, f, 0, 0, (char)200, GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF)) == 8, "out-of-range char advances safely");

	//======================================================================
	// UTF-8 + CJK 回退链
	//======================================================================
#if YMGUI_FONT_CJK
	s.buf_area = (GYrect){0, 0, 32, 16};
	s.clip = s.buf_area;

	//---- 混排推进宽:ASCII 'A'(8) + 中文 '中'(16) ----
	//"A中" = 0x41, 0xE4 0xB8 0xAD(U+4E2D)
	CHECK(YMGUI_Font_TextWidth(f, "A中") == 8 + 16, "mixed width: ASCII 8 + CJK 16");
	CHECK(YMGUI_Font_TextWidth(f, "中文") == 16 + 16, "two CJK = 32");

	//---- CJK 码点经 fallback 链命中并落像素(16x16,画在 x=0) ----
	for (int i = 0; i < 32 * 16; i++) buf[i] = 0;
	GYcoord cadv = YMGUI_Draw_Glyph(&s, f, 0, 0, 0x4E2D, GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));//'中'
	CHECK(cadv == 16, "CJK glyph advance = 16");
	CHECK(countSet(buf, 32 * 16) > 16, "'中' produced pixels via fallback");

	//---- 二分查找:表内相邻码点都能命中(0x4E00 一 / 0x4E2D 中) ----
	int32 dummy;
	CHECK(YMGUI_Draw_Glyph(&s, f, 0, 0, 0x4E00, GY_ARGB(0xFF,0xFF,0xFF,0xFF)) == 16, "U+4E00 hit");

	//---- 缺字(表外码点)安全:仍推进(用回退起点 ASCII cell_w=8) ----
	for (int i = 0; i < 32 * 16; i++) buf[i] = 0;
	GYcoord madv = YMGUI_Draw_Glyph(&s, f, 0, 0, 0x9FA5, GY_ARGB(0xFF,0xFF,0xFF,0xFF));//'龥' 不在预置集
	CHECK(madv == 8, "missing CJK advances safely (ascii cell_w)");
	CHECK(countSet(buf, 32 * 16) == 0, "missing CJK draws nothing");

	//---- UTF-8 解码正确性:Draw_Text 画 "中文" 推进 32 且有像素 ----
	for (int i = 0; i < 32 * 16; i++) buf[i] = 0;
	GYcoord tadv = YMGUI_Draw_Text(&s, f, 0, 0, "中文", GY_ARGB(0xFF,0xFF,0xFF,0xFF));
	CHECK(tadv == 32, "Draw_Text '中文' advances 32");
	CHECK(countSet(buf, 32 * 16) > 16, "Draw_Text '中文' produced pixels");
	(void)dummy;
#endif

	//======================================================================
	// 外部 flash 字形读回调(非内存映射 SPI flash 语义)
	//======================================================================
	s.buf_area = (GYrect){0, 0, 32, 16};
	s.clip = s.buf_area;
	for (int i = 0; i < 32 * 16; i++) buf[i] = 0;
	s_read_calls = 0;
	GYcoord eadv = YMGUI_Draw_Glyph(&s, &s_extfont, 0, 0, 'X', GY_ARGB(0xFF,0xFF,0xFF,0xFF));
	CHECK(eadv == 8, "extflash glyph advance = 8");
	CHECK(s_read_calls == 1, "extflash glyph_read invoked once");
	CHECK(countSet(buf, 32 * 16) > 8, "extflash 'X' produced pixels via callback");

	if (fails == 0)
		printf("test_font: ALL PASS\n");
	else
		printf("test_font: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
