#include "phone_locale.h"
#include "YMGUI_FontCache.h"
#include "YMGUI_Resource.h"
extern const uint16 YMGUI_GB2312_cps[], YMGUI_GB2312_glyph_count;
static GYresource file;
static GYfontcache cache;
static GYFONT old_fallback;
static int initialized;
static uint32 font_read(void* user, uint32 off, uint32 len, uint8* buf)
{
	return (uint32)YMGUI_Resource_Read((GYresource*)user, off, buf, len);
}
void PhoneLocale_Init(void)
{
	if (initialized)
		return;
	initialized = 1;
	old_fallback = YMGUI_Font_GetFallback();
	GYfont font = {NULL, YMGUI_GB2312_cps, 0, 0, 0, 16, 16, 8, 4, NULL, NULL};
	font.glyph_count = YMGUI_GB2312_glyph_count;
	font.fallback = old_fallback;
	uint8 probe[128];
	size_t size = (size_t)font.glyph_count * sizeof(probe);
	if (!YMGUI_Resource_Open(&file, GB2312_BIN_PATH))
		return;
	if (file.size != size || YMGUI_Resource_Read(&file, 0, probe, sizeof(probe)) != sizeof(probe) ||
		YMGUI_Resource_Read(&file, size - sizeof(probe), probe, sizeof(probe)) != sizeof(probe))
	{
		YMGUI_Resource_Close(&file);
		return;
	}
	if (YMGUI_FontCache_Init(&cache, &font, font_read, &file, 32, 128))
		YMGUI_Font_SetFallback(&cache.font);
	else
		YMGUI_Resource_Close(&file);
}
void PhoneLocale_Shutdown(void)
{
	if (!initialized)
		return;
	YMGUI_Font_SetFallback(old_fallback);
	YMGUI_FontCache_Free(&cache);
	YMGUI_Resource_Close(&file);
	old_fallback = NULL;
	initialized = 0;
}
