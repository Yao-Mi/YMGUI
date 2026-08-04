#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Label.h"
#include "YMGUI_Meter.h"
#include "YMGUI_ArcWidget.h"
#include "YMGUI_Bar.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Chart.h"
#include "YMGUI_Spinner.h"
#include "YMGUI_Switch.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_Dropdown.h"
#include "YMGUI_Tabview.h"
#include "YMGUI_Layout.h"
#include "YMGUI_State.h"
#include "YMGUI_Bind.h"
#include "YMGUI_Font.h"
#include "YMGUI_Trig.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    dashboard.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: project_Demo 第七个基础验证项目 —— 实时监控面板(system/sensor dashboard)。960x600。
  *	              本项目不催生新控件;目标是把此前只有孤立 demo、从没进过真应用的一簇能力一次性压出来:
  *	              Tabview / Meter / ArcWidget / Bar / Chart / Spinner / Switch / Checkbox
  *	              + 完整数据绑定线(UI=f(state):单向扇出 + Chart 走 AddObserver + 设置页双向绑定)
  *	              + 布局助手 Stack/Align。分层铁律:控件不认识 "CPU/温度";传感器语义、合成波形、
  *	              告警阈值全在本 app 侧。数据源=纯合成(库内 Q15 GY_Sin 波形,无 libm、headless 可复现)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * 备注信息:
  * 1.数据流:app 后端每 interval 帧用 GY_Sin 合成 CPU/内存/温度/磁盘值 → YMGUI_State_SetInt 推进 subject;
  *   绑定的 Meter/Arc/Bar/Label 自动扇出跟随,后端全程不碰任何控件(这是本项目重点验证的架构)。
  * 2.Chart 不可绑定 → 走 YMGUI_State_AddObserver 订阅 cpu/mem subject,回调里 SetNext 滚动
  *   (这是"控件绑定 + 自定义逻辑并存"的正解,第一次在真应用里检验)。
  * 3.设置页 Switch/Checkbox/Slider 双向绑到 auto/alarm/interval subject,用户拨动直接回写 state。
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 960
#define SCR_H 600
#define BAND_H 60

#define TAB_BAR_H 34
#define CHART_POINTS 60   //曲线页横向采样点数

//---- GB2312 全字库回退(同 txt_edit/excel_edit)----
#ifndef GB2312_BIN_PATH
#define GB2312_BIN_PATH "gb2312_glyphs.bin"
#endif
extern const uint16 YMGUI_GB2312_cps[];
extern const uint16 YMGUI_GB2312_glyph_count;
static FILE* s_blob = NULL;
static uint32 flashRead(const GYfont* font, uint32 off, uint32 len, uint8* buf)
{
	(void)font;
	if (s_blob == NULL) return 0;
	if (fseek(s_blob, (long)off, SEEK_SET) != 0) return 0;
	return (uint32)fread(buf, 1, len, s_blob);
}
static GYfont s_gb_font = { NULL, YMGUI_GB2312_cps, 0, 0, 0, 16, 16, 8, 4, NULL, flashRead };

//======================== 状态(唯一真相,后端只改这里,UI 自动跟)========================
// 传感器读数(单向扇出到 Meter/Arc/Bar/Label + Chart observer)
static GY_SUBJECT_INT(sv_cpu,  0);   //CPU 占用 0..100 (%)
static GY_SUBJECT_INT(sv_mem,  0);   //内存占用 0..100 (%)
static GY_SUBJECT_INT(sv_temp, 0);   //温度 0..100 (℃)
static GY_SUBJECT_INT(sv_disk, 0);   //磁盘占用 0..100 (%)
// 设置项(设置页 Switch/Checkbox/Slider 双向绑定,后端读它决定行为)
static GY_SUBJECT_BOOL(sv_auto,     1);   //自动刷新开关
static GY_SUBJECT_BOOL(sv_alarm,    0);   //高温告警开关
static GY_SUBJECT_INT (sv_interval, 8);   //刷新间隔(帧),范围 2..30

//======================== 全局控件句柄 ========================
static GYCTX g_ctx;
static GYOBJ g_chart;           //曲线页折线图
static int   g_ser_cpu = -1;    //CPU 序列索引
static int   g_ser_mem = -1;    //内存序列索引
static GYOBJ g_spinner;         //顶部刷新指示(每帧 Tick)
static GYOBJ g_lbl_cpu, g_lbl_mem, g_lbl_temp, g_lbl_disk;  //读数标签(绑定)
static GYOBJ g_cpu_meter, g_mem_arc, g_temp_meter, g_disk_bar;  //表盘控件(留句柄供 selftest 回读绑定扇出)

static int   g_selftest_fail = 0;   //selftest 失败置 1 → main 返回非 0
static int   g_frame = 0;           //帧计数(假传感器节拍)

//======================== Chart observer(绑定 + 自定义逻辑并存的正解)========================
// Chart 不可 *_Bind(它是多序列流式,不是标量控件),故 app 侧订阅 subject:
// 每当 cpu/mem 值变(后端 SetInt 触发),把新值 SetNext 进对应序列滚动。
static void onCpuChanged(GYSUBJECT s, const GYval* v, void* user_data)
{
	(void)s; (void)user_data;
	if (g_chart != NULL && g_ser_cpu >= 0)
		YMGUI_Chart_SetNext(g_chart, g_ser_cpu, v->u.i);
}
static void onMemChanged(GYSUBJECT s, const GYval* v, void* user_data)
{
	(void)s; (void)user_data;
	if (g_chart != NULL && g_ser_mem >= 0)
		YMGUI_Chart_SetNext(g_chart, g_ser_mem, v->u.i);
}

//======================== 假传感器后端(全程不碰控件,只改 state)========================
// 用库内 Q15 GY_Sin 合成三路相位错开的波形,映射到 0..100。纯整数、headless 可复现。
static int synthPct(int deg, int base, int amp)
{
	// GY_Sin 返回 Q15 [-32768,32767];换算成 [base-amp, base+amp] 再钳到 0..100
	int32 s = GY_Sin(deg);            //Q15
	int   v = base + (int)((s * amp) >> 15);
	if (v < 0)   v = 0;
	if (v > 100) v = 100;
	return v;
}
static void feedSensors(void)
{
	int d = g_frame * 3;   //每帧推进相位
	YMGUI_State_SetInt(&sv_cpu,  synthPct(d,        55, 40));
	YMGUI_State_SetInt(&sv_mem,  synthPct(d + 120,  60, 25));
	YMGUI_State_SetInt(&sv_temp, synthPct(d + 240,  48, 20));
	YMGUI_State_SetInt(&sv_disk, synthPct(d / 4,    58, 30));  //磁盘变化更慢
}

//======================== UI 构建 ========================
// 一张"指标卡片":容器 + 标题(顶) + 读数(底),中间留给调用者塞表盘控件。
// 返回卡片容器(已开 ClipChildren),表盘控件由调用者建为其子并居中。
static GYOBJ makeCard(GYOBJ page, GYcoord x, GYcoord y, GYcoord w, GYcoord h,
                      const char* title, GYOBJ* out_read_lbl)
{
	GYOBJ card = YMGUI_Creat_Obj_Creat(page, x, y, w, h);
	YMGUI_Obj_SetBgColor(card, GY_ARGB(0xFF, 0x1A, 0x1D, 0x28));

	GYOBJ tlbl = YMGUI_Creat_Label_Creat(card, 8, 6, w - 16, 16);
	YMGUI_Label_SetTextColor(tlbl, GY_ARGB(0xFF, 0xC8, 0xC8, 0xD0));
	YMGUI_Label_SetText(tlbl, title);

	GYOBJ rlbl = YMGUI_Creat_Label_Creat(card, 8, h - 22, w - 16, 16);
	YMGUI_Label_SetTextColor(rlbl, GY_ARGB(0xFF, 0x60, 0xE0, 0xA0));
	YMGUI_Label_SetText(rlbl, "0");
	if (out_read_lbl != NULL) *out_read_lbl = rlbl;
	return card;
}

// 总览页:CPU Meter + 内存 Arc + 温度 Meter 三卡横排 + 磁盘 Bar 一条。
// 读数 Label 全走单向绑定;表盘控件也走单向绑定 —— 后端改 subject 即全部自动跟。
static void buildOverview(GYOBJ page)
{
	GYcoord cw = 220, ch = 200, gap = 20, top = 20;
	GYcoord x0 = (SCR_W - (cw * 3 + gap * 2)) / 2;

	//---- CPU 卡:Meter ----
	GYOBJ c1 = makeCard(page, x0, top, cw, ch, "CPU 占用 (%)", &g_lbl_cpu);
	g_cpu_meter = YMGUI_Creat_Meter_Creat(c1, (cw - 130) / 2, 28, 130, 130);
	YMGUI_Meter_SetRange(g_cpu_meter, 0, 100);
	YMGUI_Meter_SetTicks(g_cpu_meter, 6);         //6 个大刻度:0/20/40/60/80/100
	YMGUI_Meter_SetShowLabels(g_cpu_meter, 1);    //刻度旁标数值,便于肉眼预估
	YMGUI_Meter_Bind(g_cpu_meter, &sv_cpu);   //单向:subject → 表盘
	YMGUI_Label_Bind(g_lbl_cpu, &sv_cpu);   //单向:subject → 读数

	//---- 内存卡:ArcWidget ----
	GYOBJ c2 = makeCard(page, x0 + cw + gap, top, cw, ch, "内存占用 (%)", &g_lbl_mem);
	g_mem_arc = YMGUI_Creat_Arc_Creat(c2, (cw - 130) / 2, 28, 130, 130);
	YMGUI_Arc_SetRange(g_mem_arc, 0, 100);
	YMGUI_Arc_SetColors(g_mem_arc, GY_ARGB(0xFF, 0x30, 0x34, 0x44), GY_ARGB(0xFF, 0x40, 0xA0, 0xF0));
	YMGUI_Arc_Bind(g_mem_arc, &sv_mem);
	YMGUI_Label_Bind(g_lbl_mem, &sv_mem);

	//---- 温度卡:Meter ----
	GYOBJ c3 = makeCard(page, x0 + (cw + gap) * 2, top, cw, ch, "温度 (℃)", &g_lbl_temp);
	g_temp_meter = YMGUI_Creat_Meter_Creat(c3, (cw - 130) / 2, 28, 130, 130);
	YMGUI_Meter_SetRange(g_temp_meter, 0, 150);
	YMGUI_Meter_SetTicks(g_temp_meter, 6);        //0/30/60/90/120/150
	YMGUI_Meter_SetShowLabels(g_temp_meter, 1);
	YMGUI_Meter_Bind(g_temp_meter, &sv_temp);
	YMGUI_Label_Bind(g_lbl_temp, &sv_temp);

	//---- 磁盘条:Bar(整幅横排在卡片下方)----
	GYcoord by = top + ch + 30;
	GYOBJ dlbl = YMGUI_Creat_Label_Creat(page, x0, by, 120, 16);
	YMGUI_Label_SetTextColor(dlbl, GY_ARGB(0xFF, 0xC8, 0xC8, 0xD0));
	YMGUI_Label_SetText(dlbl, "磁盘占用 (%)");
	g_disk_bar = YMGUI_Creat_Bar_Creat(page, x0, by + 20, cw * 3 + gap * 2 - 60, 22);
	YMGUI_Bar_SetRange(g_disk_bar, 0, 100);
	YMGUI_Bar_SetColors(g_disk_bar, GY_ARGB(0xFF, 0x28, 0x2C, 0x3A), GY_ARGB(0xFF, 0xF0, 0xA0, 0x40));
	YMGUI_Bar_Bind(g_disk_bar, &sv_disk);
	g_lbl_disk = YMGUI_Creat_Label_Creat(page, x0 + cw * 3 + gap * 2 - 52, by + 22, 48, 16);
	YMGUI_Label_SetTextColor(g_lbl_disk, GY_ARGB(0xFF, 0x60, 0xE0, 0xA0));
	YMGUI_Label_Bind(g_lbl_disk, &sv_disk);
}

// 曲线页:一张 Chart,2 序列(CPU/内存)。值不由绑定注入 —— Chart 是流式多序列,
// app 侧用 AddObserver 订阅 cpu/mem subject,值变时 SetNext 滚动(绑定+自定义逻辑并存)。
static void buildTrends(GYOBJ page)
{
	g_chart = YMGUI_Creat_Chart_Creat(page, 30, 30, SCR_W - 60, SCR_H - TAB_BAR_H - 80);
	YMGUI_Chart_SetRange(g_chart, 0, 100);
	YMGUI_Chart_SetPointCount(g_chart, CHART_POINTS);
	YMGUI_Chart_SetGrid(g_chart, 5, 6);
	YMGUI_Chart_SetColors(g_chart, GY_ARGB(0xFF, 0x12, 0x14, 0x1C), GY_ARGB(0xFF, 0x30, 0x34, 0x44));
	g_ser_cpu = YMGUI_Chart_AddSeries(g_chart, GY_ARGB(0xFF, 0x50, 0xC0, 0xF0));
	g_ser_mem = YMGUI_Chart_AddSeries(g_chart, GY_ARGB(0xFF, 0xF0, 0x90, 0x50));

	GYOBJ leg = YMGUI_Creat_Label_Creat(page, 30, SCR_H - TAB_BAR_H - 44, SCR_W - 60, 16);
	YMGUI_Label_SetTextColor(leg, GY_ARGB(0xFF, 0xB0, 0xB0, 0xC0));
	YMGUI_Label_SetText(leg, "蓝: CPU    橙: 内存    (随时间左移滚动)");

	//订阅 subject:值一变就喂进 Chart。第一次在真应用里检验 AddObserver。
	YMGUI_State_AddObserver(&sv_cpu, onCpuChanged, NULL);
	YMGUI_State_AddObserver(&sv_mem, onMemChanged, NULL);
}

// 设置页:Switch/Checkbox/Slider 双向绑定 + Dropdown 采样源。竖排用 Layout_Stack。
static void buildSettings(GYOBJ page)
{
	GYcoord x = 40, y = 30, rowh = 40;

	GYOBJ l1 = YMGUI_Creat_Label_Creat(page, x, y + 6, 160, 18);
	YMGUI_Label_SetTextColor(l1, GY_ARGB(0xFF, 0xC8, 0xC8, 0xD0));
	YMGUI_Label_SetText(l1, "自动刷新");
	GYOBJ sw = YMGUI_Creat_Switch_Creat(page, x + 180, y, 56, 28);
	YMGUI_Switch_Bind(sw, &sv_auto);   //双向:拨动即回写 sv_auto

	y += rowh;
	GYOBJ ckb = YMGUI_Creat_Checkbox_Creat(page, x, y, 240, 24);
	YMGUI_Checkbox_SetText(ckb, "高温告警(>80℃ 变红)");
	YMGUI_Checkbox_Bind(ckb, &sv_alarm);

	y += rowh;
	GYOBJ l3 = YMGUI_Creat_Label_Creat(page, x, y + 6, 160, 18);
	YMGUI_Label_SetTextColor(l3, GY_ARGB(0xFF, 0xC8, 0xC8, 0xD0));
	YMGUI_Label_SetText(l3, "刷新间隔(帧)");
	GYOBJ sld = YMGUI_Creat_Slider_Creat(page, x + 180, y + 4, 200, 18);
	YMGUI_Slider_SetRange(sld, 2, 30);
	YMGUI_Slider_Bind(sld, &sv_interval);
	GYOBJ ivlbl = YMGUI_Creat_Label_Creat(page, x + 392, y + 4, 48, 18);
	YMGUI_Label_SetTextColor(ivlbl, GY_ARGB(0xFF, 0x60, 0xE0, 0xA0));
	YMGUI_Label_Bind(ivlbl, &sv_interval);   //同一 subject 扇出到读数

	y += rowh;
	GYOBJ l4 = YMGUI_Creat_Label_Creat(page, x, y + 6, 160, 18);
	YMGUI_Label_SetTextColor(l4, GY_ARGB(0xFF, 0xC8, 0xC8, 0xD0));
	YMGUI_Label_SetText(l4, "采样源");
	GYOBJ dd = YMGUI_Creat_Dropdown_Creat(page, x + 180, y, 200, 26);
	YMGUI_Dropdown_AddOption(dd, "合成波形(demo)");
	YMGUI_Dropdown_AddOption(dd, "系统 /proc(未接)");
	YMGUI_Dropdown_AddOption(dd, "网络遥测(未接)");
	YMGUI_Dropdown_SetSelected(dd, 0);
}

//======================== selftest(headless,argv[1]>0 时跑一遍)========================
// 验证本项目的重点线:绑定扇出、Chart observer、双向回读、Tabview 切页。
static void selftest(GYOBJ tabview)
{
	int fail = 0;

	//1) 单向绑定扇出(本项目重点):直接改 subject,绑定的表盘控件应立即收到值。
	//   用 GetValue 真回读 —— 这是"后端只改 state、控件自动跟"的硬证据。
	YMGUI_State_SetInt(&sv_cpu,  83);
	YMGUI_State_SetInt(&sv_mem,  61);
	YMGUI_State_SetInt(&sv_temp, 47);
	YMGUI_State_SetInt(&sv_disk, 55);
	if (YMGUI_Meter_GetValue(g_cpu_meter)  != 83) { gy_log_print("selftest FAIL: cpu meter fan-out\n");  fail = 1; }
	if (YMGUI_Arc_GetValue(g_mem_arc)      != 61) { gy_log_print("selftest FAIL: mem arc fan-out\n");    fail = 1; }
	if (YMGUI_Meter_GetValue(g_temp_meter) != 47) { gy_log_print("selftest FAIL: temp meter fan-out\n"); fail = 1; }
	if (YMGUI_Bar_GetValue(g_disk_bar)     != 55) { gy_log_print("selftest FAIL: disk bar fan-out\n");   fail = 1; }

	//2) Chart observer:sv_cpu/sv_mem 变化应被 SetNext 喂进对应序列末尾。
	YMGUI_State_SetInt(&sv_cpu, 77);
	YMGUI_State_SetInt(&sv_mem, 44);
	if (g_chart != NULL && g_ser_cpu >= 0)
	{
		int32 last_cpu = YMGUI_Chart_GetValue(g_chart, g_ser_cpu, CHART_POINTS - 1);
		int32 last_mem = YMGUI_Chart_GetValue(g_chart, g_ser_mem, CHART_POINTS - 1);
		if (last_cpu != 77) { gy_log_print("selftest FAIL: chart cpu last=%d != 77\n", (int)last_cpu); fail = 1; }
		if (last_mem != 44) { gy_log_print("selftest FAIL: chart mem last=%d != 44\n", (int)last_mem); fail = 1; }
	}
	else { gy_log_print("selftest FAIL: chart/series not built\n"); fail = 1; }

	//3) 双向绑定回读:改 sv_auto,GetBool 应一致(Switch 已随之刷新视觉态)。
	YMGUI_State_SetInt(&sv_interval, 15);
	if (YMGUI_State_GetInt(&sv_interval) != 15) { gy_log_print("selftest FAIL: interval readback\n"); fail = 1; }
	YMGUI_State_SetBool(&sv_auto, 0);
	if (YMGUI_State_GetBool(&sv_auto) != 0) { gy_log_print("selftest FAIL: auto readback\n"); fail = 1; }
	YMGUI_State_SetBool(&sv_auto, 1);   //恢复,便于后续帧继续喂数据

	//4) Tabview 切页:切到每一页,GetActive 应回读正确,隐藏页不崩。
	{
		uint16 n = YMGUI_Tabview_GetTabCount(tabview);
		if (n != 3) { gy_log_print("selftest FAIL: tab count=%u != 3\n", (unsigned)n); fail = 1; }
		for (uint16 i = 0; i < n; i++)
		{
			YMGUI_Tabview_SetActive(tabview, i);
			if (YMGUI_Tabview_GetActive(tabview) != i) { gy_log_print("selftest FAIL: tab active %u\n", (unsigned)i); fail = 1; }
		}
		YMGUI_Tabview_SetActive(tabview, 0);   //回总览页
	}

	if (fail) g_selftest_fail = 1;
	else      gy_log_print("selftest: dashboard bind + chart feed OK\n");
}

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;

	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;

	SDL_LCD_Init(&disp, 1);
	g_ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Inject_SetCtx(g_ctx);   //把 ctx 注册给输入层,鼠标/键盘事件才注入得进来(交互必需)
	YMGUI_Obj_SetBgColor(g_ctx->root, GY_ARGB(0xFF, 0x0E, 0x0E, 0x14));

	//GB2312 全字库回退(中文标题/单位)
	s_blob = fopen(GB2312_BIN_PATH, "rb");
	if (s_blob != NULL)
	{
		s_gb_font.glyph_count = YMGUI_GB2312_glyph_count;
		YMGUI_Font_SetFallback(&s_gb_font);
	}
	else
		gy_log_print("warn: gb2312 blob not found, CJK limited to built-in glyphs\n");

	//顶部刷新指示 Spinner(每帧 Tick)
	g_spinner = YMGUI_Creat_Spinner_Creat(g_ctx->root, SCR_W - 30, 4, 24, 24);

	//Tabview 三页
	GYOBJ tv = YMGUI_Creat_Tabview_Creat(g_ctx->root, 0, 0, SCR_W, SCR_H);
	YMGUI_Tabview_SetBarHeight(tv, TAB_BAR_H);
	GYOBJ p_overview = YMGUI_Tabview_AddTab(tv, "总览");
	GYOBJ p_trends   = YMGUI_Tabview_AddTab(tv, "曲线");
	GYOBJ p_settings = YMGUI_Tabview_AddTab(tv, "设置");
	buildOverview(p_overview);
	buildTrends(p_trends);
	buildSettings(p_settings);

	//先喂一帧初值,避免开局全 0
	feedSensors();

	if (max_frames > 0)
		selftest(tv);

	//帧循环:自动刷新开时按 interval 帧喂一次假数据;Spinner 每帧转。
	while (SDL_LCD_PumpEvents())
	{
		int interval = YMGUI_State_GetInt(&sv_interval);
		if (interval < 1) interval = 1;
		if (YMGUI_State_GetBool(&sv_auto) && (g_frame % interval == 0))
			feedSensors();

		YMGUI_Spinner_Tick(g_spinner);
		YMGUI_Refresh(g_ctx);
		SDL_LCD_Delay(16);

		g_frame++;
		if (max_frames > 0 && g_frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(g_ctx);
	SDL_LCD_Destroy();
	if (s_blob != NULL) fclose(s_blob);
	gy_log_print("dashboard exit ok\n");
	return g_selftest_fail ? 1 : 0;
}



