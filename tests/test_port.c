#ifdef NDEBUG
#undef NDEBUG
#endif
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Hal.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Geom.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define W 96
#define H 64
static GYpx output[W * H], reference[W * H], buffer[W * H + 2];
static unsigned flushed_pixels;
static void flush(GYdisp* d, const GYrect* a, const GYpx* pixels)
{
	flushed_pixels += (unsigned)a->w * a->h;
	for (int y = 0; y < a->h; ++y)
		memcpy(output + (a->y + y) * W + a->x, pixels + y * a->w, a->w * sizeof(GYpx));
	YMGUI_Disp_FlushReady(d);
}
static GYdisp display;
static GYCTX previous_scene;
static unsigned blended_pixels, expect_partial_saving;
static GYCTX scene(void)
{
	if (previous_scene)
		YMGUI_Free_CtxFree(previous_scene);
	memset(&display, 0, sizeof(display));
	display.hor_res = W;
	display.ver_res = H;
	display.buf1 = buffer + 1;
	display.buf_px_cnt = W * H;
	display.flush_cb = flush;
	previous_scene = YMGUI_Creat_Ctx_Creat(&display, W, H);
	return previous_scene;
}
static void translucent(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	GYrect hit;
	if (GY_Rect_Intersect(&hit, a, &s->clip) && GY_Rect_Intersect(&hit, &hit, &s->buf_area))
		blended_pixels += (unsigned)hit.w * hit.h;
	YMGUI_Draw_Fill(s, a, obj->bg_color, (GYopa)(uintptr_t)obj->user_data);
}
static void shadow(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	GYrect outside = {a->x - 6, a->y - 3, a->w + 12, a->h + 6};
	YMGUI_Draw_Fill(s, &outside, obj->bg_color, 190);
}
static void parent_splash(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)a;
	GYrect parent;
	YMGUI_Obj_GetAbsArea(obj->parent, &parent);
	YMGUI_Draw_Fill(s, &parent, 0xffeef1f6, 255);
	GYrect tile = {parent.x + parent.w / 4, parent.y + parent.h / 4, parent.w / 2, parent.h / 2};
	YMGUI_Draw_Fill(s, &tile, 0xff237abc, 173);
}
static GYOBJ box(GYOBJ parent, int x, int y, int w, int h, GYcolor color)
{
	GYOBJ obj = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	obj->bg_color = color;
	return obj;
}
static void compare(GYCTX ctx, int expected_skip, const char* name)
{
	uint32 candidates, skipped;
	unsigned baseline_pixels = 0;
	YMGUI_DrawStats_GetAndReset(&candidates, &skipped);
	for (int enabled = 0; enabled < 3; ++enabled)
	{
		blended_pixels = 0;
		memset(output, 0, sizeof(output));
		buffer[0] = buffer[W * H + 1] = GY_ColorToPx(0xffa55a31);
		YMGUI_OcclusionEnabled = enabled != 0;
		YMGUI_PartialOcclusionEnabled = enabled == 2;
		ctx->inv_cnt = 1;
		ctx->inv_areas[0] = (GYrect){0, 0, W, H};
		YMGUI_Refresh(ctx);
		assert(GY_PxEqual(buffer[0], GY_ColorToPx(0xffa55a31)) && GY_PxEqual(buffer[W * H + 1], GY_ColorToPx(0xffa55a31)));
		YMGUI_DrawStats_GetAndReset(&candidates, &skipped);
		if (!enabled)
		{
			memcpy(reference, output, sizeof(output));
			baseline_pixels = blended_pixels;
		}
		else
		{
			assert(memcmp(reference, output, sizeof(output)) == 0);
			if (expected_skip == 1 && YMGUI_OCCLUSION)
				assert(skipped > 0);
			if (expected_skip == 0)
				assert(skipped == 0);
			if (enabled == 2 && expect_partial_saving && YMGUI_OCCLUSION)
				assert(blended_pixels < baseline_pixels);
		}
	}
	(void)name;
}
static void testOcclusion(void)
{
	GYCTX c = scene();
	box(c->root, 0, 0, W / 2, H, 0xffa42165);
	box(c->root, W / 2, 0, W / 2, H, 0xff2966b5);
	compare(c, 1, "union of two opaque controls");
	GYOBJ overlay = box(c->top_layer, 0, 0, W, H, 0xff183659);
	overlay->draw_cb = translucent;
	overlay->user_data = (void*)127;
	compare(c, 1, "transparent overlay keeps painter order");
	c = scene();
	box(c->root, 0, 0, W / 2 - 1, H, 0xffa42165);
	box(c->root, W / 2, 0, W / 2, H, 0xff2966b5);
	compare(c, 0, "one-pixel uncovered gap");
	c = scene();
	GYOBJ lower = box(c->root, 20, 20, 32, 24, 0xff3179b2);
	lower->draw_cb = shadow;
	box(c->root, 20, 20, 32, 24, 0xfff8ab31);
	compare(c, 0, "unbounded custom callback shadow");
	c = scene();
	GYOBJ parent = box(c->root, 20, 20, 32, 24, 0xff3179b2);
	parent->draw_cb = NULL;
	parent->state |= GY_STATE_ClipChildren;
	parent->scroll_x = 3;
	parent->scroll_y = 7;
	box(parent, -30, -30, W, H, 0xfff8ab31);
	GYOBJ hidden = box(c->root, 0, 0, W, H, 0xff181818);
	hidden->state |= GY_STATE_Hidden;
	compare(c, 0, "clipped and scrolled opaque child plus hidden control");
	display.buf_px_cnt = W * 9;
	compare(c, -1, "banded refresh preserves clipping");
	c = scene();
	for (int i = 0; i < 200; ++i)
		box(c->root, 0, 0, W, H, 0xff112233 + i);
	compare(c, 0, "draw-plan capacity fallback");
	c = scene();
	for (int i = 0; i < 70; ++i)
		box(c->root, i, 0, 1, H, 0xff112233 + i);
	compare(c, 0, "opaque-rectangle capacity conservative fallback");
	c = scene();
	for (int i = 0; i < 60; ++i)
		box(c->root, (i % 10) * 9, (i / 10) * 10, 2, 2, 0xff112233 + i);
	compare(c, 0, "fragment capacity conservative fallback");
	c = scene();
	GYOBJ marked = box(c->top_layer, 0, 0, W, H, 0xff183659);
	marked->draw_cb = translucent;
	marked->user_data = (void*)255;
	marked->state |= GY_STATE_OpaqueRect;
	compare(c, 1, "explicit opaque custom callback");
	c = scene();
	GYOBJ tinted = box(c->root, 0, 0, W, H, 0xff615997);
	tinted->draw_cb = translucent;
	tinted->user_data = (void*)127;
	tinted->state |= GY_STATE_BoundedDraw;
	box(c->top_layer, 24, 16, 48, 32, 0xff12cc98);
	expect_partial_saving = 1;
	compare(c, 0, "four disjoint clips preserve translucent blending");
	display.buf_px_cnt = W * 9;
	compare(c, -1, "partial clips across bands");
	expect_partial_saving = 0;
	for (int pose = 1; pose <= 20; ++pose)
	{
		c = scene();
		GYOBJ window = box(c->root, 20 - pose, 14 - pose, W * pose / 20, H * pose / 20, 0xffeef1f6);
		window->state |= GY_STATE_ClipChildren;
		GYOBJ status = box(window, 3, 3, W - 6, 12, 0xff237abc);
		status->draw_cb = translucent;
		status->user_data = (void*)127;
		GYOBJ splash = box(window, 0, 0, W, H, 0xffeef1f6);
		splash->draw_cb = parent_splash;
		splash->state |= GY_STATE_OpaqueRect;
		compare(c, 1, "resizing parent clipped opaque splash");
		display.buf_px_cnt = W * 9;
		compare(c, -1, "clipped opaque splash across bands");
	}
	srand(710);
	for (int test = 0; test < 30; ++test)
	{
		c = scene();
		for (int i = 0; i < 14; ++i)
		{
			GYOBJ obj = box(i % 3 ? c->root : c->top_layer, rand() % 112 - 8, rand() % 80 - 8,
							1 + rand() % W, 1 + rand() % H, 0xff000000u | (unsigned)rand());
			if (i % 4 == 0)
			{
				obj->draw_cb = translucent;
				obj->user_data = (void*)(uintptr_t)(1 + rand() % 254);
				obj->state |= GY_STATE_BoundedDraw;
			}
			if (i % 7 == 0)
			{
				obj->draw_cb = shadow;
				obj->state &= ~GY_STATE_BoundedDraw;
			}
		}
		compare(c, -1, "deterministic mixed overlap scene");
	}
	YMGUI_Free_CtxFree(previous_scene);
	previous_scene = NULL;
	puts("Occlusion image equivalence and fallback checks: PASS");
}

#include "YMGUI_Font.h"
#include "YMGUI_DrawPx.h"
#include "YMGUI_DrawImg.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_EditView.h"
#include "YMGUI_Event.h"
#if YMGUI_ANIM
#include "YMGUI_Anim.h"
#endif

static unsigned glyph_reads;
static uint8 glyph_data[128];
static uint32 readGlyph(GYFONT f, uint32 off, uint32 len, uint8* out)
{
	(void)f;
	assert(off + len <= sizeof(glyph_data));
	++glyph_reads;
	memcpy(out, glyph_data + off, len);
	return len;
}
static void testRaster(void)
{
	GYpx pixels[257];
	GYsurface s = {pixels + 1, {10, 20, 16, 16}, {0, 0, 100, 100}, 16};
	GYrect full = {10, 20, 16, 16};
	GYcolor color = 0xff2595dd;
	for (unsigned a = 0; a < 256; ++a)
	{
		for (unsigned i = 0; i < 257; ++i)
			pixels[i] = GY_ColorToPx(0xff000000u | i * 7919u);
		GYpx before[257];
		memcpy(before, pixels, sizeof(pixels));
		YMGUI_Draw_Fill(&s, &full, color, (GYopa)a);
		assert(GY_PxEqual(pixels[0], before[0]));
		for (unsigned i = 1; i < 257; ++i)
		{
			GYpx expected = a == 0 ? before[i] : a == 255 ? GY_ColorToPx(color)
														  : GY_MixPx(before[i], color, (GYopa)a);
			assert(GY_PxEqual(pixels[i], expected));
		}
	}
	const uint16 cp[] = {0x4e00};
	GYfont font = {NULL, cp, 1, 0, 0, 16, 16, 8, 4, NULL, readGlyph};
	memset(glyph_data, 0x78, sizeof(glyph_data));
	glyph_reads = 0;
	YMGUI_GlyphClipEnabled = 1;
	assert(YMGUI_Draw_Glyph(&s, &font, 50, 20, 0x4e00, color) == 16);
	assert(!glyph_reads);
	assert(YMGUI_Draw_Glyph(&s, &font, 10, 20, 0x4e00, color) == 16);
	assert(glyph_reads == 1);
	/* Compare accelerated glyph blending with the original pixel path. */
	GYpx result[256];
	for (unsigned coverage = 0; coverage < 16; ++coverage)
	{
		memset(glyph_data, coverage | (coverage << 4), sizeof(glyph_data));
		for (int mode = 0; mode < 2; ++mode)
		{
			for (unsigned i = 1; i < 257; ++i)
				pixels[i] = GY_ColorToPx(0xff000000u | i * 7919u);
			YMGUI_GlyphBlendOptimizeEnabled = mode;
			YMGUI_Draw_Glyph(&s, &font, 10, 20, 0x4e00, color);
			if (!mode)
				memcpy(result, pixels + 1, sizeof(result));
			else
				assert(!memcmp(result, pixels + 1, sizeof(result)));
		}
	}
	YMGUI_GlyphBlendOptimizeEnabled = 1;
	/* Image copy and scaling must respect the buffer even when clip is wider. */
	GYpx image_data[4] = {GY_ColorToPx(0xff123456), GY_ColorToPx(0xfffedcba), GY_PX_ZERO, GY_ColorToPx(0xff78ab23)};
	GYimg image = {0};
	image.data = image_data;
	image.w = image.h = 2;
	GYpx guard = pixels[0];
	YMGUI_Draw_Img(&s, &image, 9, 19);
	assert(GY_PxEqual(pixels[1], image_data[3]) && GY_PxEqual(pixels[0], guard));
	GYrect dst = {8, 18, 20, 20};
	YMGUI_Draw_ImgScaled(&s, &image, dst);
	for (int y = 0; y < 16; ++y)
		for (int x = 0; x < 16; ++x)
		{
			int sx = ((x + 2) * 6553 + (6553 >> 1)) >> 16;
			int sy = ((y + 2) * 6553 + (6553 >> 1)) >> 16;
			assert(GY_PxEqual(pixels[1 + y * 16 + x], image_data[sy * 2 + sx]));
		}
	assert(GY_PxEqual(pixels[0], guard));
}
static unsigned clicked, touch_frames;
static void inputEvents(GYOBJ obj, GYEvent e)
{
	(void)obj;
	if (e == GY_EVENT_Clicked)
		++clicked;
}
static void touchFrame(const GYtouchpoint* p, uint8 count, void* user)
{
	(void)p;
	(void)user;
	assert(count <= GY_TOUCH_MAX);
	++touch_frames;
}
static void typeText(const char* text)
{
	while (*text)
		YMGUI_Inject_Key((uint8)*text++, 1);
}
static void testInput(void)
{
	GYCTX ctx = scene();
	GYOBJ target = box(ctx->root, 0, 0, W, H, 0xffabcdef);
	target->event_cb = inputEvents;
	YMGUI_Inject_SetCtx(ctx);
	YMGUI_Inject_SetTouchListener(touchFrame, NULL);
	GYtouchpoint p[5] = {{10, 10, 1}, {20, 20, 2}, {30, 30, 3}, {40, 40, 4}, {50, 50, 5}};
	YMGUI_Inject_Touch(p, 1);
	assert(ctx->pressed_obj == target);
	YMGUI_Inject_Touch(NULL, 0);
	assert(clicked == 1 && !ctx->pressed_obj);
	YMGUI_Inject_Touch(p, 1);
	YMGUI_Inject_Touch(p, 2);
	assert(!ctx->pressed_obj);
	YMGUI_Inject_Touch(p + 1, 1);
	YMGUI_Inject_Touch(NULL, 0);
	assert(clicked == 1);
	YMGUI_Inject_Touch(p, 1);
	YMGUI_Inject_Touch(p + 1, 1);
	YMGUI_Inject_Touch(NULL, 0);
	assert(clicked == 1);
	YMGUI_Inject_Touch(p, 5);
	YMGUI_Inject_Touch(NULL, 0);
	GYtouchpoint duplicate[2] = {p[0], p[0]};
	YMGUI_Inject_Touch(p, 1);
	YMGUI_Inject_Touch(duplicate, 2);
	YMGUI_Inject_Touch(NULL, 0);
	assert(clicked == 1 && !ctx->pressed_obj);
	YMGUI_Inject_Touch(NULL, 1);
	YMGUI_Inject_Touch(p, 6);
	YMGUI_Inject_Touch(NULL, 0);
	assert(touch_frames > 0);
	YMGUI_Inject_SetTouchListener(NULL, NULL);
	YMGUI_Free_ObjFree(target);
	for (int multiline = 0; multiline < 2; ++multiline)
	{
		GYOBJ editor = multiline ? YMGUI_Creat_EditView_Creat(ctx->root, 0, 0, W, H, 128)
								 : YMGUI_Creat_TextInput_Creat(ctx->root, 0, 0, W, H, 128);
		for (int round = 0; round < 100; ++round)
		{
			if (multiline)
				YMGUI_EditView_SetText(editor, "你");
			else
				YMGUI_TextInput_SetText(editor, "你");
			YMGUI_SetFocus(ctx, editor);
			editor->state |= GY_STATE_Editing;
			/* Move one character away and back while held: creates a collapsed anchor. */
			YMGUI_Inject_Key(GY_KEY_SHIFT_LEFT, 1);
			YMGUI_Inject_Key(GY_KEY_SHIFT_RIGHT, 1);
			typeText("的和");
			const char* text = multiline ? YMGUI_EditView_GetText(editor) : YMGUI_TextInput_GetText(editor);
			assert(!strcmp(text, "你的和"));
			YMGUI_Inject_Key(GY_KEY_SHIFT_LEFT, 1);
			YMGUI_Inject_Key(GY_KEY_SHIFT_RIGHT, 1);
			for (int j = 0; j < 12; ++j)
				YMGUI_Inject_Key(GY_KEY_BACKSPACE, 1);
			assert(!strcmp(text, ""));
		}
		YMGUI_Free_ObjFree(editor);
	}
	YMGUI_Clipboard_SetBackend(NULL, NULL);
	YMGUI_Clipboard_SetText("你好");
	assert(!strcmp(YMGUI_Clipboard_GetText(), "你好"));
	YMGUI_Clipboard_Clear();
	assert(!strcmp(YMGUI_Clipboard_GetText(), ""));
	YMGUI_Inject_SetCtx(NULL);
	YMGUI_Free_CtxFree(previous_scene);
	previous_scene = NULL;
}
#if YMGUI_ANIM
static unsigned completed;
static void animDone(GYANIM id, uint8 cancelled, void* user)
{
	(void)id;
	(void)user;
	assert(!cancelled);
	++completed;
}
#endif

static unsigned band_calls, band_pixels;
static void bandFlush(GYDISP d, const GYrect* a, const GYpx* pixels)
{
	(void)pixels;
	assert(a->w > 0 && a->h > 0);
	assert((uint32)a->w * a->h <= d->buf_px_cnt);
	++band_calls;
	band_pixels += (unsigned)a->w * a->h;
}
static void testDirtyAndBands(void)
{
	GYCTX c = scene();
	c->inv_cnt = 0;
	GYrect left = {0, 0, 5, 5}, right = {W - 5, 0, 5, 5}, off = {-5, -5, W + 10, 5};
	YMGUI_Ctx_InvalidateArea(c, &left);
	YMGUI_Ctx_InvalidateArea(c, &right);
	YMGUI_Ctx_InvalidateArea(c, &off);
	assert(c->inv_cnt == 2);
	YMGUI_Ctx_MergeDirtyAreas(c);
	assert(c->inv_cnt == 1);
	GYOBJ parent = box(c->root, 10, 10, 20, 20, 0xff123456);
	parent->state |= GY_STATE_ClipChildren;
	GYOBJ child = box(parent, -5, -5, 40, 40, 0xff234567);
	c->inv_cnt = 0;
	YMGUI_Obj_Invalidate(child);
	assert(c->inv_cnt == 1 && c->inv_areas[0].w == 20 && c->inv_areas[0].h == 20);
	parent->state |= GY_STATE_Hidden;
	c->inv_cnt = 0;
	YMGUI_Obj_Invalidate(child);
	assert(!c->inv_cnt);
	parent->state &= (uint8)~GY_STATE_Hidden;
#if YMGUI_ANIM
	GYOBJ animated = box(c->root, 0, 0, 5, 5, 0xff123456);
	GYANIM id = YMGUI_Anim_MoveTo(animated, 1, 0, 100, GY_ANIM_LINEAR);
	assert(id && YMGUI_Anim_SetDone(c, id, animDone, NULL));
	c->inv_cnt = 0;
	YMGUI_Anim_Tick(c, 1);
	assert(!c->inv_cnt);
	YMGUI_Anim_Tick(c, 99);
	assert(animated->area.x == 1 && c->inv_cnt && completed == 1 && !YMGUI_Anim_ActiveCount(c));
	/* Every incremental frame of an unbounded subtree must match a full repaint. */
	GYOBJ moving = box(c->root, 20, 20, 12, 12, 0xff615997);
	moving->draw_cb = shadow;
	box(moving, 8, 8, 20, 16, 0xff51ab23);
	YMGUI_Obj_Invalidate(c->root);
	YMGUI_Refresh(c);
	assert(YMGUI_Anim_MoveTo(moving, 45, 25, 20, GY_ANIM_LINEAR));
	assert(YMGUI_Anim_ResizeTo(moving, 18, 18, 20, GY_ANIM_LINEAR));
	assert(YMGUI_Anim_ScrollTo(moving, 3, 4, 20, GY_ANIM_LINEAR));
	for (int tick = 0; tick < 20; ++tick)
	{
		YMGUI_Anim_Tick(c, 1);
		YMGUI_Refresh(c);
		memcpy(reference, output, sizeof(reference));
		YMGUI_Obj_Invalidate(c->root);
		YMGUI_Refresh(c);
		assert(!memcmp(reference, output, sizeof(reference)));
	}
#endif
	YMGUI_Free_CtxFree(previous_scene);
	previous_scene = NULL;
	/* Large buffers previously narrowed cap/width to int16 before clamping. */
	GYdisp d = {0};
	d.hor_res = 320;
	d.ver_res = 480;
	d.buf_px_cnt = 320 * 480;
	d.buf1 = calloc(d.buf_px_cnt + 1, sizeof(GYpx));
	d.flush_cb = bandFlush;
	GYpx sentinel = GY_ColorToPx(0xffa55a31);
	d.buf1[d.buf_px_cnt] = sentinel;
	c = YMGUI_Creat_Ctx_Creat(&d, 320, 480);
	const int widths[] = {1, 3, 5, 320};
	for (unsigned i = 0; i < 4; ++i)
	{
		c->inv_cnt = 1;
		c->inv_areas[0] = (GYrect){0, 0, widths[i], 480};
		band_calls = band_pixels = 0;
		YMGUI_Refresh(c);
		assert(band_calls == 1 && band_pixels == (unsigned)widths[i] * 480);
		assert(GY_PxEqual(d.buf1[d.buf_px_cnt], sentinel));
	}
	YMGUI_Free_CtxFree(c);
	free(d.buf1);
}

#include "YMGUI_Image.h"
static void testBlackLookup(void)
{
#if YMGUI_COLOR_DEPTH == 16 || YMGUI_COLOR_DEPTH == 24
    const unsigned n = 65536;
    GYpx* source = malloc(n * sizeof(GYpx));
    GYpx* pixels = malloc((n + 2) * sizeof(GYpx));
    assert(source && pixels);
    for (unsigned i=0;i<n;++i) {
#if YMGUI_COLOR_DEPTH == 16
        source[i]=(GYpx)i;
#else
        source[i]=(GYpx){i%256,(i*97+11)%256,(i*53+137)%256};
#endif
    }
    GYpx guard=GY_ColorToPx(0xffa55a31);
    GYsurface surface={pixels+1,{0,0,512,128},{0,0,512,128},512};
    for(unsigned opacity=0;opacity<256;++opacity) {
        memcpy(pixels+1,source,n*sizeof(GYpx)); pixels[0]=pixels[n+1]=guard;
        YMGUI_Draw_Fill(&surface,&surface.clip,GY_ARGB(37,0,0,0),(GYopa)opacity);
        for(unsigned i=0;i<n;++i)
            assert(GY_PxEqual(pixels[i+1],GY_MixPx(source[i],GY_ARGB(255,0,0,0),(GYopa)opacity)));
        assert(GY_PxEqual(pixels[0],guard) && GY_PxEqual(pixels[n+1],guard));
    }
    /* Odd stride/offset; clipped edges; change opacity without stale tables. */
    surface=(GYsurface){pixels+1,{10,20,17,40},{12,23,14,29},19};
    for(unsigned opacity=1;opacity<255;opacity+=17) {
        memcpy(pixels+1,source,n*sizeof(GYpx));
        YMGUI_Draw_Fill(&surface,&surface.buf_area,GY_ARGB(255,0,0,0),(GYopa)opacity);
        for(unsigned i=0;i<n;++i) {
            unsigned y=i/19,x=i%19;
            GYpx expected=(x>=2 && x<16 && y>=3 && y<32) ?
                GY_MixPx(source[i],GY_ARGB(255,0,0,0),(GYopa)opacity):source[i];
            assert(GY_PxEqual(pixels[i+1],expected));
        }
    }
    free(source);free(pixels);
#endif
}
static void testImageContent(void)
{
    GYCTX ctx=scene();
    GYpx* pixels=malloc(64*32*sizeof(GYpx)); assert(pixels);
    for(unsigned i=0;i<64*32;++i) pixels[i]=GY_ColorToPx(0xff113355u+i*6739);
    GYimg source={pixels,64,32,0,GY_PX_ZERO};
    GYOBJ image=YMGUI_Creat_Image_Creat(ctx->root,16,4,64,56);
    YMGUI_Image_SetSrc(image,&source);YMGUI_Image_SetScaleMode(image,GY_IMG_FIT);
    YMGUI_Obj_Invalidate(ctx->root);YMGUI_Refresh(ctx);
    for(unsigned mode=0;mode<3;++mode) {
        YMGUI_Image_SetScaleMode(image,(GYimg_scale_mode)mode);YMGUI_Refresh(ctx);
        for(unsigned i=0;i<64*32;++i) pixels[i]=GY_ColorToPx(0xff337755u+i*3769+mode*117);
        flushed_pixels=0;YMGUI_Image_InvalidateContent(image);YMGUI_Refresh(ctx);
        unsigned reduced=flushed_pixels;memcpy(reference,output,sizeof(output));
        flushed_pixels=0;YMGUI_Obj_Invalidate(image);YMGUI_Refresh(ctx);
        assert(!memcmp(reference,output,sizeof(output)));
        assert(reduced==(mode==GY_IMG_FILL ? 64u*56 : 64u*32));
        assert(flushed_pixels==64u*56);
    }
    YMGUI_Obj_SetHidden(image,1);YMGUI_Refresh(ctx);
    YMGUI_Image_InvalidateContent(image);assert(!ctx->inv_cnt);
    YMGUI_Free_CtxFree(previous_scene);previous_scene=NULL;free(pixels);
}
int main(void)
{
	testOcclusion();
	testRaster();
	testBlackLookup();
	testImageContent();
	testInput();
	testDirtyAndBands();
	puts("test_port: ALL PASS");
	return 0;
}
