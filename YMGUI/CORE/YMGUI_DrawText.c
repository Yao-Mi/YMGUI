#include "YMGUI_Font.h"
#include "YMGUI_DrawPx.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_DrawText.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 位图字体绘制图元。UTF-8 逐码点解码 + 沿 fallback 链定位字形(连续/稀疏二分),
  *	              置位/覆盖度像素混合写入(裁剪+band偏移)。支持外部 flash 字形读回调
  *	@Version:     2.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.ASCII 走连续区间字体;CJK 走稀疏排序码点表(二分)。混排靠回退链,推进宽=命中字体 cell_w
  * 2.4bpp 灰度做抗锯齿(ANTIALIAS 开),1bpp 硬边直写;收屏幕坐标,与 clip/buf_area 求交只画本 band
  * 3.外部非映射 flash:字体设 glyph_read 回调,取字形时先拷进栈缓冲再 blit
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

//一个字形最大字节数(16x16 4bpp = 16 行 x 8 字节 = 128;留足 external flash 拷贝栈缓冲)
#define GY_GLYPH_BUF_MAX 256

/**
  * @brief UTF-8 解码:取 *p 处一个码点并推进 *p。非法/截断字节按单字节吞掉(返回其原值)
  */
static uint32 utf8_next(const char** p)
{
	const uint8* s = (const uint8*)(*p);
	uint32 c = s[0];
	if (c < 0x80)                        //ASCII 单字节
	{
		*p += 1;
		return c;
	}
	if ((c & 0xE0) == 0xC0)              //2 字节 110xxxxx
	{
		if ((s[1] & 0xC0) == 0x80)
		{
			*p += 2;
			return ((c & 0x1F) << 6) | (s[1] & 0x3F);
		}
	}
	else if ((c & 0xF0) == 0xE0)         //3 字节 1110xxxx(覆盖 BMP,含 CJK)
	{
		if ((s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80)
		{
			*p += 3;
			return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
		}
	}
	else if ((c & 0xF8) == 0xF0)         //4 字节 11110xxx(BMP 外,本库暂不存字形)
	{
		if ((s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80)
		{
			*p += 4;
			return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
		}
	}
	*p += 1;                             //非法/截断:吞一字节防死循环
	return c;
}

/**
  * @brief 在单个字体里查码点的字形序号,未命中返回 -1
  *        连续模式:first..last 直接算;稀疏模式:codepoints[] 二分
  */
static int32 glyphIndex(GYFONT font, uint32 cp)
{
	if (font->codepoints == NULL)        //连续区间(ASCII)
	{
		if (cp >= font->first_char && cp <= font->last_char)
			return (int32)(cp - font->first_char);
		return -1;
	}
	//稀疏排序码点表:二分查找
	int32 lo = 0, hi = (int32)font->glyph_count - 1;
	while (lo <= hi)
	{
		int32 mid = (lo + hi) >> 1;
		uint16 v = font->codepoints[mid];
		if (v == cp)
			return mid;
		if (v < cp)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return -1;
}

//运行期全局兜底字体(见 YMGUI_Font_SetFallback);NULL=不生效。存 RAM,字体数据本身仍 const。
static GYFONT s_ext_fallback = NULL;

void YMGUI_Font_SetFallback(GYFONT font)
{
	s_ext_fallback = font;
}

GYFONT YMGUI_Font_GetFallback(void)
{
	return s_ext_fallback;
}

/**
  * @brief 沿 fallback 链找拥有该码点的字体 + 字形序号。都没有则返回起点字体、idx=-1
  *        const 链(font->fallback)走空后,再下探运行期全局兜底 s_ext_fallback(若设了)
  */
static GYFONT resolveGlyph(GYFONT font, uint32 cp, int32* out_idx)
{
	for (GYFONT f = font; f != NULL; f = f->fallback)
	{
		int32 idx = glyphIndex(f, cp);
		if (idx >= 0)
		{
			*out_idx = idx;
			return f;
		}
	}
	//const 链没命中:试运行期兜底字体(它自己也可能带 fallback 链,顺着走)
	for (GYFONT f = s_ext_fallback; f != NULL; f = f->fallback)
	{
		int32 idx = glyphIndex(f, cp);
		if (idx >= 0)
		{
			*out_idx = idx;
			return f;
		}
	}
	*out_idx = -1;
	return font;                         //回退链走空:用起点字体的 cell_w 做推进
}

/**
  * @brief 画一个已定位字形(font 里第 idx 个),内部裁剪+band偏移。gp 指向该字形点阵基址
  */
static void blitGlyph(GYSURFACE s, GYFONT font, GYcoord x, GYcoord y, const uint8* gp, GYcolor color)
{
	GYrect cell = {x, y, font->cell_w, font->cell_h};
	GYrect vis;
	if (!GY_Rect_Intersect(&vis, &cell, &s->clip))
		return;                          //完全在裁剪区外
	if (!GY_Rect_Intersect(&vis, &vis, &s->buf_area))
		return;                          //防 clip 非 buf_area 子集时越界

	GYpx* bufp = (GYpx*)s->buf;
	GYpx  fillpx = GY_ColorToPx(color);

	for (GYcoord sy = vis.y; sy < vis.y + vis.h; sy++)
	{
		GYcoord gy_row = sy - y;                     //字形内行号
		const uint8* rowp = gp + (uint32)gy_row * font->bytes_per_row;
		GYcoord by = sy - s->buf_area.y;             //band buffer 行
		GYpx* dst = bufp + (int32)by * s->stride;
		for (GYcoord sx = vis.x; sx < vis.x + vis.w; sx++)
		{
			GYcoord gx = sx - x;                     //字形内列号
			GYcoord bx = sx - s->buf_area.x;
			if (font->bpp == 4)
			{
				//4bpp 灰度:每字节 2 像素(高 nibble 在左),nibble*17 → 覆盖度 0..255
				uint8 byte = rowp[gx >> 1];
				uint8 nib = (gx & 1) ? (byte & 0x0F) : (byte >> 4);
#if YMGUI_ANTIALIAS
				GYopa opa = (GYopa)(nib * 17);       //0..15 → 0..255
				if (opa == GY_OPA_COVER)
					dst[bx] = fillpx;
				else if (opa != GY_OPA_TRANSP)
					dst[bx] = GY_MixPx(dst[bx], color, opa);
#else
				if (nib >= 8)                        //关 AA:阈值退化硬边
					dst[bx] = fillpx;
#endif
			}
			else
			{
				//1bpp 硬边:取位 MSB 在左,置位直写
				uint8 bits = rowp[gx >> 3];
				if (bits & (0x80 >> (gx & 7)))
					dst[bx] = fillpx;
			}
		}
	}
}

/**
  * @brief 画单个 Unicode 码点(沿 fallback 链找字形),返回推进宽度
  */
GYcoord YMGUI_Draw_Glyph(GYSURFACE s, GYFONT font, GYcoord x, GYcoord y, uint32 cp, GYcolor color)
{
	gy_assert(s && font);
	gy_log_explain((s == NULL) || (font == NULL), GY_LOG_PtrI, "表面或字体不存在");

	int32 idx;
	GYFONT gf = resolveGlyph(font, cp, &idx);
	if (idx < 0)
		return gf->cell_w;               //缺字:仍推进,便于对齐

	uint32 glyph_sz = (uint32)gf->cell_h * gf->bytes_per_row;
	uint32 off = (uint32)idx * glyph_sz;
	if (gf->glyph_read != NULL)
	{
		//外部非映射 flash:先把字形拷进栈缓冲再 blit
		uint8 buf[GY_GLYPH_BUF_MAX];
		if (glyph_sz > GY_GLYPH_BUF_MAX)
			return gf->cell_w;           //字形超缓冲(不该发生):安全推进
		gf->glyph_read(gf, off, glyph_sz, buf);
		blitGlyph(s, gf, x, y, buf, color);
	}
	else
	{
		blitGlyph(s, gf, x, y, gf->bitmap + off, color);
	}
	return gf->cell_w;
}

/**
  * @brief 画单个 ASCII 字符(薄封装,兼容老调用)
  */
GYcoord YMGUI_Draw_Char(GYSURFACE s, GYFONT font, GYcoord x, GYcoord y, char c, GYcolor color)
{
	return YMGUI_Draw_Glyph(s, font, x, y, (uint32)(uint8)c, color);
}

/**
  * @brief 画字符串(UTF-8,不处理换行),返回总推进宽度
  */
GYcoord YMGUI_Draw_Text(GYSURFACE s, GYFONT font, GYcoord x, GYcoord y, const char* str, GYcolor color)
{
	gy_assert(s && font && str);
	gy_log_explain((s == NULL) || (font == NULL) || (str == NULL), GY_LOG_PtrI, "表面/字体/字符串不存在");

	GYcoord cx = x;
	while (*str != '\0')
	{
		uint32 cp = utf8_next(&str);
		cx += YMGUI_Draw_Glyph(s, font, cx, y, cp, color);
	}
	return cx - x;
}

/**
  * @brief UTF-8 解码(带尾界):从 *p 取一码点并推进,但不越过 end。
  *        若一个多字节序列会越过 end(被 nbytes 截断),按单字节吞掉,防读到界外字节
  */
static uint32 utf8_next_bounded(const char** p, const char* end)
{
	const uint8* s = (const uint8*)(*p);
	uint32 c = s[0];
	uint32 need = 1;
	if (c < 0x80)                        need = 1;   //ASCII
	else if ((c & 0xE0) == 0xC0)         need = 2;   //2 字节
	else if ((c & 0xF0) == 0xE0)         need = 3;   //3 字节(CJK)
	else if ((c & 0xF8) == 0xF0)         need = 4;   //4 字节
	//序列会越界:退化成单字节(交给非界版逻辑吞一字节)
	if ((const char*)(s + need) > end)
	{
		*p += 1;
		return c;
	}
	return utf8_next(p);                 //在界内,复用主解码器
}

/**
  * @brief 画字符串前 nbytes 字节(UTF-8,不处理换行,不要求 '\0' 结尾),返回总推进宽度
  */
GYcoord YMGUI_Draw_TextN(GYSURFACE s, GYFONT font, GYcoord x, GYcoord y, const char* str, uint32 nbytes, GYcolor color)
{
	gy_assert(s && font && str);
	gy_log_explain((s == NULL) || (font == NULL) || (str == NULL), GY_LOG_PtrI, "表面/字体/字符串不存在");

	const char* end = str + nbytes;
	GYcoord cx = x;
	while (str < end)
	{
		uint32 cp = utf8_next_bounded(&str, end);
		cx += YMGUI_Draw_Glyph(s, font, cx, y, cp, color);
	}
	return cx - x;
}

/**
  * @brief 量前 nbytes 字节(UTF-8,不要求 '\0' 结尾)的像素宽度(不绘制)
  */
GYcoord YMGUI_Font_TextWidthN(GYFONT font, const char* str, uint32 nbytes)
{
	gy_assert(font && str);
	gy_log_explain((font == NULL) || (str == NULL), GY_LOG_PtrI, "字体或字符串不存在");
	const char* end = str + nbytes;
	GYcoord n = 0;
	while (str < end)
	{
		uint32 cp = utf8_next_bounded(&str, end);
		int32 idx;
		GYFONT gf = resolveGlyph(font, cp, &idx);
		n += gf->cell_w;
	}
	return n;
}

/**
  * @brief 量一段文本(UTF-8)像素宽度(不绘制)。逐码点沿链取命中字体 cell_w 累加
  */
GYcoord YMGUI_Font_TextWidth(GYFONT font, const char* str)
{
	gy_assert(font && str);
	gy_log_explain((font == NULL) || (str == NULL), GY_LOG_PtrI, "字体或字符串不存在");
	GYcoord n = 0;
	while (*str != '\0')
	{
		uint32 cp = utf8_next(&str);
		int32 idx;
		GYFONT gf = resolveGlyph(font, cp, &idx);
		n += gf->cell_w;
	}
	return n;
}
