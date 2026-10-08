#ifndef YMGUI_SCALE_BILINEAR_H
#define YMGUI_SCALE_BILINEAR_H
#include "YMGUI_DrawPx.h"
/* Caller-owned axis tables: share for equal dimensions and source stride.
 * Center sampling, clamped edges, 8-bit fractions; no allocation or float.
 * Dimensions must be positive; source stride must cover its width. */
typedef struct
{
	uint32 first, second, fraction;
} GYScaleAxis;
/* Nearest center sample. The caller can reuse offsets across all images. */
static inline uint32 GY_ScaleNearestAt(int position, int source, int target, int step)
{
	return (uint32)(((long long)position * 2 + 1) * source / ((long long)target * 2)) * step;
}
static inline void GY_ScaleNearestInit(uint32* offsets, int source, int target, int step)
{
	for (int i = 0; i < target; ++i)
		offsets[i] = GY_ScaleNearestAt(i, source, target, step);
}
static inline GYScaleAxis GY_ScaleAxisAt(int position, int source, int target, int step)
{
	long long fixed = ((long long)position * 256 + 128) * source / target - 128;
	GYScaleAxis a;
	if (fixed < 0)
		fixed = 0;
	if (fixed > (long long)(source - 1) * 256)
		fixed = (long long)(source - 1) * 256;
	a.first = (uint32)(fixed >> 8) * step;
	a.second = (uint32)(((fixed >> 8) + 1 < source) ? (fixed >> 8) + 1 : fixed >> 8) * step;
	a.fraction = (uint32)fixed & 255;
	return a;
}
static inline void GY_ScaleAxisInit(GYScaleAxis* axis, int source, int target, int step)
{
	for (int i = 0; i < target; ++i)
		axis[i] = GY_ScaleAxisAt(i, source, target, step);
}
static inline GYpx GY_ScaleBilinearPx(const GYpx* source, const GYScaleAxis* x, const GYScaleAxis* y)
{
	GYcolor a = GY_PxToColor(source[y->first + x->first]);
	GYcolor b = GY_PxToColor(source[y->first + x->second]);
	GYcolor c = GY_PxToColor(source[y->second + x->first]);
	GYcolor d = GY_PxToColor(source[y->second + x->second]);
	uint32 fx = x->fraction, fy = y->fraction, ix = 256 - fx, iy = 256 - fy;
	uint32 r = ((GY_COLOR_R(a) * ix + GY_COLOR_R(b) * fx) * iy + (GY_COLOR_R(c) * ix + GY_COLOR_R(d) * fx) * fy) >> 16;
	uint32 g = ((GY_COLOR_G(a) * ix + GY_COLOR_G(b) * fx) * iy + (GY_COLOR_G(c) * ix + GY_COLOR_G(d) * fx) * fy) >> 16;
	uint32 blue = ((GY_COLOR_B(a) * ix + GY_COLOR_B(b) * fx) * iy + (GY_COLOR_B(c) * ix + GY_COLOR_B(d) * fx) * fy) >> 16;
	return GY_ColorToPx(GY_ARGB(255, r, g, blue));
}
#endif
