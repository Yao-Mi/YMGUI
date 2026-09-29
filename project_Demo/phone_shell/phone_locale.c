#include "phone_locale.h"
#include "YMGUI_Font.h"
#include <stdio.h>
extern const uint16 YMGUI_GB2312_cps[], YMGUI_GB2312_glyph_count;
static FILE* font_file;
static GYFONT old_fallback;
static uint32 font_read(const GYfont* font, uint32 off, uint32 len, uint8* buf)
{
	(void)font;
	if (!font_file || fseek(font_file, (long)off, SEEK_SET))
		return 0;
	return (uint32)fread(buf, 1, len, font_file);
}
static GYfont chinese_font = {NULL, YMGUI_GB2312_cps, 0, 0, 0, 16, 16, 8, 4, NULL, font_read};
void PhoneLocale_Init(void)
{
	old_fallback = YMGUI_Font_GetFallback();
	font_file = fopen(GB2312_BIN_PATH, "rb");
	if (font_file)
	{
		chinese_font.glyph_count = YMGUI_GB2312_glyph_count;
		chinese_font.fallback = old_fallback;
		YMGUI_Font_SetFallback(&chinese_font);
	}
	else
		fprintf(stderr, "Yaomi: Chinese font resource unavailable\n");
}
void PhoneLocale_Shutdown(void)
{
	YMGUI_Font_SetFallback(old_fallback);
	if (font_file)
		fclose(font_file);
	font_file = NULL;
}
