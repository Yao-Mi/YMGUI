#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Label.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Switch.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_Bar.h"
#include "YMGUI_ArcWidget.h"
#include "YMGUI_Meter.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_State.h"
#include "YMGUI_Bind.h"
#include "YMGUI_Mem.h"
#include <string.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_bind.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 状态/数据绑定单测(数据驱动地基):
  *	              绑定即同步当前值、subject→多控件扇出、控件交互写回 subject、双向断环不死循环、
  *	              静态声明 subject、字符串换指向刷新、控件 free 自动摘链后再写 subject 不崩。
  *	              判定以退出码为准(fails 计数),stdout 仅供人读。
  ***************************************************************************************************************************/

#define SCR_W 240
#define SCR_H 240
#define BAND_H 240

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

//整屏帧缓冲,便于按坐标采样
static GYpx g_fb[SCR_W * SCR_H];
static void fbFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d;
	for (GYcoord yy = 0; yy < a->h; yy++)
		for (GYcoord xx = 0; xx < a->w; xx++)
		{
			GYcoord sx = a->x + xx, sy = a->y + yy;
			if (sx >= 0 && sx < SCR_W && sy >= 0 && sy < SCR_H)
				g_fb[sy * SCR_W + sx] = b[yy * a->w + xx];
		}
}

//区域内是否存在非背景像素(证明文字被画出)
static int anyNonBg(GYcoord x, GYcoord y, GYcoord w, GYcoord h, GYpx bg)
{
	for (GYcoord j = 0; j < h; j++)
		for (GYcoord i = 0; i < w; i++)
			if (g_fb[(y + j) * SCR_W + (x + i)] != bg)
				return 1;
	return 0;
}

static void click(GYCTX ctx, GYcoord x, GYcoord y)
{
	YMGUI_Event_Pointer(ctx, x, y, 1);
	YMGUI_Event_Pointer(ctx, x, y, 0);
}

//静态声明的 subject(零堆,验证 GY_SUBJECT_* 宏可用作全局)
static GY_SUBJECT_INT(g_static_sv, 3);

//app 观察者:记录被回调的次数与最后收到的值(验证业务逻辑直接订阅状态)
static int   g_obs_calls = 0;
static int32 g_obs_last = 0;
static void  appObs(GYSUBJECT s, const GYval* v, void* ud)
{
	(void)s;
	g_obs_calls++;
	g_obs_last = v->u.i;
	if (ud != NULL)
		*(int*)ud += 1;//透传 user_data 验证
}

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = fbFlush; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0, 0, 0));
	GYpx BLACK = GY_ColorToPx(GY_ARGB(0xFF, 0, 0, 0));

	//---- 运行时初始化一个 int subject,绑两个 slider(扇出)+ 一个 label ----
	GYsubject sv;
	YMGUI_State_InitInt(&sv, 7);
	CHECK(YMGUI_State_GetInt(&sv) == 7, "InitInt sets value");

	GYOBJ sld1 = YMGUI_Creat_Slider_Creat(ctx->root, 10, 10, 100, 20);
	GYOBJ sld2 = YMGUI_Creat_Slider_Creat(ctx->root, 10, 40, 100, 20);
	YMGUI_Slider_SetRange(sld1, 0, 100);
	YMGUI_Slider_SetRange(sld2, 0, 100);

	//绑定即同步:两个 slider 都应立刻变成 subject 当前值 7
	YMGUI_Slider_Bind(sld1, &sv);
	YMGUI_Slider_Bind(sld2, &sv);
	CHECK(YMGUI_Slider_GetValue(sld1) == 7, "bind applies current value to sld1");
	CHECK(YMGUI_Slider_GetValue(sld2) == 7, "bind applies current value to sld2");

	//label 绑同一 subject,数值→文本
	GYOBJ lb = YMGUI_Creat_Label_Creat(ctx->root, 10, 70, 60, 20);
	YMGUI_Label_SetBgEnable(lb, 1);
	YMGUI_Label_SetTextColor(lb, GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	lb->bg_color = GY_ARGB(0xFF, 0, 0, 0);
	YMGUI_Label_Bind(lb, &sv);
	YMGUI_Refresh(ctx);
	CHECK(anyNonBg(10, 70, 60, 20, BLACK), "label shows text after bind (value 7)");

	//---- 后端改状态:两个 slider + label 一起动(subject→控件扇出) ----
	YMGUI_State_SetInt(&sv, 42);
	CHECK(YMGUI_Slider_GetValue(sld1) == 42, "SetInt fans out to sld1");
	CHECK(YMGUI_Slider_GetValue(sld2) == 42, "SetInt fans out to sld2");

	//---- 交互写回:拖 sld1 到最右 → 值=100 → 写回 subject → sld2 跟着到 100(双向,不死循环) ----
	//slider 在 (10,10) 100x20,拖柄宽=高=20,travel=80;rel=px-20 钳到[0,80]。
	//命中需 x<110(右边界 exclusive),x>=100 即 rel 饱和→value=100。取 108。
	click(ctx, 108, 20);
	CHECK(YMGUI_Slider_GetValue(sld1) == 100, "drag sld1 to right → 100");
	CHECK(YMGUI_State_GetInt(&sv) == 100, "sld1 writes back to subject");
	CHECK(YMGUI_Slider_GetValue(sld2) == 100, "writeback fans out to sld2 (two-way, no infinite loop)");

	//---- 摘链安全:free sld2 后再改 subject,不崩,sld1 仍跟随 ----
	YMGUI_Free_ObjFree(sld2);
	YMGUI_State_SetInt(&sv, 5);
	CHECK(YMGUI_Slider_GetValue(sld1) == 5, "after freeing sld2, subject still drives sld1");

	//---- compare-and-skip:重复写同值不应引发问题(隐式:不崩、值稳定) ----
	YMGUI_State_SetInt(&sv, 5);
	CHECK(YMGUI_Slider_GetValue(sld1) == 5, "repeat same value is stable");

	//---- 字符串 subject:换指向即刷新,空串则空白 ----
	GYsubject ss;
	YMGUI_State_InitStr(&ss, "Hi");
	GYOBJ lb2 = YMGUI_Creat_Label_Creat(ctx->root, 10, 100, 60, 20);
	YMGUI_Label_SetBgEnable(lb2, 1);
	YMGUI_Label_SetTextColor(lb2, GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	lb2->bg_color = GY_ARGB(0xFF, 0, 0, 0);
	YMGUI_Label_Bind(lb2, &ss);
	YMGUI_Refresh(ctx);
	CHECK(anyNonBg(10, 100, 60, 20, BLACK), "string subject renders 'Hi'");

	YMGUI_State_SetStr(&ss, "");
	YMGUI_Refresh(ctx);
	CHECK(!anyNonBg(10, 100, 60, 20, BLACK), "empty string clears label");

	CHECK(YMGUI_State_GetStr(&ss) != NULL, "GetStr returns the pointer");

	//---- 静态声明的 subject 可用 ----
	GYOBJ lb3 = YMGUI_Creat_Label_Creat(ctx->root, 10, 130, 60, 20);
	YMGUI_Label_Bind(lb3, &g_static_sv);
	CHECK(YMGUI_State_GetInt(&g_static_sv) == 3, "static-declared subject holds init value");
	YMGUI_State_SetInt(&g_static_sv, 88);
	CHECK(YMGUI_State_GetInt(&g_static_sv) == 88, "static subject writable");

	//---- Switch 双向 bool:绑同步、点击写回、SetBool 刷新 ----
	GYsubject bv;
	YMGUI_State_InitBool(&bv, 1);
	GYOBJ sw = YMGUI_Creat_Switch_Creat(ctx->root, 120, 10, 40, 20);
	YMGUI_Switch_Bind(sw, &bv);
	CHECK(YMGUI_Switch_GetOn(sw) == 1, "switch bind applies current bool (on)");
	//点击开关切换 → 写回 subject 变 0
	click(ctx, 140, 20);
	CHECK(YMGUI_Switch_GetOn(sw) == 0, "switch click toggled to off");
	CHECK(YMGUI_State_GetBool(&bv) == 0, "switch click wrote back to subject");
	//后端改 subject → 开关跟随
	YMGUI_State_SetBool(&bv, 1);
	CHECK(YMGUI_Switch_GetOn(sw) == 1, "SetBool drives switch back on");

	//---- Checkbox 与 Bar 绑同一 bool subject:点复选框 → 进度条(0/1)跟随(扇出+异构) ----
	GYOBJ ckb = YMGUI_Creat_Checkbox_Creat(ctx->root, 120, 40, 80, 20);
	GYOBJ bar = YMGUI_Creat_Bar_Creat(ctx->root, 120, 70, 80, 12);
	YMGUI_Bar_SetRange(bar, 0, 1);
	YMGUI_Checkbox_Bind(ckb, &bv);   //bv 当前=1
	YMGUI_Bar_Bind(bar, &bv);
	CHECK(YMGUI_Checkbox_GetChecked(ckb) == 1, "checkbox bind applies current bool");
	CHECK(YMGUI_Bar_GetValue(bar) == 1, "bar bind applies current bool");
	//点复选框 → 写回 0 → switch 和 bar 都跟随(三控件绑同一 subject)
	click(ctx, 130, 50);
	CHECK(YMGUI_Checkbox_GetChecked(ckb) == 0, "checkbox click toggled off");
	CHECK(YMGUI_State_GetBool(&bv) == 0, "checkbox wrote back to subject");
	CHECK(YMGUI_Switch_GetOn(sw) == 0, "fan-out: switch follows checkbox via shared subject");
	CHECK(YMGUI_Bar_GetValue(bar) == 0, "fan-out: bar follows via shared subject");

	//---- TextInput 双向 str:绑同步、键入写回、扇出到 label ----
	GYsubject tv;
	YMGUI_State_InitStr(&tv, "ab");
	GYOBJ ti = YMGUI_Creat_TextInput_Creat(ctx->root, 10, 160, 120, 24, 64);
	GYOBJ lbT = YMGUI_Creat_Label_Creat(ctx->root, 10, 190, 120, 20);
	YMGUI_TextInput_Bind(ti, &tv);
	YMGUI_Label_Bind(lbT, &tv);      //label 绑同一 str subject
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "ab") == 0, "textinput bind applies current str");
	//聚焦后键入 'c' → 文本变 "abc" → 写回 subject → label 也应读到 "abc"
	YMGUI_SetFocus(ctx, ti);
	YMGUI_Event_Key(ctx, 'c');
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "abc") == 0, "typing appended to textinput");
	CHECK(strcmp(YMGUI_State_GetStr(&tv), "abc") == 0, "textinput wrote back to subject");
	//再键入 'd' → 原地改内部缓冲,Touch 强制通知,subject 仍读到最新
	YMGUI_Event_Key(ctx, 'd');
	CHECK(strcmp(YMGUI_State_GetStr(&tv), "abcd") == 0, "in-place edit still notifies subject (Touch)");
	//后端改 subject → 文本框跟随
	YMGUI_State_SetStr(&tv, "xy");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "xy") == 0, "SetStr drives textinput");

	//---- Arc 与 Meter 绑同一 int subject:单向显示,后端改值 → 两者一起跟随(扇出+异构) ----
	GYsubject gauge;
	YMGUI_State_InitInt(&gauge, 30);
	GYOBJ arc = YMGUI_Creat_Arc_Creat(ctx->root, 210, 10, 60, 60);
	GYOBJ meter = YMGUI_Creat_Meter_Creat(ctx->root, 210, 80, 80, 80);
	YMGUI_Arc_SetRange(arc, 0, 100);
	YMGUI_Meter_SetRange(meter, 0, 100);
	YMGUI_Arc_Bind(arc, &gauge);      //gauge 当前=30
	YMGUI_Meter_Bind(meter, &gauge);
	CHECK(YMGUI_Arc_GetValue(arc) == 30, "arc bind applies current value");
	CHECK(YMGUI_Meter_GetValue(meter) == 30, "meter bind applies current value");
	//后端推新值 → 环形进度和仪表盘一起跟随(单向扇出,无写回)
	YMGUI_State_SetInt(&gauge, 85);
	CHECK(YMGUI_Arc_GetValue(arc) == 85, "SetInt drives arc value");
	CHECK(YMGUI_Meter_GetValue(meter) == 85, "fan-out: meter follows via shared subject");
	//越界值 → 各控件 SetValue 自钳制(不写回,subject 保持原值)
	YMGUI_State_SetInt(&gauge, 200);
	CHECK(YMGUI_Arc_GetValue(arc) == 100, "arc clamps out-of-range value");
	CHECK(YMGUI_Meter_GetValue(meter) == 100, "meter clamps out-of-range value");
	CHECK(YMGUI_State_GetInt(&gauge) == 200, "single-directional: subject unchanged by widget clamp");

	//---- app 观察者:业务逻辑直接订阅状态,与控件绑定并存 ----
	GYsubject av;
	YMGUI_State_InitInt(&av, 10);
	int udHits = 0;
	GYobserver* obsH = YMGUI_State_AddObserver(&av, appObs, &udHits);
	CHECK(obsH != NULL, "AddObserver returns handle");
	CHECK(g_obs_calls == 0, "AddObserver does not fire immediately");
	//后端改状态 → app 观察者收到
	YMGUI_State_SetInt(&av, 42);
	CHECK(g_obs_calls == 1 && g_obs_last == 42, "app observer notified on backend SetInt");
	CHECK(udHits == 1, "user_data passed through to app observer");
	//值未变 → compare-and-skip,不触发
	YMGUI_State_SetInt(&av, 42);
	CHECK(g_obs_calls == 1, "unchanged value does not notify app observer");
	//与控件绑定并存:同一 subject 同时挂 slider 和 app 观察者,交互写回时两者都动
	GYOBJ sldA = YMGUI_Creat_Slider_Creat(ctx->root, 10, 210, 100, 20);
	YMGUI_Slider_SetRange(sldA, 0, 100);
	YMGUI_Slider_Bind(sldA, &av);   //绑定即 apply,slider 现应=42
	CHECK(YMGUI_Slider_GetValue(sldA) == 42, "slider bind applies current app-subject value");
	click(ctx, 108, 220);           //拖到饱和端 → 写回 subject → app 观察者也收到
	CHECK(YMGUI_State_GetInt(&av) == 100, "slider drag wrote back to shared subject");
	CHECK(g_obs_calls == 2 && g_obs_last == 100, "app observer sees UI-driven change too");
	//摘掉 app 观察者后,后续变化不再回调(slider 仍在链上,不受影响)
	YMGUI_State_RemoveObserver(obsH);
	YMGUI_State_SetInt(&av, 7);
	CHECK(g_obs_calls == 2, "removed app observer no longer notified");
	CHECK(YMGUI_Slider_GetValue(sldA) == 7, "widget observer still live after app observer removed");

	//---- 析构不崩(绑定的控件随树释放,自动摘链) ----
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	//释放后再写 subject:观察者已随控件摘净,不应崩(sv 是栈上,链应为空)
	YMGUI_State_SetInt(&sv, 999);
	CHECK(YMGUI_State_GetInt(&sv) == 999, "after ctx free, subject write is safe (observers unlinked)");

	if (fails == 0)
		printf("test_bind: ALL PASS\n");
	else
		printf("test_bind: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
