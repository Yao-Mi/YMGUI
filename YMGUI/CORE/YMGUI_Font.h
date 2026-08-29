#ifndef YMGUI_FONT_H
#define YMGUI_FONT_H

#include "YMGUI_PubType.h"
#include "YMGUI_Surface.h"

//===========================================================================
// 位图字体(定宽点阵)。裸机友好:纯数据,无 FPU/stb 依赖
//   字形定位两种模式:
//     codepoints==NULL → 连续区间 first_char..last_char(ASCII 默认字体)
//     codepoints!=NULL → 稀疏排序码点表 codepoints[0..glyph_count-1](CJK,二分查找)
//   混排靠回退链:本字体无此码点则下探 fallback。cell_h 各字体一致时基线自动对齐
//   外部 flash:glyph_read!=NULL 时字形字节要先经回调拷进 RAM(非内存映射 SPI flash)
//===========================================================================
struct YMGUI_font;//前置声明(回调 typedef 与 fallback 自引用用)

//外部 flash 字形读回调:把 bitmap 偏移 off 处 len 字节拷进 buf,返回拷贝字节数
typedef uint32 (*GYglyph_read_cb)(const struct YMGUI_font* font, uint32 off, uint32 len, uint8* buf);

typedef struct YMGUI_font
{
	const uint8* bitmap;       //点阵数据基址(glyph_read!=NULL 时是外部 flash 逻辑偏移基准)
	const uint16* codepoints;  //稀疏排序码点表;NULL=用连续 first_char..last_char
	uint16       glyph_count;  //codepoints 表长度(仅稀疏模式)
	uint8        first_char;   //首字符码(连续模式,通常 32 空格)
	uint8        last_char;    //末字符码(连续模式,通常 126 ~)
	uint8        cell_w;       //单元格宽(像素)
	uint8        cell_h;       //单元格高(像素)
	uint8        bytes_per_row;//每行字节数:1bpp=(cell_w+7)/8;4bpp=(cell_w*4+7)/8
	uint8        bpp;          //每像素位数:1=硬边点阵;4=灰度覆盖度(抗锯齿)
	const struct YMGUI_font* fallback;//缺字时下探的下一字体(NULL=链尾)
	GYglyph_read_cb glyph_read;//外部 flash 读回调(NULL=直接用 bitmap 指针,内部/映射 flash)
}GYfont;
typedef const GYfont* GYFONT;

//库内嵌的默认字体(见 YMGUI_FontData.c,由 tools/gen_font.py 生成)
extern const GYfont YMGUI_Font_Default;

//运行期全局兜底字体:任一字体的 const fallback 链走空后,再下探这个(默认 NULL 无影响)。
//  用途:把"运行期才知道的"字库(如放外部 flash 的 GB2312 全集,app 自造 GYfont + glyph_read
//  回调)挂进所有控件的回退链——控件仍用 &YMGUI_Font_Default,零改动即出任意中文。
//  代价:.bss 一个指针(NULL 时不生效);字体数据本身仍全 const。传 NULL 解除。
void  YMGUI_Font_SetFallback(GYFONT font);
GYFONT YMGUI_Font_GetFallback(void);

//---- 文本绘制图元(碰 surface,归 CORE) ----
//画单个 Unicode 码点(沿 fallback 链找字形),返回推进宽度(命中字体的 cell_w)
GYcoord YMGUI_Draw_Glyph(GYSURFACE s, GYFONT font, GYcoord x, GYcoord y, uint32 cp, GYcolor color);
//画单个 ASCII 字符(薄封装,兼容老调用),返回推进宽度
GYcoord YMGUI_Draw_Char(GYSURFACE s, GYFONT font, GYcoord x, GYcoord y, char c, GYcolor color);
//画字符串(UTF-8,不处理换行),返回总推进宽度
GYcoord YMGUI_Draw_Text(GYSURFACE s, GYFONT font, GYcoord x, GYcoord y, const char* str, GYcolor color);
//画字符串前 nbytes 字节(UTF-8,不处理换行,不要求 '\0' 结尾),返回总推进宽度。
//  用于画大缓冲里的非结尾子串(如 TextView 的一行)。截断的多字节序列按单字节吞掉。
GYcoord YMGUI_Draw_TextN(GYSURFACE s, GYFONT font, GYcoord x, GYcoord y, const char* str, uint32 nbytes, GYcolor color);
//量一段文本(UTF-8)的像素宽度(不绘制)
GYcoord YMGUI_Font_TextWidth(GYFONT font, const char* str);
//量前 nbytes 字节(UTF-8,不要求 '\0' 结尾)的像素宽度(不绘制)
GYcoord YMGUI_Font_TextWidthN(GYFONT font, const char* str, uint32 nbytes);

#endif // !YMGUI_FONT_H
