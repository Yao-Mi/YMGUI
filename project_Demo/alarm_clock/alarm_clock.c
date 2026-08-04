#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_Roller.h"
#include "YMGUI_Switch.h"
#include "YMGUI_MsgBox.h"
#include "YMGUI_Font.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    alarm_clock.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-05
  *	@Description: project_Demo 第八个基础验证项目 —— 闹钟(alarm clock)。480x320。
  *	              本项目不催生新控件;目标是把刚落地的 MsgBox 模态弹窗放进真应用里压一遍:
  *	              到点 + 使能开 → 弹出阻塞模态,遮罩锁住整个界面,必须点"关闭"按钮才解锁。
  *	              三个 Roller(时/分/秒)设定闹钟时刻,Switch 使能,帧驱动一个假时钟每帧走 1 秒。
  *	              分层铁律:控件不认识"闹钟/时刻";时间推进、到点判定、去抖全在本 app 侧。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * 备注信息:
  * 1.时钟模型:g_now_sec 是"当天已过秒数"(0..86399),每帧 +1(演示可见跳秒);到 86400 归 0。
  * 2.到点判定:g_now_sec==闹钟秒 且 使能开 且 本次未触发过(g_fired 去抖)→ MsgBox_Show。
  *   过了这一秒清 g_fired,下一整天同一时刻能再响。
  * 3.MsgBox 是库控件(挂 top_layer),app 只管配置文案 + Show;关闭按钮回调里回写状态标签。
  * 4.headless 自检:把闹钟设在"当前+2秒",跑几帧应触发模态;模拟点关闭按钮 → 模态隐藏、
  *   遮罩解锁(底层按钮又能点)。断言全过打印 OK。
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 480
#define SCR_H 320
#define BAND_H 64

//---- GB2312 全字库回退(同 dashboard/txt_edit)----
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

//======================== 时钟状态(唯一真相,app 侧管)========================
static GYCTX g_ctx;
static GYOBJ g_roll_h, g_roll_m, g_roll_s;   //时/分/秒设定滚轮
static GYOBJ g_lbl_now;                       //当前时间大字
static GYOBJ g_lbl_alarm;                     //闹钟时刻标签
static GYOBJ g_lbl_status;                    //状态行(响过/已关)
static GYOBJ g_sw_enable;                     //使能开关
static GYOBJ g_mb;                            //模态弹窗(库控件,挂 top_layer)

static int32 g_now_sec = 0;    //当天已过秒数 0..86399
static uint8 g_fired    = 0;   //本次到点去抖:已弹过就不重复弹
static int   g_ring_cnt = 0;   //累计响铃次数(演示 + selftest 回读)
static int   g_selftest_fail = 0;

//两位数字表(滚轮行文本):0..59 复用,时段只取前 24 行
static char s_num2[60][3];
static const char* s_num2p[60];
static void initNumTable(void)
{
	for (int i = 0; i < 60; i++)
	{
		s_num2[i][0] = (char)('0' + i / 10);
		s_num2[i][1] = (char)('0' + i % 10);
		s_num2[i][2] = '\0';
		s_num2p[i] = s_num2[i];
	}
}

static int32 alarmSec(void)
{
	int32 h = YMGUI_Roller_GetSelected(g_roll_h);
	int32 m = YMGUI_Roller_GetSelected(g_roll_m);
	int32 s = YMGUI_Roller_GetSelected(g_roll_s);
	return (h * 60 + m) * 60 + s;
}

static void fmtHMS(char* buf, int cap, const char* prefix, int32 sec)
{
	int32 h = (sec / 3600) % 24;
	int32 m = (sec / 60) % 60;
	int32 s = sec % 60;
	snprintf(buf, cap, "%s%02d:%02d:%02d", prefix, (int)h, (int)m, (int)s);
}

static void refreshNowLabel(void)
{
	char buf[32];
	fmtHMS(buf, sizeof(buf), "", g_now_sec);
	YMGUI_Label_SetText(g_lbl_now, buf);
}
static void refreshAlarmLabel(void)
{
	char buf[40];
	fmtHMS(buf, sizeof(buf), "闹钟 ", alarmSec());
	YMGUI_Label_SetText(g_lbl_alarm, buf);
}

//======================== 模态关闭回调 ========================
static void onAlarmDismiss(GYOBJ mb, int idx)
{
	(void)mb; (void)idx;
	char buf[48];
	snprintf(buf, sizeof(buf), "闹钟已关闭 (共响 %d 次)", g_ring_cnt);
	YMGUI_Label_SetText(g_lbl_status, buf);
}

//到点:弹模态
static void ringAlarm(void)
{
	char body[80];
	g_ring_cnt++;
	fmtHMS(body, sizeof(body), "到设定时刻 ", alarmSec());
	YMGUI_MsgBox_ClearButtons(g_mb);
	YMGUI_MsgBox_SetTitle(g_mb, "闹钟");
	strncat(body, "\n点击关闭停止", sizeof(body) - strlen(body) - 1);
	YMGUI_MsgBox_SetText(g_mb, body);
	YMGUI_MsgBox_AddButton(g_mb, "关闭", onAlarmDismiss);
	YMGUI_MsgBox_Show(g_mb);
	YMGUI_Label_SetText(g_lbl_status, "闹钟响了! 请点关闭");
}

//滚轮改动 → 更新闹钟标签
static void onRollChanged(GYOBJ roller, int32 index)
{
	(void)roller; (void)index;
	refreshAlarmLabel();
}

//使能开关
static void onEnableChanged(GYOBJ sw, uint8 on)
{
	(void)sw;
	YMGUI_Label_SetText(g_lbl_status, on ? "闹钟已启用" : "闹钟已停用");
}

//每帧推进假时钟 1 秒,并做到点判定
static void tickClock(void)
{
	g_now_sec = (g_now_sec + 1) % 86400;
	refreshNowLabel();

	int32 a = alarmSec();
	if (YMGUI_Switch_GetOn(g_sw_enable) && g_now_sec == a)
	{
		if (!g_fired)
		{
			g_fired = 1;
			ringAlarm();
		}
	}
	else
	{
		g_fired = 0;   //离开这一秒后允许下次再响
	}
}

//======================== UI 构建 ========================
static GYOBJ makeRoller(GYcoord x, GYcoord y, int32 rows_max)
{
	GYOBJ r = YMGUI_Creat_Roller_Creat(g_ctx->root, x, y, 60, 120);
	YMGUI_Roller_SetLines(r, s_num2p, rows_max);
	YMGUI_Roller_SetVisibleRows(r, 3);
	YMGUI_Roller_SetInteractive(r, 1);   //开交互态:拖动改选中(默认程序态只认 SetSelected)
	YMGUI_Roller_SetChanged(r, onRollChanged);
	return r;
}

//======================== headless 自检 ========================
static void selftest(void)
{
	//闹钟设在"当前+2秒",使能打开,应在几帧内触发模态
	int32 target = (g_now_sec + 2) % 86400;
	YMGUI_Roller_SetSelected(g_roll_h, (target / 3600) % 24, 0);
	YMGUI_Roller_SetSelected(g_roll_m, (target / 60) % 60, 0);
	YMGUI_Roller_SetSelected(g_roll_s, target % 60, 0);
	YMGUI_Switch_SetOn(g_sw_enable, 1);
	refreshAlarmLabel();

	//推进时钟直到触发(上限 10 帧防跑飞)
	int guard = 0;
	while (!YMGUI_MsgBox_IsShown(g_mb) && guard < 10)
	{
		tickClock();
		guard++;
	}
	if (YMGUI_MsgBox_IsShown(g_mb) != 1)
	{
		gy_log_print("selftest FAIL: modal not shown at alarm time\n");
		g_selftest_fail = 1;
		return;
	}
	if (g_ring_cnt != 1)
	{
		gy_log_print("selftest FAIL: ring count != 1\n");
		g_selftest_fail = 1;
	}

	//模态显示中:点关闭按钮(卡片第一个子=按钮)应隐藏模态、解锁
	GYOBJ card = g_mb->child_head;
	GYOBJ btn  = (card != NULL) ? card->child_head : NULL;
	if (btn == NULL)
	{
		gy_log_print("selftest FAIL: no close button\n");
		g_selftest_fail = 1;
		return;
	}
	GYrect a; YMGUI_Obj_GetAbsArea(btn, &a);
	YMGUI_Event_Pointer(g_ctx, (GYcoord)(a.x + a.w / 2), (GYcoord)(a.y + a.h / 2), 1);
	YMGUI_Event_Pointer(g_ctx, (GYcoord)(a.x + a.w / 2), (GYcoord)(a.y + a.h / 2), 0);
	if (YMGUI_MsgBox_IsShown(g_mb) != 0)
	{
		gy_log_print("selftest FAIL: modal still shown after close\n");
		g_selftest_fail = 1;
		return;
	}

	if (!g_selftest_fail)
		gy_log_print("selftest: alarm modal + roller OK\n");
}

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;

	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;

	initNumTable();

	SDL_LCD_Init(&disp, 1);
	g_ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Inject_SetCtx(g_ctx);
	YMGUI_Obj_SetBgColor(g_ctx->root, GY_ARGB(0xFF, 0x12, 0x14, 0x1C));

	//GB2312 全字库回退(中文标题/按钮)
	s_blob = fopen(GB2312_BIN_PATH, "rb");
	if (s_blob != NULL)
	{
		s_gb_font.glyph_count = YMGUI_GB2312_glyph_count;
		YMGUI_Font_SetFallback(&s_gb_font);
	}
	else
		gy_log_print("warn: gb2312 blob not found, CJK limited to built-in glyphs\n");

	GYOBJ title = YMGUI_Creat_Label_Creat(g_ctx->root, 0, 6, SCR_W, 16);
	YMGUI_Label_SetText(title, "闹钟");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	//当前时间大字
	g_lbl_now = YMGUI_Creat_Label_Creat(g_ctx->root, 0, 30, SCR_W, 16);
	YMGUI_Label_SetTextColor(g_lbl_now, GY_ARGB(0xFF, 0xF0, 0xF0, 0xFF));
	refreshNowLabel();

	//三滚轮:时 : 分 : 秒
	g_roll_h = makeRoller(80,  70, 24);
	GYOBJ c1 = YMGUI_Creat_Label_Creat(g_ctx->root, 146, 118, 12, 16);
	YMGUI_Label_SetText(c1, ":");
	g_roll_m = makeRoller(170, 70, 60);
	GYOBJ c2 = YMGUI_Creat_Label_Creat(g_ctx->root, 236, 118, 12, 16);
	YMGUI_Label_SetText(c2, ":");
	g_roll_s = makeRoller(260, 70, 60);

	//闹钟标签 + 使能开关
	g_lbl_alarm = YMGUI_Creat_Label_Creat(g_ctx->root, 40, 210, 200, 16);
	YMGUI_Label_SetTextColor(g_lbl_alarm, GY_ARGB(0xFF, 0xA0, 0xD0, 0xFF));

	GYOBJ lbl_en = YMGUI_Creat_Label_Creat(g_ctx->root, 300, 210, 60, 16);
	YMGUI_Label_SetText(lbl_en, "使能");
	g_sw_enable = YMGUI_Creat_Switch_Creat(g_ctx->root, 360, 206, 48, 24);
	YMGUI_Switch_SetOn(g_sw_enable, 1);
	YMGUI_Switch_SetChanged(g_sw_enable, onEnableChanged);

	//状态行
	g_lbl_status = YMGUI_Creat_Label_Creat(g_ctx->root, 0, 260, SCR_W, 16);
	YMGUI_Label_SetText(g_lbl_status, "拨动滚轮设定闹钟时刻");
	YMGUI_Label_SetTextColor(g_lbl_status, GY_ARGB(0xFF, 0xA0, 0xE0, 0xA0));

	//模态弹窗(库控件,初始隐藏)
	g_mb = YMGUI_Creat_MsgBox_Creat(g_ctx);

	//默认闹钟设在 07:30:00
	YMGUI_Roller_SetSelected(g_roll_h, 7, 0);
	YMGUI_Roller_SetSelected(g_roll_m, 30, 0);
	YMGUI_Roller_SetSelected(g_roll_s, 0, 0);
	refreshAlarmLabel();

	if (max_frames > 0)
		selftest();

	//帧循环:每帧走 1 秒假时钟 + 滚轮缓动 Tick
	while (SDL_LCD_PumpEvents())
	{
		tickClock();
		YMGUI_Roller_Tick(g_roll_h);
		YMGUI_Roller_Tick(g_roll_m);
		YMGUI_Roller_Tick(g_roll_s);
		YMGUI_Refresh(g_ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(g_ctx);
	SDL_LCD_Destroy();
	if (s_blob != NULL) fclose(s_blob);
	gy_log_print("alarm_clock exit ok\n");
	return g_selftest_fail ? 1 : 0;
}
