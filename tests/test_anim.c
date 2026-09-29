#include "YMGUI_Anim.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include <stdio.h>

static int fails;
#define CHECK(cond, msg)                 \
	do                                   \
	{                                    \
		if (!(cond))                     \
		{                                \
			printf("  FAIL: %s\n", msg); \
			fails++;                     \
		}                                \
	} while (0)

static int32 value_seen;
static int done_count;
static int cancel_count;

static void valueChanged(int32 value, void* user_data)
{
	(void)user_data;
	value_seen = value;
}

static void animationDone(GYANIM animation, uint8 cancelled, void* user_data)
{
	(void)animation;
	(void)user_data;
	done_count++;
	if (cancelled)
		cancel_count++;
}

static int32 halfRate(GYAnimEase ease, int32 progress, void* user_data)
{
	(void)ease;
	(void)user_data;
	return progress / 2;
}

int main(void)
{
	GYdisp disp = {0};
	disp.hor_res = 320;
	disp.ver_res = 240;
	disp.buf_px_cnt = 320 * 20;
	disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, 320, 240);
	GYOBJ box = YMGUI_Creat_Obj_Creat(ctx->root, 10, 20, 40, 30);
	ctx->inv_cnt = 0;

	GYANIM move = YMGUI_Anim_MoveTo(box, 110, 70, 1000, GY_ANIM_LINEAR);
	CHECK(move != 0 && YMGUI_Anim_ActiveCount(ctx) == 1, "move starts and is counted");
	YMGUI_Anim_Tick(ctx, 500);
	CHECK(box->area.x == 60 && box->area.y == 45, "linear move reaches midpoint");
	CHECK(ctx->inv_cnt >= 1, "move invalidates old/new coverage");
	YMGUI_Anim_Tick(ctx, 500);
	CHECK(box->area.x == 110 && box->area.y == 70, "move reaches exact endpoint");
	CHECK(YMGUI_Anim_ActiveCount(ctx) == 0, "completed move is removed");

	GYANIM delayed = YMGUI_Anim_ResizeTo(box, 80, 60, 200, GY_ANIM_SMOOTH);
	CHECK(YMGUI_Anim_SetDelay(ctx, delayed, 100), "delay can be set before start");
	YMGUI_Anim_Tick(ctx, 50);
	CHECK(box->area.w == 40 && box->area.h == 30, "delay holds initial value");
	YMGUI_Anim_Tick(ctx, 150);
	CHECK(box->area.w == 60 && box->area.h == 45, "smooth resize reaches symmetric midpoint");

	GYANIM first = YMGUI_Anim_MoveTo(box, 200, 100, 500, GY_ANIM_LINEAR);
	CHECK(YMGUI_Anim_SetDone(ctx, first, animationDone, NULL), "done callback attaches");
	GYANIM replacement = YMGUI_Anim_MoveTo(box, 150, 90, 500, GY_ANIM_LINEAR);
	CHECK(replacement != 0 && first != replacement, "same property starts replacement animation");
	CHECK(cancel_count == 1, "replacement reports old animation cancelled");

	GYANIM value = YMGUI_Anim_ValueTo(ctx, box, 0, 100, 100, GY_ANIM_LINEAR, valueChanged, NULL);
	CHECK(YMGUI_Anim_SetDone(ctx, value, animationDone, NULL), "value animation done callback attaches");
	YMGUI_Anim_Tick(ctx, 100);
	CHECK(value_seen == 100, "value animation applies endpoint");
	CHECK(done_count == 2 && cancel_count == 1, "normal completion is distinguished from cancel");

	YMGUI_Anim_SetRateCallback(ctx, halfRate, NULL);
	GYANIM external = YMGUI_Anim_ValueTo(ctx, NULL, 0, 200, 100, GY_ANIM_OVERSHOOT, valueChanged, NULL);
	YMGUI_Anim_Tick(ctx, 100);
	CHECK(external != 0 && value_seen == 100, "external rate callback controls interpolation");
	YMGUI_Anim_SetRateCallback(ctx, NULL, NULL);
	GYANIM wide = YMGUI_Anim_ValueTo(ctx, box, INT32_MIN, INT32_MAX, 100,
							 GY_ANIM_LINEAR, valueChanged, NULL);
	YMGUI_Anim_Tick(ctx, 50);
	CHECK(wide != 0 && value_seen == -1, "full int32 range interpolates without overflow");
	YMGUI_Anim_Tick(ctx, 50);
	CHECK(value_seen == INT32_MAX, "full int32 range reaches endpoint");
	GYANIM clamp = YMGUI_Anim_ValueTo(ctx, box, INT32_MAX - 1000, INT32_MAX, 100,
							  GY_ANIM_OVERSHOOT, valueChanged, NULL);
	YMGUI_Anim_Tick(ctx, 70);
	CHECK(clamp != 0 && value_seen == INT32_MAX, "overshoot clamps at int32 maximum");
	YMGUI_Anim_Tick(ctx, 30);
	if (sizeof(GYcoord) == sizeof(int16))
	{
		box->scroll_x = INT16_MAX - 1000;
		GYANIM scroll = YMGUI_Anim_ScrollTo(box, INT16_MAX, 0, 100, GY_ANIM_OVERSHOOT);
		YMGUI_Anim_Tick(ctx, 70);
		CHECK(scroll != 0 && box->scroll_x == INT16_MAX, "overshoot clamps at 16-bit coordinate maximum");
		YMGUI_Anim_Tick(ctx, 30);
	}

	GYOBJ doomed = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, 10, 10);
	GYANIM doomed_anim = YMGUI_Anim_MoveTo(doomed, 20, 20, 1000, GY_ANIM_LINEAR);
	CHECK(doomed_anim != 0, "owned animation starts");
	uint16 before = YMGUI_Anim_ActiveCount(ctx);
	YMGUI_Free_ObjFree(doomed);
	CHECK(YMGUI_Anim_ActiveCount(ctx) + 1 == before, "object destruction cancels owned animation");

	YMGUI_Anim_CancelAll(ctx);
	CHECK(YMGUI_Anim_ActiveCount(ctx) == 0, "cancel all empties scheduler");
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);
	printf(fails ? "test_anim: %d FAILED\n" : "test_anim: ALL PASS\n", fails);
	return fails ? 1 : 0;
}
