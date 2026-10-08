#define _POSIX_C_SOURCE 200809L
#include "YMGUI_DrawPx.h"
#include "YMGUI_StagedFlush.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static GYpx *src, *dst;
static int count, accept;
static size_t staged;
static GYbus bus;
static int error;
static int access(const GYrect* a, const GYpx* p, void* u)
{
	(void)a;
	(void)p;
	(void)u;
	assert(bus.owner == GY_BUS_GUI);
	return accept;
}
static int send(const GYrect* a, const GYpx* p, int first, void* u)
{
	(void)u;
	assert(bus.owner == GY_BUS_LCD);
	assert(first == (count == 0));
	if (p == src)
	{
	}
	else
		staged += a->w * a->h * sizeof(GYpx);
	memcpy(dst + a->y * 320, p, a->w * a->h * sizeof(GYpx));
	++count;
	return !error;
}
static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec * 1e-9;
}
int main(void)
{
	src = malloc(320 * 240 * sizeof(GYpx));
	dst = malloc(320 * 240 * sizeof(GYpx));
	GYpx* stage = malloc(320 * 48 * sizeof(GYpx));
	assert(src && dst && stage);
	for (int i = 0; i < 320 * 240; ++i)
		src[i] = GY_ColorToPx(0xff000000u + (unsigned)i * 6739);
	GYrect a = {0, 0, 320, 240};
	assert(YMGUI_Bus_Init(&bus, NULL));
	assert(YMGUI_Bus_Try(&bus, GY_BUS_GUI));
	for (int mode = 0; mode < 2; ++mode)
	{
		double total = 0;
		int calls = 0;
		size_t bytes = 0;
		accept = mode;
		for (int i = 0; i < 301; ++i)
		{
			count = 0;
			staged = 0;
			double t = now();
			assert(YMGUI_StagedFlushAuto(&bus, &a, src, stage, 320 * 48, access, send, NULL));
			total += now() - t;
			calls = count;
			bytes = staged;
			assert(!memcmp(src, dst, 320 * 240 * sizeof(GYpx)));
			assert(bus.owner == GY_BUS_GUI);
		}
		printf("%s %d sends; %zu staging bytes/frame; mean prepare+simulated send %.3f us\n", mode ? "direct" : "staged", calls, bytes, total * 1e6 / 301);
	}
	error = 1;
	count = 0;
	assert(!YMGUI_StagedFlushAuto(&bus, &a, src, NULL, 0, access, send, NULL));
	assert(bus.owner == GY_BUS_GUI);
	free(src);
	free(dst);
	free(stage);
	puts("PASS: identical sent pixels and synchronous error ownership");
}
