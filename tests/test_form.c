#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_Switch.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Bar.h"
#include "YMGUI_Mem.h"
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_form.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 纯矩形表单四件套单测:checkbox 切换/switch 切换/slider 拖动改值/bar 显值,回调触发
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static void dummyFlush(GYdisp* d, const GYrect* a, const GYpx* b) { (void)d; (void)a; (void)b; }

static int cb_calls = 0, sw_calls = 0, sld_calls = 0;
static uint8 last_checked, last_on;
static int32 last_val;
static void onCb(GYOBJ o, uint8 c)  { (void)o; cb_calls++;  last_checked = c; }
static void onSw(GYOBJ o, uint8 on) { (void)o; sw_calls++;  last_on = on; }
static void onSld(GYOBJ o, int32 v) { (void)o; sld_calls++; last_val = v; }

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL;
	disp.flush_cb = dummyFlush;
	disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Inject_SetCtx(ctx);

	//---- Checkbox: 点击切换 ----
	GYOBJ cb = YMGUI_Creat_Checkbox_Creat(ctx->root, 10, 10, 120, 20);
	YMGUI_Checkbox_SetText(cb, "Enable");
	YMGUI_Checkbox_SetChanged(cb, onCb);
	CHECK(YMGUI_Checkbox_GetChecked(cb) == 0, "checkbox default unchecked");
	//点击(按下+抬起在框内,方框在 10..30)
	YMGUI_Inject_Pointer(15, 20, 1);
	YMGUI_Inject_Pointer(15, 20, 0);
	CHECK(YMGUI_Checkbox_GetChecked(cb) == 1, "checkbox toggled on by click");
	CHECK(cb_calls == 1 && last_checked == 1, "checkbox changed cb fired (checked=1)");
	//再点一次 → 关
	YMGUI_Inject_Pointer(15, 20, 1);
	YMGUI_Inject_Pointer(15, 20, 0);
	CHECK(YMGUI_Checkbox_GetChecked(cb) == 0, "checkbox toggled off");

	//---- Switch: 点击切换 ----
	GYOBJ sw = YMGUI_Creat_Switch_Creat(ctx->root, 10, 50, 60, 30);//y=50..80
	YMGUI_Switch_SetChanged(sw, onSw);
	CHECK(YMGUI_Switch_GetOn(sw) == 0, "switch default off");
	YMGUI_Inject_Pointer(40, 65, 1);
	YMGUI_Inject_Pointer(40, 65, 0);
	CHECK(YMGUI_Switch_GetOn(sw) == 1, "switch toggled on");
	CHECK(sw_calls == 1 && last_on == 1, "switch changed cb fired");

	//---- Slider: 拖动改值 ----
	GYOBJ sld = YMGUI_Creat_Slider_Creat(ctx->root, 20, 120, 200, 20);//x=20..220,y=120..140
	YMGUI_Slider_SetRange(sld, 0, 100);
	YMGUI_Slider_SetChanged(sld, onSld);
	CHECK(YMGUI_Slider_GetValue(sld) == 0, "slider default 0");
	//在滑块最右端按下 → 值应接近 max
	YMGUI_Inject_Pointer(215, 130, 1);
	CHECK(YMGUI_Slider_GetValue(sld) > 80, "press near right → high value");
	//拖到最左 → 值接近 min
	YMGUI_Inject_Pointer(20, 130, 1);//按住移动(pressed 已捕获)
	CHECK(YMGUI_Slider_GetValue(sld) == 0, "drag to left → value 0");
	//拖到中间 → 约 50
	YMGUI_Inject_Pointer(120, 130, 1);
	int32 mid = YMGUI_Slider_GetValue(sld);
	CHECK(mid >= 40 && mid <= 60, "drag to middle → ~50");
	YMGUI_Inject_Pointer(120, 130, 0);//抬起
	CHECK(sld_calls > 0, "slider changed cb fired during drag");

	//SetValue 编程设值
	YMGUI_Slider_SetValue(sld, 75);
	CHECK(YMGUI_Slider_GetValue(sld) == 75, "slider SetValue works");
	YMGUI_Slider_SetValue(sld, 999);//超范围钳制
	CHECK(YMGUI_Slider_GetValue(sld) == 100, "slider value clamped to max");

	//---- Bar: 显值(不交互) ----
	GYOBJ bar = YMGUI_Creat_Bar_Creat(ctx->root, 20, 160, 200, 16);
	YMGUI_Bar_SetRange(bar, 0, 200);
	YMGUI_Bar_SetValue(bar, 50);
	CHECK(YMGUI_Bar_GetValue(bar) == 50, "bar value set");
	//点击 bar 不应有任何反应(无 event_cb)
	YMGUI_Inject_Pointer(100, 168, 1);
	YMGUI_Inject_Pointer(100, 168, 0);
	CHECK(YMGUI_Bar_GetValue(bar) == 50, "bar ignores clicks");

	//---- 级联释放 ----
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_form: ALL PASS\n");
	else
		printf("test_form: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
