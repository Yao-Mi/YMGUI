#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Joystick.h"
#include <stdio.h>

#define SCR_W 160
#define SCR_H 120

static int fails;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static int changed_count;
static int released_count;
static int16 released_x, released_y;
static int custom_draw_count;
static int override_release;

static void flushCb(GYdisp* disp, const GYrect* area, const GYpx* buffer)
{
	(void)disp; (void)area; (void)buffer;
}

static void changedCb(GYOBJ joystick, int16 x, int16 y)
{
	(void)joystick; (void)x; (void)y;
	changed_count++;
}

static void releasedCb(GYOBJ joystick, int16 x, int16 y)
{
	(void)joystick;
	released_count++;
	released_x = x;
	released_y = y;
	if (override_release)
		YMGUI_Joystick_SetValue(joystick, 25, 0);
}

static void customDraw(GYOBJ joystick, GYSURFACE surface, const GYrect* abs)
{
	(void)joystick; (void)surface; (void)abs;
	custom_draw_count++;
}

int main(void)
{
	GYdisp disp = {0};
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = SCR_W * SCR_H;
	disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
	disp.flush_cb = flushCb;
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	GYOBJ joystick = YMGUI_Creat_Joystick_Creat(ctx->root, 20, 10, 100, 100);
	YMGUI_Joystick_SetChangedCb(joystick, changedCb);
	YMGUI_Joystick_SetReleasedCb(joystick, releasedCb);
	YMGUI_Joystick_SetDrawCb(joystick, customDraw);
	YMGUI_Inject_SetCtx(ctx);
	int16 initial_x = 1, initial_y = 1;
	YMGUI_Joystick_GetValue(joystick, &initial_x, &initial_y);
	CHECK(initial_x == 0 && initial_y == 0, "joystick value initializes at center");
	YMGUI_Refresh(ctx);
	CHECK(custom_draw_count > 0, "custom joystick skin draws on first frame");
	YMGUI_Joystick_SetDrawCb(joystick, NULL);

	CHECK(YMGUI_Joystick_GetAutoCenter(joystick) == 1, "auto-center defaults on");
	CHECK(YMGUI_Joystick_GetDeadzone(joystick) == 8, "default deadzone is 8");

	//中心约 (70,60)，向右拖到边缘应钳到单位圆。
	YMGUI_Inject_Pointer(70, 60, 1);
	YMGUI_Inject_Pointer(119, 60, 1);
	int16 x = 0, y = 0;
	YMGUI_Joystick_GetValue(joystick, &x, &y);
	CHECK(x >= 99 && y == 0, "right drag clamps to +X edge");
	CHECK(YMGUI_Joystick_IsActive(joystick), "joystick active while pressed");
	YMGUI_Inject_Pointer(119, 60, 0);
	YMGUI_Joystick_GetValue(joystick, &x, &y);
	CHECK(x == 0 && y == 0, "auto-center resets value on release");
	CHECK(released_count == 1 && released_x >= 99 && released_y == 0,
	      "release callback receives final value");

	//released 回调在自动回中之后执行，可覆盖成业务自定义停靠值。
	override_release = 1;
	YMGUI_Inject_Pointer(70, 60, 1);
	YMGUI_Inject_Pointer(21, 60, 1);
	YMGUI_Inject_Pointer(21, 60, 0);
	YMGUI_Joystick_GetValue(joystick, &x, &y);
	CHECK(x == 25 && y == 0, "release callback can override auto-center result");
	CHECK(released_count == 2 && released_x <= -99, "override callback still receives final input");
	override_release = 0;
	YMGUI_Joystick_SetValue(joystick, 0, 0);

	//保持模式:松手后保留位置，回调仍通知操控结束。
	YMGUI_Joystick_SetAutoCenter(joystick, 0);
	YMGUI_Inject_Pointer(70, 60, 1);
	YMGUI_Inject_Pointer(70, 11, 1);
	YMGUI_Inject_Pointer(70, 11, 0);
	YMGUI_Joystick_GetValue(joystick, &x, &y);
	CHECK(x == 0 && y >= 99, "hold mode preserves final value");
	CHECK(!YMGUI_Joystick_IsActive(joystick), "hold mode still ends active state");
	CHECK(released_count == 3 && released_y >= 99, "hold mode emits release callback");
	int changed_before_auto = changed_count;
	YMGUI_Joystick_SetAutoCenter(joystick, 1);
	YMGUI_Joystick_GetValue(joystick, &x, &y);
	CHECK(x == 0 && y == 0, "switching HOLD to AUTO centers inactive joystick");
	CHECK(changed_count == changed_before_auto + 1, "HOLD to AUTO notifies value change");

	//程序设值按圆形范围钳位，不触发 changed。
	int before = changed_count;
	YMGUI_Joystick_SetValue(joystick, 100, 100);
	YMGUI_Joystick_GetValue(joystick, &x, &y);
	CHECK(x >= 70 && x <= 71 && y >= 70 && y <= 71, "diagonal SetValue clamps to circle");
	CHECK(changed_count == before, "programmatic SetValue does not notify changed");

	//死区与自定义绘制。
	YMGUI_Joystick_SetDeadzone(joystick, 20);
	YMGUI_Inject_Pointer(70, 60, 1);
	YMGUI_Inject_Pointer(73, 60, 1);
	YMGUI_Joystick_GetValue(joystick, &x, &y);
	CHECK(x == 0 && y == 0, "small movement is suppressed by deadzone");
	YMGUI_Inject_Pointer(73, 60, 0);
	YMGUI_Joystick_SetDrawCb(joystick, customDraw);
	YMGUI_Refresh(ctx);
	CHECK(custom_draw_count > 1, "custom draw callback can be restored after creation");

	YMGUI_Inject_SetCtx(NULL);
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);
	if (fails == 0) printf("test_joystick: ALL PASS\n");
	else printf("test_joystick: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
