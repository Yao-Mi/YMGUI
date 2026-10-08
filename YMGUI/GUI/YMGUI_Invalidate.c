#include "YMGUI_Invalidate.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"

volatile uint8 YMGUI_OcclusionEnabled = 1;
volatile uint8 YMGUI_PartialOcclusionEnabled = 1;
volatile uint8 YMGUI_DirtyClipEnabled = 1;
static uint32 draw_candidates, draw_skipped;
void YMGUI_DrawStats_GetAndReset(uint32* candidates, uint32* skipped)
{
	if (candidates)
		*candidates = draw_candidates;
	if (skipped)
		*skipped = draw_skipped;
	draw_candidates = draw_skipped = 0;
}

/**
 ***************************************************************************************************************************
 *	@FileName:    YMGUI_Invalidate.c
 *	@Author:      yaomimaoren
 *	@Date:        2026-07-01
 *	@Description: 失效/脏矩形系统 + Refresh。与 band 刷新管线咬合:只重绘脏区,逐 band flush
 *	@Version:     1.0
 *
 ***************************************************************************************************************************
 *
 * 备注信息：
 * 1.脏区当前用单个合并包围盒(简单起见);后续可换成多矩形列表减少重绘面积
 * 2.空闲无脏区时 Refresh 直接返回 —— 保留模式的省电关键
 *
 ***************************************************************************************************************************
 * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
 ***************************************************************************************************************************/

/**
 * @brief 把一块屏幕区域并入 ctx 脏矩形(取包围盒并集)
 */
/**
 * @brief 两矩形是否相交或相邻(用于决定是否合并;相邻也合并以减少碎片)
 */
static int rectTouch(const GYrect* a, const GYrect* b)
{
	// 在任一轴上完全分离(留 0 间隙即相邻)则不接触
	if (a->x > b->x + b->w || b->x > a->x + a->w)
		return 0;
	if (a->y > b->y + b->h || b->y > a->y + a->h)
		return 0;
	return 1;
}

/**
 * @brief 取两矩形的包围盒并集,写回 dst
 */
static void rectUnion(GYRECT dst, const GYrect* a, const GYrect* b)
{
	GYcoord x1 = GYMin(a->x, b->x);
	GYcoord y1 = GYMin(a->y, b->y);
	GYcoord x2 = GYMax(a->x + a->w, b->x + b->w);
	GYcoord y2 = GYMax(a->y + a->h, b->y + b->h);
	dst->x = x1;
	dst->y = y1;
	dst->w = x2 - x1;
	dst->h = y2 - y1;
}

/**
 * @brief 把整个脏区列表塌缩成单个包围盒(列表满时的退化路径)
 */
static void collapseAll(GYCTX ctx, const GYrect* extra)
{
	GYrect box = ctx->inv_areas[0];
	for (uint8 i = 1; i < ctx->inv_cnt; i++)
		rectUnion(&box, &box, &ctx->inv_areas[i]);
	if (extra != NULL)
		rectUnion(&box, &box, extra);
	ctx->inv_areas[0] = box;
	ctx->inv_cnt = 1;
}

/* Optional transition policy; normal invalidation stays local. */
void YMGUI_Ctx_MergeDirtyAreas(GYCTX ctx)
{
	if (ctx && !ctx->destroying && ctx->inv_cnt > 1)
		collapseAll(ctx, NULL);
}

/**
 * @brief 把一块屏幕区域并入 ctx 脏矩形列表,维护"互不重叠"不变式
 *        新区若与已有块接触则反复合并成一块(连通分量),否则新占一槽;
 *        槽满则塌缩为单包围盒
 */
void YMGUI_Ctx_InvalidateArea(GYCTX ctx, const GYrect* area)
{
	gy_assert(ctx && area);
	gy_log_explain((ctx == NULL) || (area == NULL), GY_LOG_PtrI, "上下文或区域不存在");
	if (ctx == NULL || area == NULL || ctx->destroying)
		return;
	if (area->w <= 0 || area->h <= 0)
		return;

	GYrect merged = *area;
	/* Clip before coalescing: offscreen rectangles must not bridge two
	   unrelated visible dirty regions into a large bounding box. */
	if (YMGUI_DirtyClipEnabled && ctx->disp)
	{
		GYDISP disp = (GYDISP)ctx->disp;
		GYrect screen = {0, 0, disp->hor_res, disp->ver_res};
		if (!GY_Rect_Intersect(&merged, area, &screen))
			return;
	}
	// 反复扫描:凡与 merged 接触的已有块都并进来并从列表移除,直到没有新的接触
	int changed = 1;
	while (changed)
	{
		changed = 0;
		for (uint8 i = 0; i < ctx->inv_cnt;)
		{
			if (rectTouch(&merged, &ctx->inv_areas[i]))
			{
				rectUnion(&merged, &merged, &ctx->inv_areas[i]);
				// 移除第 i 块(用末尾块填补)
				ctx->inv_areas[i] = ctx->inv_areas[ctx->inv_cnt - 1];
				ctx->inv_cnt--;
				changed = 1;
			}
			else
			{
				i++;
			}
		}
	}

	// merged 现在与列表里所有块都不接触,占一个新槽
	if (ctx->inv_cnt < GY_INV_MAX)
	{
		ctx->inv_areas[ctx->inv_cnt] = merged;
		ctx->inv_cnt++;
	}
	else
	{
		// 列表满:塌缩为单包围盒(退化,仍正确只是多刷些)
		collapseAll(ctx, &merged);
	}
}

/**
 * @brief 标记对象需要重绘(算屏幕绝对区域并入脏矩形)
 */
void YMGUI_Obj_Invalidate(GYOBJ obj)
{
	GYrect abs;
	if (obj == NULL || obj->ctx == NULL || obj->ctx->destroying)
		return;
	if (obj->state & GY_STATE_Hidden)
		return;
	YMGUI_Obj_GetAbsArea(obj, &abs);
	YMGUI_Obj_InvalidateVisibleArea(obj, &abs);
}

void YMGUI_Obj_InvalidateVisibleArea(GYOBJ obj, const GYrect* area)
{
	if (!obj || !obj->ctx || !area)
		return;
	GYrect visible = *area;
	if (YMGUI_DirtyClipEnabled)
	{
		for (GYOBJ p = obj; p; p = p->parent)
		{
			if (p->state & GY_STATE_Hidden)
				return;
			if (p != obj && (p->state & GY_STATE_ClipChildren))
			{
				GYrect bounds;
				YMGUI_Obj_GetAbsArea(p, &bounds);
				if (!GY_Rect_Intersect(&visible, &visible, &bounds))
					return;
			}
		}
	}
	YMGUI_Ctx_InvalidateArea(obj->ctx, &visible);
}

void YMGUI_Obj_InvalidatePaintScope(GYOBJ obj)
{
	if (!obj || !obj->ctx)
		return;
	if (!YMGUI_DirtyClipEnabled)
	{
		YMGUI_Obj_Invalidate(obj->ctx->root);
		return;
	}
	/* Custom callbacks and descendants can extend beyond obj->area.
	   Only an ancestor's ClipChildren guarantees their combined bounds. */
	GYOBJ scope = NULL;
	for (GYOBJ p = obj; p; p = p->parent)
	{
		if (p->state & GY_STATE_Hidden)
			return;
		if (!scope && p != obj && (p->state & GY_STATE_ClipChildren))
			scope = p;
	}
	if (scope)
		YMGUI_Obj_Invalidate(scope);
	else
		YMGUI_Obj_Invalidate(obj->ctx->root);
}

/**
 * @brief 递归绘制:若对象绝对区域与本 band 裁剪区相交,则调其 draw_cb,再递归子对象
 */
static void drawObjRec(GYOBJ obj, GYSURFACE s)
{
	GYrect abs, hit;
	if (obj->state & GY_STATE_Hidden)
		return;
	YMGUI_Obj_GetAbsArea(obj, &abs);
	// 与当前 band 裁剪区相交才画(裁剪由 draw_cb 内的图元再次收窄)
	if (GY_Rect_Intersect(&hit, &abs, &s->clip))
	{
		if (obj->draw_cb != NULL)
		{
			++draw_candidates;
			obj->draw_cb(obj, s, &abs);
		}
	}
	// 子对象递归。若本对象开 ClipChildren,把 surface.clip 收窄到本对象区域,
	// 递归结束后恢复(滚动容器:超出区域的子项被裁掉)
	GYrect saved_clip = s->clip;
	if (obj->state & GY_STATE_ClipChildren)
	{
		GYrect newclip;
		if (!GY_Rect_Intersect(&newclip, &abs, &s->clip))
			return; // 本对象完全在裁剪区外,子项也不可见
		s->clip = newclip;
	}
	GYOBJ c = obj->child_head;
	while (c != NULL)
	{
		drawObjRec(c, s);
		c = c->sibling;
	}
	s->clip = saved_clip; // 恢复
}

/* A bounded plan keeps the original painter order. Excess entries or rectangle
   fragments only disable an optimisation; they never omit necessary drawing. */
#if YMGUI_OCCLUSION
#define DRAW_PLAN_MAX 128
#define COVER_MAX 64
typedef struct
{
	GYOBJ obj;
	GYrect abs, clip;
	uint8 skip, clip_count;
} DrawEntry;
static struct
{
	DrawEntry entries[DRAW_PLAN_MAX];
	GYrect visible_clips[DRAW_PLAN_MAX][4];
	GYrect covers[COVER_MAX], fragments[2][COVER_MAX];
	uint16 count, cover_count;
	uint8 overflow;
} draw_plan;

static void collectDraw(GYOBJ obj, const GYrect* clip)
{
	GYrect abs, hit, child_clip = *clip;
	if (obj->state & GY_STATE_Hidden)
		return;
	YMGUI_Obj_GetAbsArea(obj, &abs);
	if (obj->draw_cb && GY_Rect_Intersect(&hit, &abs, clip))
	{
		if (draw_plan.count == DRAW_PLAN_MAX)
		{
			draw_plan.overflow = 1;
			return;
		}
		DrawEntry* e = &draw_plan.entries[draw_plan.count++];
		e->obj = obj;
		e->abs = abs;
		e->clip = *clip;
		e->skip = e->clip_count = 0;
	}
	if ((obj->state & GY_STATE_ClipChildren) && !GY_Rect_Intersect(&child_clip, &abs, clip))
		return;
	for (GYOBJ c = obj->child_head; c && !draw_plan.overflow; c = c->sibling)
		collectDraw(c, &child_clip);
}
static int fragmentAdd(GYrect* out, unsigned* count, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		return 1;
	if (*count == COVER_MAX)
		return 0;
	out[(*count)++] = (GYrect){(GYcoord)x, (GYcoord)y, (GYcoord)w, (GYcoord)h};
	return 1;
}
/* Return remaining rectangles, or -1 when conservative fallback is needed. */
static int uncovered(const GYrect* area, GYrect** remaining)
{
	unsigned count = 1, active = 0;
	draw_plan.fragments[active][0] = *area;
	for (unsigned c = 0; c < draw_plan.cover_count; ++c)
	{
		unsigned next_count = 0;
		GYrect* out = draw_plan.fragments[active ^ 1];
		for (unsigned i = 0; i < count; ++i)
		{
			GYrect r = draw_plan.fragments[active][i], hit;
			if (!GY_Rect_Intersect(&hit, &r, &draw_plan.covers[c]))
			{
				if (!fragmentAdd(out, &next_count, r.x, r.y, r.w, r.h))
					return -1;
				continue;
			}
			if (!fragmentAdd(out, &next_count, r.x, r.y, r.w, hit.y - r.y) ||
				!fragmentAdd(out, &next_count, r.x, hit.y + hit.h, r.w, r.y + r.h - hit.y - hit.h) ||
				!fragmentAdd(out, &next_count, r.x, hit.y, hit.x - r.x, hit.h) ||
				!fragmentAdd(out, &next_count, hit.x + hit.w, hit.y, r.x + r.w - hit.x - hit.w, hit.h))
				return -1;
		}
		count = next_count;
		active ^= 1;
		if (!count)
		{
			*remaining = draw_plan.fragments[active];
			return 0;
		}
	}
	*remaining = draw_plan.fragments[active];
	return (int)count;
}
#endif
static void drawScene(GYCTX ctx, GYSURFACE s)
{
#if YMGUI_OCCLUSION
	GYrect saved_clip = s->clip;
	if (!YMGUI_OcclusionEnabled)
		goto baseline;
	draw_plan.count = draw_plan.cover_count = draw_plan.overflow = 0;
	collectDraw(ctx->root, &s->clip);
	if (ctx->top_layer && !draw_plan.overflow)
		collectDraw(ctx->top_layer, &s->clip);
	if (draw_plan.overflow)
		goto baseline;
	/* Top to bottom: subtract the union of opaque visible rectangles. */
	for (int i = draw_plan.count - 1; i >= 0; --i)
	{
		DrawEntry* e = &draw_plan.entries[i];
		GYrect visible;
		GY_Rect_Intersect(&visible, &e->abs, &e->clip);
		/* Unknown custom callbacks may paint outside abs; consider their
		   entire clip until they explicitly promise bounded drawing. */
		const GYrect* footprint = YMGUI_Obj_DrawIsBounded(e->obj) ? &visible : &e->clip;
		GYrect* remaining;
		int count = uncovered(footprint, &remaining);
		e->skip = count == 0;
		/* Restrict splitting to callbacks with a declared bounded footprint.
		   At most four disjoint calls keep fragmentation overhead bounded. */
		if (YMGUI_PartialOcclusionEnabled && count > 0 && count <= 4 && YMGUI_Obj_DrawIsBounded(e->obj))
		{
			uint32 pixels = 0;
			for (int n = 0; n < count; ++n)
				pixels += (uint32)remaining[n].w * remaining[n].h;
			if (pixels < (uint32)footprint->w * footprint->h)
			{
				e->clip_count = (uint8)count;
				for (int n = 0; n < count; ++n)
					draw_plan.visible_clips[i][n] = remaining[n];
			}
		}
		if (!e->skip && YMGUI_Obj_DrawIsOpaqueRect(e->obj) && draw_plan.cover_count < COVER_MAX)
			draw_plan.covers[draw_plan.cover_count++] = visible;
	}
	/* Bottom to top: transparent blending sees the same background. */
	for (unsigned i = 0; i < draw_plan.count; ++i)
	{
		DrawEntry* e = &draw_plan.entries[i];
		++draw_candidates;
		if (e->skip)
		{
			++draw_skipped;
			continue;
		}
		if (e->clip_count)
		{
			for (unsigned n = 0; n < e->clip_count; ++n)
			{
				s->clip = draw_plan.visible_clips[i][n];
				e->obj->draw_cb(e->obj, s, &e->abs);
			}
		}
		else
		{
			s->clip = e->clip;
			e->obj->draw_cb(e->obj, s, &e->abs);
		}
	}
	s->clip = saved_clip;
	return;
baseline:
#endif
	drawObjRec(ctx->root, s);
	if (ctx->top_layer)
		drawObjRec(ctx->top_layer, s);
}

/**
 * @brief 自旋等到上一次异步 flush 完成(flush_busy 清零)。有 wait_cb 就调它(裸机可 __WFI 省电),
 *        否则忙等。同步 flush_cb 在返回前已调 FlushReady,故此处立即通过。
 */
static void waitFlushIdle(GYDISP disp)
{
	while (disp->flush_busy)
	{
		if (disp->wait_cb != NULL)
			disp->wait_cb(disp);
	}
}

/**
 * @brief 刷新单个脏矩形:裁到屏幕 → 按 buf 容量切 band → 逐 band 重绘 → flush
 *        pcur 指向"下一条 band 该用哪块 buffer"的游标(双缓冲在 buf1/buf2 间 ping-pong;
 *        单缓冲恒为 buf1)。跨脏矩形沿用同一游标,让重叠不在矩形边界断开。
 */
static void refreshOneRect(GYCTX ctx, GYDISP disp, const GYrect* rect, GYpx** pcur)
{
	// 脏区裁到屏幕范围
	GYrect screen = {0, 0, disp->hor_res, disp->ver_res};
	GYrect dirty;
	if (!GY_Rect_Intersect(&dirty, rect, &screen))
		return;

	int dbl = (disp->buf2 != NULL); // 双缓冲开关

	// 每块 buffer 容量 buf_px_cnt。若脏区宽 > 容量,必须横向也切块,否则一行就越界。
	// tile_w = 一块的最大宽度(不超过脏区宽,也不超过容量)
	uint32 cap = disp->buf_px_cnt;
	GYcoord tile_w = (GYcoord)((cap < (uint32)dirty.w) ? cap : (uint32)dirty.w);
	if (tile_w < 1)
		tile_w = 1;

	GYsurface s;

	// 横向按 tile_w 切列
	for (GYcoord tx = dirty.x; tx < dirty.x + dirty.w; tx += tile_w)
	{
		GYcoord tw = (tx + tile_w <= dirty.x + dirty.w) ? tile_w : (dirty.x + dirty.w - tx);
		s.stride = tw; // buffer 每行 = 本列块宽度
		// 本列块每条 band 能容纳的行数 = 容量 / 块宽
		uint32 rows = cap / (uint32)tw;
		if (rows > (uint32)dirty.h)
			rows = (uint32)dirty.h;
		GYcoord band_h = (GYcoord)rows;
		if (band_h < 1)
			band_h = 1;
		// 纵向按 band_h 切行
		for (GYcoord y = dirty.y; y < dirty.y + dirty.h; y += band_h)
		{
			GYcoord bh = (y + band_h <= dirty.y + dirty.h) ? band_h : (dirty.y + dirty.h - y);
			s.buf = *pcur; // 本条 band 渲染进当前 buffer
			s.buf_area.x = tx;
			s.buf_area.y = y;
			s.buf_area.w = tw;
			s.buf_area.h = bh;
			s.clip = s.buf_area;

			// 从根开始重绘所有与本块相交的对象
			drawScene(ctx, &s);

			if (dbl)
			{
				// 双缓冲:发起本块 flush 前先等上一块传完(任一时刻至多 1 个在途传输),
				// 然后置忙、发起(异步 port 立即返回),切到另一块 → CPU 渲染下条 band 与本块 DMA 重叠
				waitFlushIdle(disp);
				disp->flush_busy = 1;
				disp->flush_cb(disp, &s.buf_area, s.buf);
				*pcur = (*pcur == disp->buf1) ? disp->buf2 : disp->buf1;
			}
			else
			{
				// 单缓冲:同步推给面板(唯一碰硬件处)。游标恒为 buf1
				disp->flush_cb(disp, &s.buf_area, s.buf);
			}
		}
	}
}

/**
 * @brief 刷新一帧:遍历脏矩形列表,每块各自 band 刷新。无脏区直接返回
 *        脏块互不重叠 → 不会重复混合同一像素
 */
void YMGUI_Refresh(GYCTX ctx)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	if (ctx->inv_cnt == 0)
		return; // 无脏区,空闲不耗

	GYDISP disp = (GYDISP)ctx->disp;
	gy_assert(disp && disp->buf1 && disp->flush_cb);

	// 双缓冲:入口先排空上帧可能仍在途的 DMA(否则本帧首块会覆盖正在传的 buffer);
	// 游标从 buf1 起,穿过本帧所有脏矩形,让 ping-pong 重叠不在矩形边界断开
	GYpx* cur = disp->buf1;
	if (disp->buf2 != NULL)
		waitFlushIdle(disp);

	for (uint8 i = 0; i < ctx->inv_cnt; i++)
		refreshOneRect(ctx, disp, &ctx->inv_areas[i], &cur);

	// 双缓冲:等本帧最后一块传完再返回,保证 Refresh 返回后所有 buffer 空闲(下帧可安全复用)
	if (disp->buf2 != NULL)
		waitFlushIdle(disp);

	YMGUI_Disp_FrameDone(disp);

	ctx->inv_cnt = 0; // 本帧脏区已处理
}
