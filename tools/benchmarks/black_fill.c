#define _POSIX_C_SOURCE 200809L
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawPx.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec * 1e-9;
}
static int cmp(const void* a, const void* b)
{
	double x = *(const double*)a, y = *(const double*)b;
	return (x > y) - (x < y);
}
int main(void)
{
	const unsigned n = 65536;
	GYpx *src = malloc(n * sizeof(GYpx)), *pixels = malloc(n * sizeof(GYpx));
	assert(src && pixels);
	for (unsigned i = 0; i < n; ++i)
	{
#if YMGUI_COLOR_DEPTH == 16
		src[i] = i;
#else
		src[i] = (GYpx){i % 256, (i * 97 + 11) % 256, (i * 53 + 137) % 256};
#endif
	}
	GYsurface s = {pixels, {0, 0, 512, 128}, {0, 0, 512, 128}, 512};
	for (unsigned a = 0; a < 256; ++a)
	{
		memcpy(pixels, src, n * sizeof(GYpx));
		YMGUI_Draw_Fill(&s, &s.clip, GY_ARGB(255, 0, 0, 0), a);
		for (unsigned i = 0; i < n; ++i)
			assert(GY_PxEqual(pixels[i], GY_MixPx(src[i], GY_ARGB(255, 0, 0, 0), a)));
	}
	puts("PASS: every opacity; all RGB565 pixels / RGB888 component values");
	const unsigned sizes[][2] = {{320, 240}, {320, 48}, {16, 16}, {8, 8}};
	for (unsigned pattern = 0; pattern < 2; ++pattern)
		for (unsigned k = 0; k < 4; ++k)
		{
			s.stride = s.clip.w = s.buf_area.w = sizes[k][0];
			s.clip.h = s.buf_area.h = sizes[k][1];
			unsigned count = s.stride * s.clip.h;
			pixels = realloc(pixels, count * sizeof(GYpx));
			src = realloc(src, count * sizeof(GYpx));
			s.buf = pixels;
			for (unsigned i = 0; i < count; ++i)
				src[i] = GY_ColorToPx(pattern ? 0xffabcdefu : (0xff000000u | (i * 6739u & 0xffffffu)));
			double samples[2][301];
			for (int run = 0; run < 301; ++run)
				for (int mode = run % 2, j = 0; j < 2; ++j, mode = 1 - mode)
				{
					memcpy(pixels, src, count * sizeof(GYpx));
					YMGUI_BlackLutEnabled = mode;
					double t = now();
					YMGUI_Draw_Fill(&s, &s.clip, GY_ARGB(255, 0, 0, 0), 165);
					samples[mode][run] = (now() - t) * 1e6;
				}
			qsort(samples[0], 301, sizeof(double), cmp);
			qsort(samples[1], 301, sizeof(double), cmp);
			printf("%s %ux%u scalar %.3f us lookup %.3f us speedup %.2fx\n", pattern ? "uniform" : "varied", sizes[k][0], sizes[k][1], samples[0][150], samples[1][150], samples[0][150] / samples[1][150]);
		}
	free(src);
	free(pixels);
}
