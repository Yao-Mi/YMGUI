#define _POSIX_C_SOURCE 200809L
#include "YMGUI_Image.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Invalidate.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static GYpx frame[320 * 480], reference[320 * 480];
static unsigned flushed;
static void flush(GYdisp* d, const GYrect* a, const GYpx* p)
{
	for (int y = 0; y < a->h; ++y)
		memcpy(frame + (a->y + y) * 320 + a->x, p + y * a->w, a->w * sizeof(GYpx));
	flushed += a->w * a->h;
	YMGUI_Disp_FlushReady(d);
}
static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec * 1e-9;
}
int main(void)
{
	GYpx* source = malloc(320 * 240 * sizeof(GYpx));
	GYpx* band = malloc(320 * 48 * sizeof(GYpx));
	assert(source && band);
	GYdisp d = {0};
	d.hor_res = 320;
	d.ver_res = 480;
	d.buf1 = band;
	d.buf_px_cnt = 320 * 48;
	d.flush_cb = flush;
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&d, 320, 480);
	GYOBJ image = YMGUI_Creat_Image_Creat(ctx->root, 0, 0, 320, 480);
	GYimg img = {source, 320, 240, 0, GY_PX_ZERO};
	YMGUI_Image_SetSrc(image, &img);
	YMGUI_Image_SetScaleMode(image, GY_IMG_FIT);
	for (int i = 0; i < 320 * 240; ++i)
		source[i] = GY_ColorToPx(0xff000000u + i * 6739u);
	YMGUI_Refresh(ctx);
	double totals[2] = {0};
	unsigned traffic[2] = {0};
	for (int sample = 0; sample < 301; ++sample)
	{
		for (unsigned i = 0; i < 320 * 240; ++i)
			source[i] = GY_ColorToPx(0xff000000u + i * 6739u + sample * 371u);
		for (int mode = sample % 2, j = 0; j < 2; ++j, mode = 1 - mode)
		{
			flushed = 0;
			double t = now();
			if (mode)
				YMGUI_Image_InvalidateContent(image);
			else
				YMGUI_Obj_Invalidate(image);
			YMGUI_Refresh(ctx);
			totals[mode] += now() - t;
			traffic[mode] = flushed;
			if (j == 0)
				memcpy(reference, frame, sizeof(frame));
			else
				assert(!memcmp(reference, frame, sizeof(frame)));
		}
	}
	for (int mode = 0; mode < 2; ++mode)
		printf("%s %u pixels/frame; mean refresh %.3f us\n", mode ? "content" : "whole", traffic[mode], totals[mode] * 1e6 / 301);
	YMGUI_Free_CtxFree(ctx);
	free(source);
	free(band);
	puts("PASS: 301 updated frames exactly equal across whole/content refresh");
}
