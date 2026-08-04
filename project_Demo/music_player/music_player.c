#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Font.h"
#include "YMGUI_Trig.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Roller.h"
#include "YMGUI_BarChart.h"
#include "SDL_LCD.h"
#include "mp_audio.h"
#include "mp_lrc.h"
#include "mp_spectrum.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    music_player.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 第 5 个基础验证项目——音乐播放器。催生库控件 Roller(通用居中高亮平滑滚动列表:歌词/
  *	              时间/日历通用)+ BarChart(通用柱状图,此处当频谱用)。音乐语义全在 app 侧(mp_audio:ffmpeg
  *	              popen 解码 + SDL2 声卡 + 播放时钟 + 暂停/seek + 静音兜底;mp_lrc:.lrc 解析;
  *	              mp_spectrum:整数 Goertzel 频段分析)。UI:标题 + 播放/暂停 + 可拖动进度 Slider(seek)
  *	              + 时间标签 + BarChart 频谱 + Roller 歌词(按时钟 SetSelected 到当前行)。无文件/无
  *	              ffmpeg 时合成旋律兜底,headless 无声卡时静音兜底。GB2312 全字库回退。headless 自检
  *	              打印 "selftest: ... OK" + "music_player exit ok",以 exit code 判成败。
  *	@Version:     1.0
  ***************************************************************************************************************************/

#define SCR_W 800
#define SCR_H 480
#define BAND_H 60

#define SPEC_BANDS 24 //频谱柱数

//---- GB2312 全字库回退 ----
#ifndef GB2312_BIN_PATH
#define GB2312_BIN_PATH "gb2312_glyphs.bin"
#endif
//内置默认曲目(CMake 传绝对路径;未定义时回落到当前目录相对名)
#ifndef MP_DEFAULT_AUDIO
#define MP_DEFAULT_AUDIO NULL
#endif
#ifndef MP_DEFAULT_LRC
#define MP_DEFAULT_LRC NULL
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

//---- 全局状态 ----
static GYCTX   g_ctx = NULL;
static MPaudio* g_audio = NULL;
static MPlrc   g_lrc;
static uint8   g_has_lrc = 0;

static GYOBJ g_btn_play = NULL;
static GYOBJ g_slider   = NULL;

//---- 播放/暂停图标(程序生成,colorkey 透明抠形;用贴图代替文字) ----
#define MP_ICON 28
static GYpx  g_play_px[MP_ICON * MP_ICON];
static GYpx  g_pause_px[MP_ICON * MP_ICON];
static GYimg g_img_play;
static GYimg g_img_pause;

static void buildPlayIcons(void)
{
	GYpx keypx = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0x00, 0xFF)); //品红透明底
	GYpx fgpx  = GY_ColorToPx(GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0)); //近白前景
	int x, y;
	for (y = 0; y < MP_ICON; y++)
	{
		for (x = 0; x < MP_ICON; x++)
		{
			int idx = y * MP_ICON + x;
			int half = (22 - x); //三角:左宽右尖(尖朝右=播放),x 6..22
			int on_play = (x >= 6 && x <= 22 && (y - MP_ICON / 2) <= half
			               && (MP_ICON / 2 - y) <= half);
			g_play_px[idx] = on_play ? fgpx : keypx;
			int on_pause = (((x >= 7 && x <= 11) || (x >= 17 && x <= 21)) && y >= 6 && y <= 21);
			g_pause_px[idx] = on_pause ? fgpx : keypx;
		}
	}
	g_img_play.data  = g_play_px;  g_img_play.w  = MP_ICON; g_img_play.h  = MP_ICON;
	g_img_play.use_key = 1;        g_img_play.key = keypx;
	g_img_pause.data = g_pause_px; g_img_pause.w = MP_ICON; g_img_pause.h = MP_ICON;
	g_img_pause.use_key = 1;       g_img_pause.key = keypx;
}

//按播放态更新按钮图标(播放中显暂停图,反之显播放图)
static void updatePlayIcon(void)
{
	YMGUI_Button_SetImage(g_btn_play,
		mp_audio_is_playing(g_audio) ? &g_img_pause : &g_img_play);
}
static GYOBJ g_lbl_cur  = NULL;
static GYOBJ g_lbl_dur  = NULL;
static GYOBJ g_lbl_stat = NULL;
static GYOBJ g_spectrum = NULL;
static GYOBJ g_roller   = NULL;

static uint8 g_user_dragging = 0; //用户正拖进度条(拖时不被时钟回写)
static int   g_selftest_fail = 0;

//毫秒 -> "m:ss"
static void fmtTime(char* buf, int32 bufsz, int32 ms)
{
	if (ms < 0) ms = 0;
	int32 s = ms / 1000;
	snprintf(buf, bufsz, "%d:%02d", s / 60, s % 60);
}

//---- 回调 ----
static void onPlayPause(GYOBJ btn)
{
	(void)btn;
	if (g_audio == NULL) return;
	//播完后再点从头开始
	if (mp_audio_is_finished(g_audio))
		mp_audio_seek_ms(g_audio, 0);
	mp_audio_toggle(g_audio);
	updatePlayIcon();
}

static void onSeek(GYOBJ sld, int32 value)
{
	(void)sld;
	//用户拖动进度条 -> seek 到对应毫秒
	if (g_audio == NULL) return;
	int32 dur = mp_audio_dur_ms(g_audio);
	int32 ms = (int32)((int64)value * dur / 1000); //slider 范围 0..1000
	mp_audio_seek_ms(g_audio, ms);
}

//每帧:把播放时钟同步到进度条 / 时间 / 歌词 / 频谱
static void syncFromClock(void)
{
	if (g_audio == NULL) return;
	int32 pos = mp_audio_pos_ms(g_audio);
	int32 dur = mp_audio_dur_ms(g_audio);

	//进度条(拖动中不回写,避免抖)
	if (!g_user_dragging && dur > 0)
	{
		int32 v = (int32)((int64)pos * 1000 / dur);
		YMGUI_Slider_SetValue(g_slider, v);
	}

	//时间标签
	char tc[16], td[16];
	fmtTime(tc, sizeof(tc), pos);
	fmtTime(td, sizeof(td), dur);
	YMGUI_Label_SetText(g_lbl_cur, tc);
	YMGUI_Label_SetText(g_lbl_dur, td);

	//歌词:按当前毫秒高亮对应行(平滑滚动)
	if (g_has_lrc)
	{
		int32 idx = mp_lrc_index_at(&g_lrc, pos);
		if (idx >= 0 && idx != YMGUI_Roller_GetSelected(g_roller))
			YMGUI_Roller_SetSelected(g_roller, idx, 1);
	}

	//频谱:取一窗 mono 做频段分析喂柱
	int16 win[1024];
	int32 got = mp_audio_peek_mono(g_audio, win, 1024);
	if (got > 64)
	{
		int32 bands[SPEC_BANDS];
		mp_spectrum_analyze(win, got, MP_AUD_RATE, SPEC_BANDS, bands, 100);
		YMGUI_BarChart_SetValues(g_spectrum, bands, SPEC_BANDS);
	}
}

//默认歌词(无 .lrc 文件时用,演示 Roller 平滑滚动 + 中文回退)
static const struct { int32 ms; const char* t; } DEFAULT_LRC[] = {
	{     0, "YMGUI 音乐播放器演示" },
	{  2000, "Roller 控件:居中高亮" },
	{  4000, "平滑滚动到当前行" },
	{  6000, "BarChart 控件:实时频谱" },
	{  8000, "拖动进度条可以跳转" },
	{ 10000, "点击按钮暂停 / 播放" },
	{ 12000, "歌词按时钟自动滚动" },
	{ 14000, "同一个 Roller 也能显示" },
	{ 16000, "时间 / 日历 / 拨码盘" },
	{ 18000, "库不认识歌词语义" },
	{ 20000, "音乐逻辑全在 app 侧" },
};

static void loadLyrics(const char* lrc_path)
{
	mp_lrc_clear(&g_lrc);
	g_has_lrc = 0;
	if (lrc_path != NULL && mp_lrc_load_file(&g_lrc, lrc_path) > 0)
		g_has_lrc = 1;
	else
	{
		//内置默认词
		int i, n = (int)(sizeof(DEFAULT_LRC) / sizeof(DEFAULT_LRC[0]));
		for (i = 0; i < n; i++)
			mp_lrc_add(&g_lrc, DEFAULT_LRC[i].ms, DEFAULT_LRC[i].t);
		g_has_lrc = (g_lrc.count > 0);
	}
	//把歌词行灌进 Roller
	YMGUI_Roller_Clear(g_roller);
	int32 i;
	for (i = 0; i < g_lrc.count; i++)
		YMGUI_Roller_AddLine(g_roller, g_lrc.lines[i].text);
	YMGUI_Roller_SetSelected(g_roller, 0, 0);
}

//===========================================================================
// selftest —— headless 下验证核心逻辑(不依赖真实声卡/图形)
//===========================================================================
static void selftest(void)
{
	int fails = 0;

	//1) LRC 解析:时间戳 + 排序 + 查询
	{
		MPlrc t; mp_lrc_clear(&t);
		const char* lrc = "[ti:x]\n[00:01.50]line A\n[00:03.00]line B\n[00:00.00]intro\n";
		int32 n = mp_lrc_parse(&t, lrc);
		if (n != 3) { gy_log_print("selftest FAIL: lrc parsed %d want 3\n", n); fails++; }
		//应按时间排序:intro(0) < A(1500) < B(3000)
		if (t.count >= 3)
		{
			if (t.lines[0].ms != 0 || t.lines[1].ms != 1500 || t.lines[2].ms != 3000)
			{ gy_log_print("selftest FAIL: lrc sort ms=%d,%d,%d\n", t.lines[0].ms, t.lines[1].ms, t.lines[2].ms); fails++; }
			if (strcmp(t.lines[0].text, "intro") != 0)
			{ gy_log_print("selftest FAIL: lrc text[0]='%s'\n", t.lines[0].text); fails++; }
		}
		//查询:2000ms 应命中 index 1(A,1500<=2000<3000)
		if (mp_lrc_index_at(&t, 2000) != 1) { gy_log_print("selftest FAIL: lrc index@2000=%d want 1\n", mp_lrc_index_at(&t, 2000)); fails++; }
		if (mp_lrc_index_at(&t, 5000) != 2) { gy_log_print("selftest FAIL: lrc index@5000\n"); fails++; }
		if (mp_lrc_index_at(&t, -100) != 0) { gy_log_print("selftest FAIL: lrc index@neg\n"); fails++; }
	}

	//2) 频谱:纯正弦应在对应频段有能量,静音应全 0
	{
		int16 sine[512];
		int i;
		//1000Hz 正弦
		for (i = 0; i < 512; i++)
		{
			int32 deg = (int32)(((int64)360 * 1000 * i / MP_AUD_RATE) % 360);
			sine[i] = (int16)((GY_Sin(deg) * 3) / 4);
		}
		int32 bands[SPEC_BANDS];
		mp_spectrum_analyze(sine, 512, MP_AUD_RATE, SPEC_BANDS, bands, 100);
		int32 sum = 0; for (i = 0; i < SPEC_BANDS; i++) sum += bands[i];
		if (sum <= 0) { gy_log_print("selftest FAIL: spectrum sine no energy\n"); fails++; }

		int16 quiet[512]; for (i = 0; i < 512; i++) quiet[i] = 0;
		mp_spectrum_analyze(quiet, 512, MP_AUD_RATE, SPEC_BANDS, bands, 100);
		sum = 0; for (i = 0; i < SPEC_BANDS; i++) sum += bands[i];
		if (sum != 0) { gy_log_print("selftest FAIL: spectrum silence energy=%d\n", sum); fails++; }
	}

	//3) 音频引擎:合成兜底 + seek + 位置 + peek
	{
		MPaudio* a = mp_audio_create();
		if (a == NULL) { gy_log_print("selftest FAIL: audio create\n"); fails++; }
		else
		{
			mp_audio_load_synth(a, 5); //5 秒
			int32 dur = mp_audio_dur_ms(a);
			if (dur < 4800 || dur > 5200) { gy_log_print("selftest FAIL: synth dur=%d\n", dur); fails++; }
			mp_audio_seek_ms(a, 2000);
			int32 p = mp_audio_pos_ms(a);
			if (p < 1900 || p > 2100) { gy_log_print("selftest FAIL: seek pos=%d want ~2000\n", p); fails++; }
			//seek 越界钳制
			mp_audio_seek_ms(a, 999999);
			if (mp_audio_pos_ms(a) > dur) { gy_log_print("selftest FAIL: seek clamp\n"); fails++; }
			//peek 应拿到样本
			int16 w[256];
			mp_audio_seek_ms(a, 1000);
			int32 g = mp_audio_peek_mono(a, w, 256);
			if (g <= 0) { gy_log_print("selftest FAIL: peek got %d\n", g); fails++; }
			mp_audio_destroy(a);
		}
	}

	if (fails == 0)
		gy_log_print("selftest: lrc + spectrum + audio engine OK\n");
	else
	{
		gy_log_print("selftest: %d FAILED\n", fails);
		g_selftest_fail = 1;
	}
}

//===========================================================================
// main
//===========================================================================
int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	//约定:argv[1]=帧数上限(headless 自检)。argv[2]=音频文件。argv[3]=.lrc
	//无 argv[2]/argv[3] 时回落到内置默认曲目(工程自带的 可能-队长.mp3 / .lrc)
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	const char* audio_path = (argc > 2) ? argv[2] : MP_DEFAULT_AUDIO;
	const char* lrc_path   = (argc > 3) ? argv[3] : MP_DEFAULT_LRC;
	int frame = 0;

	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;

	SDL_LCD_Init(&disp, 1);
	g_ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(g_ctx->root, GY_ARGB(0xFF, 0x10, 0x12, 0x1A));

	//GB2312 全字库回退
	s_blob = fopen(GB2312_BIN_PATH, "rb");
	if (s_blob != NULL) { s_gb_font.glyph_count = YMGUI_GB2312_glyph_count; YMGUI_Font_SetFallback(&s_gb_font); }
	else gy_log_print("warn: gb2312 blob not found, CJK limited to built-in glyphs\n");

	//标题
	GYOBJ title = YMGUI_Creat_Label_Creat(g_ctx->root, 16, 10, SCR_W - 32, 20);
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0x50, 0xE0, 0xC0));
	YMGUI_Label_SetText(title, "music_player —— 音乐播放器(Roller 歌词 + BarChart 频谱)");

	//---- 频谱(上半):BarChart 当频谱用 —— 随高度渐变 + 顶部峰值回落 ----
	g_spectrum = YMGUI_Creat_BarChart_Creat(g_ctx->root, 16, 40, SCR_W - 32, 150);
	YMGUI_BarChart_SetBarCount(g_spectrum, SPEC_BANDS);
	YMGUI_BarChart_SetRange(g_spectrum, 100);
	YMGUI_BarChart_SetColorMode(g_spectrum, GY_BARCHART_COLOR_BY_HEIGHT);
	YMGUI_BarChart_SetGradient(g_spectrum, GY_ARGB(0xFF, 0x20, 0x80, 0xF0),
	                           GY_ARGB(0xFF, 0xF0, 0x40, 0x80));
	YMGUI_BarChart_SetTopMode(g_spectrum, GY_BARCHART_TOP_BAR);
	YMGUI_BarChart_SetTopColor(g_spectrum, GY_ARGB(0xFF, 0xFF, 0xF0, 0x80));
	YMGUI_BarChart_SetBgColor(g_spectrum, GY_ARGB(0xFF, 0x0A, 0x0C, 0x14));
	YMGUI_BarChart_SetDecay(g_spectrum, 6, 2);
	YMGUI_BarChart_SetGap(g_spectrum, 3);

	//---- 歌词 Roller(中部) ----
	g_roller = YMGUI_Creat_Roller_Creat(g_ctx->root, 16, 200, SCR_W - 32, 170);
	YMGUI_Roller_SetVisibleRows(g_roller, 5);
	YMGUI_Roller_SetRowHeight(g_roller, 32);
	YMGUI_Roller_SetColors(g_roller, GY_ARGB(0xFF, 0x10, 0x12, 0x1A),
	                        GY_ARGB(0xFF, 0x70, 0x74, 0x84), GY_ARGB(0xFF, 0xF0, 0xF4, 0xFF),
	                        GY_ARGB(0xFF, 0x1E, 0x24, 0x34));
	YMGUI_Roller_SetEaseDiv(g_roller, 5);

	//---- 进度条 + 时间(下部) ----
	g_lbl_cur = YMGUI_Creat_Label_Creat(g_ctx->root, 16, 388, 50, 18);
	YMGUI_Label_SetTextColor(g_lbl_cur, GY_ARGB(0xFF, 0xC0, 0xC4, 0xD0));
	YMGUI_Label_SetText(g_lbl_cur, "0:00");

	g_slider = YMGUI_Creat_Slider_Creat(g_ctx->root, 72, 390, SCR_W - 144, 16);
	YMGUI_Slider_SetRange(g_slider, 0, 1000);
	YMGUI_Slider_SetValue(g_slider, 0);
	YMGUI_Slider_SetChanged(g_slider, onSeek);

	g_lbl_dur = YMGUI_Creat_Label_Creat(g_ctx->root, SCR_W - 66, 388, 50, 18);
	YMGUI_Label_SetTextColor(g_lbl_dur, GY_ARGB(0xFF, 0xC0, 0xC4, 0xD0));
	YMGUI_Label_SetText(g_lbl_dur, "0:00");

	//---- 播放/暂停按钮(图标代替文字:▶/⏸,colorkey 抠形,保留底色圆角块作可点区) ----
	buildPlayIcons();
	g_btn_play = YMGUI_Creat_Button_Creat(g_ctx->root, SCR_W / 2 - 22, 418, 44, 40);
	YMGUI_Button_SetImage(g_btn_play, &g_img_play);
	YMGUI_Button_SetClicked(g_btn_play, onPlayPause);

	//状态栏
	g_lbl_stat = YMGUI_Creat_Label_Creat(g_ctx->root, 16, SCR_H - 16, SCR_W - 32, 14);
	YMGUI_Label_SetTextColor(g_lbl_stat, GY_ARGB(0xFF, 0x70, 0x74, 0x84));

	//---- 音频引擎 ----
	g_audio = mp_audio_create();
	uint8 real = 0;
	if (g_audio) real = mp_audio_load(g_audio, audio_path);

	loadLyrics(lrc_path);

	{
		char st[128];
		snprintf(st, sizeof(st), "%s  |  %s  |  拖动进度条跳转,点按钮播放/暂停",
		         real ? "已加载音频文件" : (audio_path ? "解码失败:合成旋律兜底" : "无文件:合成旋律兜底"),
		         mp_audio_is_silent(g_audio) ? "静音兜底(无声卡)" : "声卡输出");
		YMGUI_Label_SetText(g_lbl_stat, st);
	}

	YMGUI_Inject_SetCtx(g_ctx);

	if (max_frames > 0)
		selftest();
	else
		mp_audio_play(g_audio); //窗口模式自动开播

	updatePlayIcon();

	while (SDL_LCD_PumpEvents())
	{
		//检测进度条拖动:按下态时视为用户拖动(时钟不回写)
		g_user_dragging = (g_slider->state & GY_STATE_Pressed) ? 1 : 0;

		mp_audio_tick(g_audio);
		syncFromClock();
		YMGUI_BarChart_Tick(g_spectrum);
		YMGUI_Roller_Tick(g_roller);

		//播完自动复位按钮图标为 ▶
		if (mp_audio_is_finished(g_audio) && mp_audio_is_playing(g_audio))
		{
			mp_audio_pause(g_audio);
			updatePlayIcon();
		}

		YMGUI_Refresh(g_ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	if (g_audio) mp_audio_destroy(g_audio);
	YMGUI_Free_CtxFree(g_ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	if (s_blob != NULL) fclose(s_blob);
	gy_log_print("music_player exit ok\n");
	return g_selftest_fail ? 1 : 0;
}
