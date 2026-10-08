#ifdef NDEBUG
#undef NDEBUG
#endif
#include "YMGUI_Arena.h"
#include "YMGUI_Bus.h"
#include "YMGUI_StagedFlush.h"
#include "YMGUI_FontCache.h"
#include "YMGUI_Resource.h"
#include "YMGUI_Mem.h"
#include "YMGUI_ScaleBilinear.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static void arena_test(void)
{
	unsigned char storage[4097];
	GYarena a;
	assert(YMGUI_Arena_Init(&a, storage + 1, sizeof(storage) - 1));
	size_t largest = YMGUI_Arena_LargestFree(&a);
	void *x = YMGUI_Arena_Alloc(&a, 17), *y = YMGUI_Arena_Alloc(&a, 700);
	assert(x && y && !((uintptr_t)x % 8));
	memset(x, 0x59, 17);
	void* old = x;
	x = YMGUI_Arena_Realloc(&a, x, 1000);
	assert(x && x != old);
	for (int i = 0; i < 17; ++i)
		assert(((unsigned char*)x)[i] == 0x59);
	assert(!YMGUI_Arena_Realloc(&a, x, SIZE_MAX));
	assert(((unsigned char*)x)[0] == 0x59);
	YMGUI_Arena_Free(&a, y);
	YMGUI_Arena_Free(&a, x);
	assert(a.used == 0 && a.peak && YMGUI_Arena_LargestFree(&a) == largest);
	void* slots[96];
	unsigned n = 0;
	while (n < 96 && (slots[n] = YMGUI_Arena_Alloc(&a, 13 + n)))
		++n;
	for (unsigned i = 0; i < n; i += 2)
		YMGUI_Arena_Free(&a, slots[i]);
	for (unsigned i = 1; i < n; i += 2)
		YMGUI_Arena_Free(&a, slots[i]);
	assert(YMGUI_Arena_LargestFree(&a) == largest);
	assert(!YMGUI_Arena_Init(&a, storage, 4));
}
static GYbus bus;
static int isr, critical, fail_send;
static unsigned chunks;
static GYpx copied[21];
static uint32 enter(void* u)
{
	(void)u;
	assert(!critical);
	critical = 1;
	return 19;
}
static void leave(uint32 s, void* u)
{
	(void)u;
	assert(critical && s == 19);
	critical = 0;
}
static int in_isr(void* u)
{
	(void)u;
	return isr;
}
static int send(const GYrect* a, const GYpx* p, int first, void* u)
{
	(void)u;
	assert(bus.owner == GY_BUS_LCD);
	assert(!YMGUI_Bus_Try(&bus, GY_BUS_WORKER));
	assert(first == (chunks == 0));
	memcpy(copied + (a->y - 2) * 3, p, a->w * a->h * sizeof(GYpx));
	++chunks;
	return !fail_send;
}
static void bus_test(void)
{
	GYbus_io io = {NULL, enter, leave, in_isr};
	assert(YMGUI_Bus_Init(&bus, &io));
	isr = 1;
	assert(!YMGUI_Bus_Try(&bus, GY_BUS_GUI));
	isr = 0;
	assert(YMGUI_Bus_Try(&bus, GY_BUS_WORKER));
	assert(!YMGUI_Bus_Try(&bus, GY_BUS_GUI));
	assert(!YMGUI_Bus_Release(&bus, GY_BUS_GUI));
	assert(YMGUI_Bus_Release(&bus, GY_BUS_WORKER));
	assert(YMGUI_Bus_Try(&bus, GY_BUS_GUI));
	GYpx pixels[21], stage[6];
	for (int i = 0; i < 21; ++i)
		pixels[i] = GY_ColorToPx(0xff000000u + (uint32)i);
	GYrect a = {1, 2, 3, 7};
	assert(YMGUI_StagedFlush(&bus, &a, pixels, stage, 6, send, NULL));
	assert(chunks == 4 && !memcmp(pixels, copied, sizeof(pixels)) && bus.owner == GY_BUS_GUI);
	isr = 1;
	GYpx preserved = stage[0];
	assert(!YMGUI_StagedFlush(&bus, &a, pixels, stage, 6, send, NULL));
	assert(GY_PxEqual(stage[0], preserved));
	isr = 0;
	chunks = 0;
	fail_send = 1;
	assert(!YMGUI_StagedFlush(&bus, &a, pixels, stage, 6, send, NULL));
	assert(bus.owner == GY_BUS_GUI);
	assert(YMGUI_Bus_BeginLCD(&bus));
	isr = 1;
	assert(YMGUI_Bus_EndLCD(&bus));
	isr = 0;
	assert(YMGUI_Bus_Release(&bus, GY_BUS_GUI));
	assert(bus.deferred >= 6 && bus.violations == 1);
}
static uint8 data[128];
static int fail_read, closed;
static uint32 source(void* u, uint32 o, uint32 n, uint8* out)
{
	(void)u;
	if (fail_read)
		return 0;
	memcpy(out, data + o, n);
	return n;
}
static void* open_data(const char* path, size_t* n, void* u)
{
	(void)path;
	(void)u;
	*n = 128;
	return data;
}
static size_t read_data(void* h, size_t o, void* out, size_t n, void* u)
{
	(void)u;
	memcpy(out, (uint8*)h + o, n);
	return fail_read ? 0 : n;
}
static void close_data(void* h, void* u)
{
	(void)h;
	(void)u;
	++closed;
}
static void resource_test(void)
{
	for (int i = 0; i < 128; ++i)
		data[i] = i;
	GYresource_io io = {NULL, open_data, read_data, close_data};
	YMGUI_Resource_SetBackend(&io);
	GYresource r;
	uint8 out[16];
	assert(YMGUI_Resource_Open(&r, "font"));
	YMGUI_Resource_SetBackend(NULL);
	assert(YMGUI_Resource_Read(&r, 120, out, 16) == 0);
	assert(YMGUI_Resource_Read(&r, 112, out, 16) == 16 && out[0] == 112);
	YMGUI_Resource_Close(&r);
	YMGUI_Resource_Close(&r);
	assert(closed == 1);
	YMGUI_Resource_SetBackend(&io);
	size_t size = 0;
	void* loaded = YMGUI_Resource_Load("font", &size);
	assert(loaded && size == 128 && !memcmp(loaded, data, 128));
	GY_free1(loaded);
	fail_read = 1;
	loaded = YMGUI_Resource_Load("font", &size);
	assert(!loaded && size == 0 && closed == 3);
	fail_read = 0;
	YMGUI_Resource_SetBackend(NULL);
	GYfont font = {0};
	font.cell_h = 2;
	font.bytes_per_row = 4;
	font.glyph_count = 16;
	GYfontcache cache;
	assert(YMGUI_FontCache_Init(&cache, &font, source, NULL, 2, 2));
	assert(cache.font.glyph_read(&cache.font, 0, 8, out) == 8 && cache.reads == 1);
	assert(cache.font.glyph_read(&cache.font, 0, 8, out) == 8 && cache.hot_hits == 1);
	cache.hot_enabled = 0;
	assert(cache.font.glyph_read(&cache.font, 0, 8, out) == 8 && cache.small_hits == 1);
	cache.hot_enabled = 1;
	cache.clock = 0xffffffffu;
	assert(cache.font.glyph_read(&cache.font, 0, 8, out) == 8 && cache.clock == 2);
	fail_read = 1;
	memset(out, 0xff, 8);
	assert(!cache.font.glyph_read(&cache.font, 120, 8, out));
	for (int i = 0; i < 8; ++i)
		assert(!out[i]);
	assert(cache.failures == 1);
	fail_read = 0;
	YMGUI_FontCache_Clear(&cache);
	assert(cache.font.glyph_read(&cache.font, 0, 8, out) == 8 && cache.reads == 3);
	YMGUI_FontCache_Free(&cache);
	YMGUI_FontCache_Free(&cache);
	assert(YMGUI_FontCache_Init(&cache, &font, source, NULL, 0, 0));
	assert(cache.font.glyph_read(&cache.font, 0, 8, out) == 8);
	assert(cache.font.glyph_read(&cache.font, 0, 8, out) == 8 && cache.reads == 2);
	YMGUI_FontCache_Free(&cache);
}
static void scale_test(void)
{
	GYScaleAxis x[9];
	GY_ScaleAxisInit(x, 1, 9, 1);
	GYpx pixel = GY_ColorToPx(0xffa6c175);
	for (int i = 0; i < 9; ++i)
	{
		assert(x[i].first == 0 && x[i].second == 0);
		assert(GY_PxEqual(GY_ScaleBilinearPx(&pixel, &x[i], &x[i]), pixel));
	}
	uint32 nearest[7];
	GY_ScaleNearestInit(nearest, 19, 7, 3);
	const uint32 expected[7] = {3, 12, 18, 27, 36, 42, 51};
	assert(!memcmp(nearest, expected, sizeof(expected)));
}
int main(void)
{
	arena_test();
	bus_test();
	resource_test();
	scale_test();
	puts("test_port_support: ALL PASS");
	return 0;
}
