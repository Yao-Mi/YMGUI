#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Font.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Image.h"
#include "SDL_LCD.h"
#include "mp_audio.h"
#include "vp_video.h"
#include <SDL2/SDL.h> //SDL_GetTicks:拖动预览节流用(SDL 已由 SDL_LCD 起)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    video_player.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 第 6 个基础验证项目——视频播放器。催生的库能力是"缩放 blit 图元"(CORE:Draw_ImgScaled)+
  *	              把既有 Image 控件扩出 NONE/FIT/FILL 缩放模式(不新增控件、不动 Canvas)。视频画面 = Image
  *	              处于 FIT 模式 + app 每帧把解码帧换进 GYimg 像素。视频/音频语义全在 app 侧:vp_video(ffprobe
  *	              探参 + ffmpeg popen 流式解码 rgb565le + 非阻塞读 + 帧环形缓冲背压 + seek 重启);mp_audio
  *	              (复用音乐播放器:ffmpeg 整曲 PCM + SDL2 声卡)。A/V 同步:音频为主钟,视频丢/持帧跟随。
  *	              UI:标题 + 视频窗(Image FIT)+ 播放/暂停(图标)+ 可拖动进度 Slider(seek)+ 时间标签 +
  *	              状态栏。无 ffmpeg/文件时合成动画+旋律兜底,headless 无声卡静音兜底。GB2312 全字库回退。
  *	              headless 自检打印 "selftest: ... OK" + "video_player exit ok",以 exit code 判成败。
  *	@Version:     1.0
  ***************************************************************************************************************************/

#define SCR_W 800
#define SCR_H 480
#define BAND_H 60

//视频解码上限盒(display_buf 尺寸,Image FIT 再缩到窗内)
#define VID_MAX_W 640
#define VID_MAX_H 360

//---- GB2312 全字库回退 ----
#ifndef GB2312_BIN_PATH
#define GB2312_BIN_PATH "gb2312_glyphs.bin"
#endif
#ifndef VP_DEFAULT_VIDEO
#define VP_DEFAULT_VIDEO NULL
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
static GYCTX    g_ctx   = NULL;
static MPaudio* g_audio = NULL;
static VPvideo* g_video = NULL;

static GYOBJ g_video_pane = NULL; //视频画面(Image,FIT)
static GYOBJ g_btn_play   = NULL;
static GYOBJ g_slider     = NULL;
static GYOBJ g_lbl_cur    = NULL;
static GYOBJ g_lbl_dur    = NULL;
static GYOBJ g_lbl_stat   = NULL;

static GYimg g_frame_img;          //指向 vp_video 的 display_buf(不复制)
static uint8  g_user_dragging = 0;
static uint8  g_prev_dragging = 0; //上一帧拖动态(检测松手边沿)
static int32  g_pending_seek  = -1;//拖动中待处理的视频 seek 目标毫秒(-1=无)
static uint32 g_last_preview_ms = 0;//上次拖动预览 seek 的墙钟时刻(节流)
#define VP_PREVIEW_THROTTLE_MS 300 //拖动中最多每 300ms 预览 seek 视频一次(避免狂重启 ffmpeg)
static int   g_selftest_fail = 0;

//---- 播放/暂停图标(程序生成,colorkey 透明抠形) ----
#define VP_ICON 28
static GYpx  g_play_px[VP_ICON * VP_ICON];
static GYpx  g_pause_px[VP_ICON * VP_ICON];
static GYimg g_img_play;
static GYimg g_img_pause;

static void buildPlayIcons(void)
{
	GYpx keypx = GY_ColorToPx(GY_ARGB(0xFF, 0xFF, 0x00, 0xFF));
	GYpx fgpx  = GY_ColorToPx(GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0));
	int x, y;
	for (y = 0; y < VP_ICON; y++)
	{
		for (x = 0; x < VP_ICON; x++)
		{
			int idx = y * VP_ICON + x;
			int half = (22 - x); //三角尖朝右=播放
			int on_play = (x >= 6 && x <= 22 && (y - VP_ICON / 2) <= half
			               && (VP_ICON / 2 - y) <= half);
			g_play_px[idx] = on_play ? fgpx : keypx;
			int on_pause = (((x >= 7 && x <= 11) || (x >= 17 && x <= 21)) && y >= 6 && y <= 21);
			g_pause_px[idx] = on_pause ? fgpx : keypx;
		}
	}
	g_img_play.data  = g_play_px;  g_img_play.w  = VP_ICON; g_img_play.h  = VP_ICON;
	g_img_play.use_key = 1;        g_img_play.key = keypx;
	g_img_pause.data = g_pause_px; g_img_pause.w = VP_ICON; g_img_pause.h = VP_ICON;
	g_img_pause.use_key = 1;       g_img_pause.key = keypx;
}

static void updatePlayIcon(void)
{
	YMGUI_Button_SetImage(g_btn_play,
		mp_audio_is_playing(g_audio) ? &g_img_pause : &g_img_play);
}

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
	if (mp_audio_is_finished(g_audio)) //播完再点从头
	{
		mp_audio_seek_ms(g_audio, 0);
		vp_video_seek_ms(g_video, 0);
	}
	mp_audio_toggle(g_audio);
	updatePlayIcon();
}

static void onSeek(GYOBJ sld, int32 value)
{
	(void)sld;
	//进度条 0..1000 -> 毫秒。音频 seek 是 O(1)(改帧游标+清队列),即时做,给听觉反馈。
	//视频 seek 要重启 ffmpeg 管道(closePipe+popen,pclose 还阻塞等子进程),是重活——
	//拖动中每次值变都触发会堆几十次重启卡死。故拖动中只记目标,松手(主循环检测)才做一次。
	int32 dur = mp_audio_dur_ms(g_audio);
	if (dur <= 0) dur = vp_video_dur_ms(g_video);
	int32 ms = (int32)((int64)value * dur / 1000);
	mp_audio_seek_ms(g_audio, ms);
	if (g_user_dragging)
		g_pending_seek = ms;       //拖动中:推迟视频 seek
	else
		vp_video_seek_ms(g_video, ms); //单击(非拖动):直接 seek 视频
}

//每帧:把音频主时钟同步到进度条 / 时间标签
static void syncClock(void)
{
	int32 pos = mp_audio_pos_ms(g_audio);
	int32 dur = mp_audio_dur_ms(g_audio);
	if (dur <= 0) dur = vp_video_dur_ms(g_video);

	if (!g_user_dragging && dur > 0)
		YMGUI_Slider_SetValue(g_slider, (int32)((int64)pos * 1000 / dur));

	char tc[16], td[16];
	fmtTime(tc, sizeof(tc), pos);
	fmtTime(td, sizeof(td), dur);
	YMGUI_Label_SetText(g_lbl_cur, tc);
	YMGUI_Label_SetText(g_lbl_dur, td);
}

//===========================================================================
// selftest —— headless 验证核心逻辑(不依赖真实声卡/图形/ffmpeg)
//===========================================================================
#include "YMGUI_DrawImg.h" //Draw_ImgScaled(缩放图元)自检
static void selftest(void)
{
	int fails = 0;

	//1) 缩放 blit 图元:2x2(四象限 1/2/3/4)放大到 4x4,角落应映射到对应象限色
	{
		GYpx src[4] = { 10, 20, 30, 40 }; //TL,TR,BL,BR
		GYimg img = { src, 2, 2, 0, 0 };
		GYpx dstpx[16];
		memset(dstpx, 0, sizeof(dstpx));
		GYsurface s;
		s.buf = dstpx; s.stride = 4;
		s.buf_area = (GYrect){ 0, 0, 4, 4 };
		s.clip     = (GYrect){ 0, 0, 4, 4 };
		YMGUI_Draw_ImgScaled(&s, &img, (GYrect){ 0, 0, 4, 4 });
		if (dstpx[0] != 10)  { gy_log_print("selftest FAIL: scale TL=%d want 10\n", dstpx[0]); fails++; }
		if (dstpx[3] != 20)  { gy_log_print("selftest FAIL: scale TR=%d want 20\n", dstpx[3]); fails++; }
		if (dstpx[12] != 30) { gy_log_print("selftest FAIL: scale BL=%d want 30\n", dstpx[12]); fails++; }
		if (dstpx[15] != 40) { gy_log_print("selftest FAIL: scale BR=%d want 40\n", dstpx[15]); fails++; }
	}

	//2) 视频引擎:兜底/真解码都应给出正尺寸缓冲 + update 出帧
	{
		VPvideo* v = vp_video_create();
		if (v == NULL) { gy_log_print("selftest FAIL: video create\n"); fails++; }
		else
		{
			//传 NULL 强制走合成兜底(不依赖 ffmpeg,自检恒可跑)
			vp_video_open(v, NULL, VID_MAX_W, VID_MAX_H);
			if (vp_video_width(v) <= 0 || vp_video_height(v) <= 0)
			{ gy_log_print("selftest FAIL: video size %dx%d\n", vp_video_width(v), vp_video_height(v)); fails++; }
			if (vp_video_pixels(v) == NULL) { gy_log_print("selftest FAIL: video pixels null\n"); fails++; }
			//t=0 应出首帧
			if (!vp_video_update(v, 0)) { gy_log_print("selftest FAIL: video no frame @0\n"); fails++; }
			//A/V 同步:不同时钟位置应得不同帧内容(动画在动)
			GYpx a = vp_video_pixels(v)[100];
			vp_video_update(v, 500);
			GYpx b = vp_video_pixels(v)[100];
			if (a == b) { gy_log_print("selftest WARN: synth frame not advancing (a=b=%d)\n", a); }
			//seek 越界钳制到时长
			vp_video_seek_ms(v, 999999);
			//同一时钟不应重复出帧(免重绘)
			vp_video_update(v, 4000);
			if (vp_video_update(v, 4000)) { gy_log_print("selftest FAIL: video redraw same ts\n"); fails++; }
			vp_video_destroy(v);
		}
	}

	//3) 音频引擎:合成兜底 + seek + 位置(A/V 主钟来源)
	{
		MPaudio* a = mp_audio_create();
		if (a == NULL) { gy_log_print("selftest FAIL: audio create\n"); fails++; }
		else
		{
			mp_audio_load_synth(a, 5);
			int32 dur = mp_audio_dur_ms(a);
			if (dur < 4800 || dur > 5200) { gy_log_print("selftest FAIL: synth dur=%d\n", dur); fails++; }
			mp_audio_seek_ms(a, 2000);
			int32 p = mp_audio_pos_ms(a);
			if (p < 1900 || p > 2100) { gy_log_print("selftest FAIL: seek pos=%d want ~2000\n", p); fails++; }
			mp_audio_destroy(a);
		}
	}

	if (fails == 0)
		gy_log_print("selftest: video decode + scale + A/V sync OK\n");
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
	//argv[1]=帧数上限(headless 自检);argv[2]=视频文件(默认内置 卖瓜.mp4)
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	const char* video_path = (argc > 2) ? argv[2] : VP_DEFAULT_VIDEO;
	int frame = 0;

	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;

	SDL_LCD_Init(&disp, 1);
	g_ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(g_ctx->root, GY_ARGB(0xFF, 0x0A, 0x0B, 0x10));

	//GB2312 全字库回退
	s_blob = fopen(GB2312_BIN_PATH, "rb");
	if (s_blob != NULL) { s_gb_font.glyph_count = YMGUI_GB2312_glyph_count; YMGUI_Font_SetFallback(&s_gb_font); }
	else gy_log_print("warn: gb2312 blob not found, CJK limited to built-in glyphs\n");

	//标题
	GYOBJ title = YMGUI_Creat_Label_Creat(g_ctx->root, 16, 8, SCR_W - 32, 18);
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0x50, 0xC0, 0xF0));
	YMGUI_Label_SetText(title, "video_player —— 视频播放器(Image FIT + Draw_ImgScaled 缩放图元)");

	//---- 视频窗:Image 处于 FIT 模式,黑底看清 letterbox ----
	g_video_pane = YMGUI_Creat_Image_Creat(g_ctx->root, 16, 32, SCR_W - 32, 380);
	YMGUI_Obj_SetBgColor(g_video_pane, GY_ARGB(0xFF, 0x00, 0x00, 0x00));
	YMGUI_Image_SetScaleMode(g_video_pane, GY_IMG_FIT);

	//---- 进度条 + 时间 ----
	g_lbl_cur = YMGUI_Creat_Label_Creat(g_ctx->root, 16, 420, 50, 18);
	YMGUI_Label_SetTextColor(g_lbl_cur, GY_ARGB(0xFF, 0xC0, 0xC4, 0xD0));
	YMGUI_Label_SetText(g_lbl_cur, "0:00");

	g_slider = YMGUI_Creat_Slider_Creat(g_ctx->root, 72, 422, SCR_W - 144, 16);
	YMGUI_Slider_SetRange(g_slider, 0, 1000);
	YMGUI_Slider_SetValue(g_slider, 0);
	YMGUI_Slider_SetChanged(g_slider, onSeek);

	g_lbl_dur = YMGUI_Creat_Label_Creat(g_ctx->root, SCR_W - 66, 420, 50, 18);
	YMGUI_Label_SetTextColor(g_lbl_dur, GY_ARGB(0xFF, 0xC0, 0xC4, 0xD0));
	YMGUI_Label_SetText(g_lbl_dur, "0:00");

	//---- 播放/暂停按钮 ----
	buildPlayIcons();
	g_btn_play = YMGUI_Creat_Button_Creat(g_ctx->root, SCR_W / 2 - 22, 448, 44, 28);
	YMGUI_Button_SetImage(g_btn_play, &g_img_play);
	YMGUI_Button_SetClicked(g_btn_play, onPlayPause);

	//状态栏
	g_lbl_stat = YMGUI_Creat_Label_Creat(g_ctx->root, 16, SCR_H - 14, SCR_W - 32, 12);
	YMGUI_Label_SetTextColor(g_lbl_stat, GY_ARGB(0xFF, 0x70, 0x74, 0x84));

	//---- 引擎:视频解码 + 音频(同一文件,音轨归 mp_audio,视频轨归 vp_video) ----
	g_video = vp_video_create();
	uint8 vreal = 0;
	if (g_video) vreal = vp_video_open(g_video, video_path, VID_MAX_W, VID_MAX_H);
	//视频帧 → GYimg → Image(FIT)。data 指向稳定 display_buf,后续原地更新
	g_frame_img.data = vp_video_pixels(g_video);
	g_frame_img.w    = vp_video_width(g_video);
	g_frame_img.h    = vp_video_height(g_video);
	g_frame_img.use_key = 0; g_frame_img.key = 0;
	YMGUI_Image_SetSrc(g_video_pane, &g_frame_img);

	g_audio = mp_audio_create();
	uint8 areal = 0;
	if (g_audio) areal = mp_audio_load(g_audio, video_path); //从同一文件抽音轨

	{
		char st[160];
		snprintf(st, sizeof(st), "视频:%s  |  音频:%s  |  拖动进度条跳转,点按钮播放/暂停",
		         vreal ? "ffmpeg 解码" : "合成动画兜底",
		         areal ? (mp_audio_is_silent(g_audio) ? "已解码(静音兜底)" : "已解码+声卡")
		               : "合成旋律兜底");
		YMGUI_Label_SetText(g_lbl_stat, st);
	}

	YMGUI_Inject_SetCtx(g_ctx);

	if (max_frames > 0)
		selftest(); //headless 自检(ffmpeg 无关);随后仍开播,让帧限跑也走真实解码+同步冒烟
	mp_audio_play(g_audio); //开播(主钟推进)
	vp_video_update(g_video, 0); //先出首帧
	updatePlayIcon();

	int32 frames_shown = 0; //冒烟统计:实际推进的视频帧数
	while (SDL_LCD_PumpEvents())
	{
		g_user_dragging = (g_slider->state & GY_STATE_Pressed) ? 1 : 0;
		if (g_user_dragging && g_pending_seek >= 0)
		{
			//拖动中节流预览:每 VP_PREVIEW_THROTTLE_MS 才重启一次 ffmpeg 让画面粗略跟随
			uint32 now = SDL_GetTicks();
			if (now - g_last_preview_ms >= VP_PREVIEW_THROTTLE_MS)
			{
				vp_video_seek_ms(g_video, g_pending_seek);
				g_last_preview_ms = now;
				g_pending_seek = -1; //已消费;下次值变再置新目标
			}
		}
		//松手边沿(拖动结束):把最后攒下的目标精确 seek 一次(节流可能漏掉最终落点)
		if (g_prev_dragging && !g_user_dragging && g_pending_seek >= 0)
		{
			vp_video_seek_ms(g_video, g_pending_seek);
			g_pending_seek = -1;
		}
		g_prev_dragging = g_user_dragging;

		mp_audio_tick(g_audio);
		int32 pos = mp_audio_pos_ms(g_audio); //音频主钟
		vp_video_read_tick(g_video);          //补读解码帧
		if (vp_video_update(g_video, pos))    //按主钟选帧 → display_buf
		{
			YMGUI_Obj_Invalidate(g_video_pane); //帧变了才重绘视频窗
			frames_shown++;
		}
		syncClock();

		//播完自动复位
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

	if (g_video) vp_video_destroy(g_video);
	if (g_audio) mp_audio_destroy(g_audio);
	YMGUI_Free_CtxFree(g_ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	if (s_blob != NULL) fclose(s_blob);
	if (max_frames > 0)
		gy_log_print("smoke: %d video frames shown over %d ticks\n", frames_shown, frame);
	gy_log_print("video_player exit ok\n");
	return g_selftest_fail ? 1 : 0;
}
