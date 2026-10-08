#ifndef YMGUI_FONT_CACHE_H
#define YMGUI_FONT_CACHE_H
#include "YMGUI_Font.h"
#include <stddef.h>
typedef uint32 (*GYglyph_source_cb)(void* user, uint32 offset, uint32 size, uint8* out);
typedef struct
{
	GYfont font; /* first member: read callback recovers this cache */
	GYglyph_source_cb read;
	void* user;
	void *small, *hot;
	size_t stride;
	uint32 glyph_size, clock, reads, small_hits, hot_hits, failures;
	uint16 small_count, hot_count, next;
	uint8 hot_enabled;
} GYfontcache;
/* Cache allocation failure degrades to the available tier or direct reads.
 * Caller owns the cache and source; do not initialize an active cache twice. */
uint8 YMGUI_FontCache_Init(GYfontcache* cache, GYFONT description, GYglyph_source_cb read, void* user, uint16 small_count, uint16 hot_count);
void YMGUI_FontCache_Clear(GYfontcache* cache);
void YMGUI_FontCache_Free(GYfontcache* cache);
#endif
