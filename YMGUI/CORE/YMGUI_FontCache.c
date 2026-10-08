#include "YMGUI_FontCache.h"
#include "YMGUI_Mem.h"
#include <string.h>
typedef struct
{
	uint32 offset, stamp;
} CacheEntry;
static CacheEntry* entryAt(void* data, size_t stride, unsigned index)
{
	return (CacheEntry*)((uint8*)data + stride * index);
}
static uint32 nextStamp(GYfontcache* c)
{
	if (c->clock == 0xffffffffu)
	{
		for (unsigned i = 0; i < c->hot_count; ++i)
			if (entryAt(c->hot, c->stride, i)->stamp)
				entryAt(c->hot, c->stride, i)->stamp = 1;
		c->clock = 1;
	}
	return ++c->clock;
}
static uint32 cacheRead(GYFONT font, uint32 off, uint32 len, uint8* out)
{
	GYfontcache* c = (GYfontcache*)font;
	if (!out)
		return 0;
	memset(out, 0, len);
	if (!c->read || len != c->glyph_size || !len || off % len || off / len >= c->font.glyph_count)
		return 0;
	CacheEntry* victim = NULL;
	if (c->hot_enabled)
		for (unsigned i = 0; i < c->hot_count; ++i)
		{
			CacheEntry* e = entryAt(c->hot, c->stride, i);
			if (e->stamp && e->offset == off)
			{
				memcpy(out, e + 1, len);
				e->stamp = nextStamp(c);
				++c->hot_hits;
				return len;
			}
			if (!victim || e->stamp < victim->stamp)
				victim = e;
		}
	for (unsigned i = 0; i < c->small_count; ++i)
	{
		CacheEntry* e = entryAt(c->small, c->stride, i);
		if (e->stamp && e->offset == off)
		{
			memcpy(out, e + 1, len);
			++c->small_hits;
			goto remember;
		}
	}
	++c->reads;
	if (c->read(c->user, off, len, out) != len)
	{
		memset(out, 0, len);
		++c->failures;
		return 0;
	}
	if (c->small_count)
	{
		CacheEntry* e = entryAt(c->small, c->stride, c->next);
		e->offset = off;
		memcpy(e + 1, out, len);
		e->stamp = 1;
		c->next = (uint16)((c->next + 1) % c->small_count);
	}
remember:
	if (victim)
	{
		victim->offset = off;
		memcpy(victim + 1, out, len);
		victim->stamp = nextStamp(c);
	}
	return len;
}
uint8 YMGUI_FontCache_Init(GYfontcache* c, GYFONT font, GYglyph_source_cb read, void* user, uint16 small_count, uint16 hot_count)
{
	if (!c || !font || !read || !font->cell_h || !font->bytes_per_row || !font->glyph_count)
		return 0;
	memset(c, 0, sizeof(*c));
	c->font = *font;
	c->font.bitmap = NULL;
	c->font.glyph_read = cacheRead;
	c->read = read;
	c->user = user;
	c->glyph_size = (uint32)font->cell_h * font->bytes_per_row;
	c->stride = (sizeof(CacheEntry) + c->glyph_size + 7u) & ~(size_t)7u;
	if (small_count && c->stride <= ((size_t)-1) / small_count)
		c->small = GY_malloc0(c->stride * small_count);
	if (hot_count && c->stride <= ((size_t)-1) / hot_count)
		c->hot = GY_malloc1(c->stride * hot_count);
	c->small_count = c->small ? small_count : 0;
	c->hot_count = c->hot ? hot_count : 0;
	c->hot_enabled = 1;
	YMGUI_FontCache_Clear(c);
	return 1;
}
void YMGUI_FontCache_Clear(GYfontcache* c)
{
	if (!c)
		return;
	if (c->small)
		memset(c->small, 0, c->stride * c->small_count);
	if (c->hot)
		memset(c->hot, 0, c->stride * c->hot_count);
	c->clock = c->next = 0;
}
void YMGUI_FontCache_Free(GYfontcache* c)
{
	if (!c)
		return;
	GY_free0(c->small);
	GY_free1(c->hot);
	memset(c, 0, sizeof(*c));
}
