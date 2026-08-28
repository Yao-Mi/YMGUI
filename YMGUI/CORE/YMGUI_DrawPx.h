#ifndef YMGUI_DRAWPX_H
#define YMGUI_DRAWPX_H

#include "YMGUI_PubType.h"
#include "YMGUI_Surface.h"
#include "YMGUI_PubDefine.h"

//===========================================================================
// 内部共享:单像素写入(裁剪到 surface.clip + band 偏移 + 可选混合)
//   line/circle/arc 等图元都用它,保证裁剪逻辑一处实现
//===========================================================================

//---------------------------------------------------------------------------
// 纯像素混合:把 src 色以覆盖度 opa 混到已有 dst 像素上,返回结果 GYpx
//   RGB888/RGB565 解包到 8bit 分量线性混合再打包;灰度/1bpp 转亮度混合
//   framebuffer 无 alpha,alpha 只在这一瞬间存在(见 PubType GYopa 说明)
//   抗锯齿(coverage 当 opa)与半透明填充共用此内核
//---------------------------------------------------------------------------
static inline GYpx GY_MixPx(GYpx dst, GYcolor src, GYopa opa)
{
#if YMGUI_COLOR_DEPTH == 24
	uint32 inv = 255u - opa;
	GYpx out;
	out.r = (uint8)((GY_COLOR_R(src) * opa + (uint32)dst.r * inv) / 255u);
	out.g = (uint8)((GY_COLOR_G(src) * opa + (uint32)dst.g * inv) / 255u);
	out.b = (uint8)((GY_COLOR_B(src) * opa + (uint32)dst.b * inv) / 255u);
	return out;
#elif YMGUI_COLOR_DEPTH == 16
	uint32 dr = (((uint32)dst >> 11) & 0x1F) << 3;
	uint32 dg = (((uint32)dst >> 5) & 0x3F) << 2;
	uint32 db = ((uint32)dst & 0x1F) << 3;
	uint32 sr = GY_COLOR_R(src);
	uint32 sg = GY_COLOR_G(src);
	uint32 sb = GY_COLOR_B(src);
	uint32 inv = 255u - opa;
	uint32 rr = (sr * opa + dr * inv) / 255u;
	uint32 rg = (sg * opa + dg * inv) / 255u;
	uint32 rb = (sb * opa + db * inv) / 255u;
	return (GYpx)(((rr & 0xF8) << 8) | ((rg & 0xFC) << 3) | (rb >> 3));
#else
	uint32 dgray = (uint32)dst;
	uint32 sgray = (GY_COLOR_R(src) * 77 + GY_COLOR_G(src) * 150 + GY_COLOR_B(src) * 29) >> 8;
	uint32 inv = 255u - opa;
	return (GYpx)((sgray * opa + dgray * inv) / 255u);
#endif
}

//写一个屏幕坐标像素(不透明直写)。越界/裁剪外自动忽略
static inline void GY_PutPx(GYSURFACE s, GYcoord x, GYcoord y, GYpx px)
{
	//裁剪到 surface.clip(屏幕坐标)
	if (x < s->clip.x || x >= s->clip.x + s->clip.w)
		return;
	if (y < s->clip.y || y >= s->clip.y + s->clip.h)
		return;
	//屏幕坐标 → band buffer 偏移
	GYcoord bx = x - s->buf_area.x;
	GYcoord by = y - s->buf_area.y;
	//防御:再钳到 buf_area 内(clip 若不是 buf_area 子集则挡住越界写)
	if (bx < 0 || bx >= s->buf_area.w || by < 0 || by >= s->buf_area.h)
		return;
	((GYpx*)s->buf)[(int32)by * s->stride + bx] = px;
}

//混合一个屏幕坐标像素(读背景-混合-写回)。opa 为覆盖度:0 跳过、255 直写、其余混合。
//裁剪/band 偏移逻辑与 GY_PutPx 一致;抗锯齿图元逐点走此。越界/裁剪外自动忽略。
static inline void GY_BlendPx(GYSURFACE s, GYcoord x, GYcoord y, GYcolor color, GYopa opa)
{
	if (opa == GY_OPA_TRANSP)
		return;
	//裁剪到 surface.clip(屏幕坐标)
	if (x < s->clip.x || x >= s->clip.x + s->clip.w)
		return;
	if (y < s->clip.y || y >= s->clip.y + s->clip.h)
		return;
	GYcoord bx = x - s->buf_area.x;
	GYcoord by = y - s->buf_area.y;
	if (bx < 0 || bx >= s->buf_area.w || by < 0 || by >= s->buf_area.h)
		return;
	GYpx* p = &((GYpx*)s->buf)[(int32)by * s->stride + bx];
	if (opa == GY_OPA_COVER)
		*p = GY_ColorToPx(color);      //满覆盖直写,省一次读+混合
	else
		*p = GY_MixPx(*p, color, opa); //读-混-写回
}

#endif // !YMGUI_DRAWPX_H
