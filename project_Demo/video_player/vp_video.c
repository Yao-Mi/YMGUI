#ifndef _GNU_SOURCE
#define _GNU_SOURCE //F_SETPIPE_SZ / F_GETPIPE_SZ 需要(须在任何 include 前)
#endif
#include "vp_video.h"
#include "YMGUI_PubDefine.h" //GY_ARGB
#include "YMGUI_DrawPx.h"    //GY_ColorToPx
#include "YMGUI_Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <SDL2/SDL.h> //仅用 SDL_GetTicks 给 synth 兜底时钟(SDL 已由 SDL_LCD 起)

/**
  ***************************************************************************************************************************
  *	@FileName:    vp_video.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 视频解码引擎(app 侧,视频语义全在此)。ffprobe 探尺寸/帧率/时长;ffmpeg(popen)流式
  *	              解码成 rgb565le 裸帧(小端 = GYpx 零转换),缩放到 <= max_w×max_h(保比偶数)。管道 fd
  *	              置 O_NONBLOCK,单线程非阻塞读入帧环形缓冲(满则停读→ffmpeg 写阻塞形成背压)。vp_video_update
  *	              按音频主时钟选"当前应显示帧"拷进稳定 display_buf(丢过期/持未来)。无 ffmpeg/ffprobe/不可解
  *	              → 合成动画兜底。ffmpeg/SDL/解码只在 app 侧,GUI 库零音视频依赖。
  *	@Version:     1.0
  ***************************************************************************************************************************/

#define VP_RING 12 //帧环形缓冲槽数(预缓冲深度 & 背压阈值)

#if YMGUI_COLOR_DEPTH == 24
#define VP_FFMPEG_PIXFMT "rgb24"
#else
#define VP_FFMPEG_PIXFMT "rgb565le"
#endif

typedef struct
{
	GYpx* px;      //帧像素(out_w*out_h)
	int32 pts_ms;  //呈现时间戳(毫秒)
	uint8 valid;   //1=装有已解码帧
}VPframe;

struct VPvideo
{
	int32 out_w, out_h;   //输出帧尺寸(display_buf 尺寸)
	int32 dur_ms;         //总时长
	int32 fps_num, fps_den;

	FILE* pipe;           //ffmpeg popen
	int   fd;             //管道读端 fd(非阻塞)
	char  path[1024];     //源路径(seek 重启用)

	uint8* fillbuf;       //当前帧累积区(out_w*out_h*2 字节)
	int64  frame_bytes;   //一帧字节数(out_w*out_h*2)
	int64  fill_bytes;    //fillbuf 已填字节
	int64  pipe_frame_i;  //本管道自启动起已读出的完整帧数
	int32  pipe_base_ms;  //本管道对应的起始时间(seek 目标)

	VPframe ring[VP_RING];
	int32  ring_head;     //最旧有效帧槽
	int32  ring_count;    //有效帧数

	GYpx*  display_buf;   //稳定输出缓冲(app 的 GYimg 指向它)
	int32  last_shown_pts;//display_buf 现帧 pts(-1=无)
	int32  last_ms;       //上次 update 的 now_ms

	uint8  eof;           //管道读到 EOF
	uint8  is_synth;      //合成兜底
};

//把 src 里的单引号转义为 '\'' 序列写入 dst(防命令断裂),dst 至少 4*len+1
static void escSingle(const char* src, char* dst, int32 dstsz)
{
	int32 k = 0, j = 0;
	while (src[k] != '\0' && j < dstsz - 5)
	{
		if (src[k] == '\'') { dst[j++]='\''; dst[j++]='\\'; dst[j++]='\''; dst[j++]='\''; }
		else dst[j++] = src[k];
		k++;
	}
	dst[j] = '\0';
}

//---------------------------------------------------------------------------
// 缓冲管理
//---------------------------------------------------------------------------
static void freeRing(VPvideo* v)
{
	int i;
	for (i = 0; i < VP_RING; i++)
	{
		if (v->ring[i].px) { free(v->ring[i].px); v->ring[i].px = NULL; }
		v->ring[i].valid = 0;
	}
	v->ring_head = v->ring_count = 0;
}

//分配所有随尺寸变化的缓冲(display_buf / fillbuf / ring 各槽)
static uint8 allocBuffers(VPvideo* v)
{
	int i;
	int64 npx = (int64)v->out_w * v->out_h;
	v->frame_bytes = npx * (int64)sizeof(GYpx); //rgb565: 2 字节/px
	v->display_buf = (GYpx*)calloc((size_t)npx, sizeof(GYpx));
	v->fillbuf     = (uint8*)malloc((size_t)v->frame_bytes);
	if (v->display_buf == NULL || v->fillbuf == NULL) return 0;
	for (i = 0; i < VP_RING; i++)
	{
		v->ring[i].px = (GYpx*)malloc((size_t)npx * sizeof(GYpx));
		if (v->ring[i].px == NULL) return 0;
		v->ring[i].valid = 0;
	}
	return 1;
}

//关闭当前 ffmpeg 管道(不动缓冲)
static void closePipe(VPvideo* v)
{
	if (v->pipe) { pclose(v->pipe); v->pipe = NULL; }
	v->fd = -1;
	v->fill_bytes = 0;
	v->pipe_frame_i = 0;
	v->eof = 0;
}

//---------------------------------------------------------------------------
// ffprobe:探 宽/高/帧率/时长。成功返回 1
//---------------------------------------------------------------------------
static uint8 probe(const char* path, int32* w, int32* h,
                   int32* fps_num, int32* fps_den, int32* dur_ms)
{
	char esc[1024]; escSingle(path, esc, (int32)sizeof(esc));
	char cmd[2048];
	snprintf(cmd, sizeof(cmd),
	         "ffprobe -v error -select_streams v:0 "
	         "-show_entries stream=width,height,r_frame_rate "
	         "-show_entries format=duration "
	         "-of default=noprint_wrappers=1 '%s' 2>/dev/null", esc);
	FILE* p = popen(cmd, "r");
	if (p == NULL) return 0;
	int gw = 0, gh = 0, fn = 0, fd = 0; double dur = 0.0;
	char line[256];
	while (fgets(line, sizeof(line), p))
	{
		if      (sscanf(line, "width=%d", &gw) == 1) {}
		else if (sscanf(line, "height=%d", &gh) == 1) {}
		else if (sscanf(line, "r_frame_rate=%d/%d", &fn, &fd) == 2) {}
		else if (sscanf(line, "duration=%lf", &dur) == 1) {}
	}
	int rc = pclose(p);
	if (rc != 0 || gw <= 0 || gh <= 0) return 0;
	if (fn <= 0 || fd <= 0) { fn = 25; fd = 1; } //帧率探不到 → 25fps
	*w = gw; *h = gh; *fps_num = fn; *fps_den = fd;
	*dur_ms = (dur > 0.0) ? (int32)(dur * 1000.0 + 0.5) : 0;
	return 1;
}

//按 max 盒等比缩放源尺寸到偶数输出尺寸
static void fitSize(int32 sw, int32 sh, int32 mw, int32 mh, int32* ow, int32* oh)
{
	if (sw <= 0 || sh <= 0) { *ow = mw; *oh = mh; }
	else if (sw <= mw && sh <= mh) { *ow = sw; *oh = sh; }        //小于盒:原尺寸
	else if ((int64)sw * mh <= (int64)sh * mw)                     //高受限
	{ *oh = mh; *ow = (int32)((int64)sw * mh / sh); }
	else                                                           //宽受限
	{ *ow = mw; *oh = (int32)((int64)sh * mw / sw); }
	if (*ow < 2) *ow = 2; if (*oh < 2) *oh = 2;
	*ow &= ~1; *oh &= ~1; //偶数(部分滤镜/编码器友好)
}

//启动 ffmpeg 管道:从 start_ms 起,scale 到 out_w×out_h,rgb565le 裸流到 stdout,fd 置非阻塞
static uint8 startPipe(VPvideo* v, int32 start_ms)
{
	char esc[1024]; escSingle(v->path, esc, (int32)sizeof(esc));
	char cmd[2560];
	//-ss 放 -i 前 = 关键帧快速定位(输出时间从 0 起,故 pts 基准由 pipe_base_ms 记)
	if (start_ms > 0)
		snprintf(cmd, sizeof(cmd),
		         "ffmpeg -nostdin -v error -ss %d.%03d -i '%s' -an -vf scale=%d:%d "
		         "-f rawvideo -pix_fmt " VP_FFMPEG_PIXFMT " - 2>/dev/null",
		         start_ms / 1000, start_ms % 1000, esc, v->out_w, v->out_h);
	else
		snprintf(cmd, sizeof(cmd),
		         "ffmpeg -nostdin -v error -i '%s' -an -vf scale=%d:%d "
		         "-f rawvideo -pix_fmt " VP_FFMPEG_PIXFMT " - 2>/dev/null",
		         esc, v->out_w, v->out_h);
	v->pipe = popen(cmd, "r");
	if (v->pipe == NULL) return 0;
	v->fd = fileno(v->pipe);
	int fl = fcntl(v->fd, F_GETFL, 0);
	if (fl != -1) fcntl(v->fd, F_SETFL, fl | O_NONBLOCK);
	//放大管道缓冲:默认仅 64KB,一帧就 460KB,导致每 tick 只能抽约 1/7 帧、吞吐锁死在
	//~9fps。想放到约 4 帧,让 ffmpeg 在两次 read_tick 之间预缓冲数帧。注意:请求超过
	///proc/sys/fs/pipe-max-size 时非特权进程会**直接失败**(不是钳位),故从大到小试、
	//取第一个成功的;全失败就退回默认(只是慢些,不影响正确性)。
#ifdef F_SETPIPE_SZ
	{
		long want = (long)v->frame_bytes * 4;
		if (want < 1 << 20) want = 1 << 20; //至少 1MB
		while (want >= 128 * 1024)
		{
			if (fcntl(v->fd, F_SETPIPE_SZ, (int)want) >= 0) break;
			want /= 2; //超上限/失败:减半再试
		}
		gy_log_print("vp_video: pipe size set -> %d (frame=%lld)\n",
		             fcntl(v->fd, F_GETPIPE_SZ), (long long)v->frame_bytes);
	}
#else
	gy_log_print("vp_video: F_SETPIPE_SZ not available\n");
#endif
	v->fill_bytes = 0;
	v->pipe_frame_i = 0;
	v->pipe_base_ms = start_ms;
	v->eof = 0;
	return 1;
}

//---------------------------------------------------------------------------
// 生命周期
//---------------------------------------------------------------------------
VPvideo* vp_video_create(void)
{
	VPvideo* v = (VPvideo*)calloc(1, sizeof(VPvideo));
	if (v == NULL) return NULL;
	v->fd = -1;
	v->fps_num = 25; v->fps_den = 1;
	v->last_shown_pts = -1;
	return v;
}

void vp_video_destroy(VPvideo* v)
{
	if (v == NULL) return;
	closePipe(v);
	freeRing(v);
	if (v->display_buf) free(v->display_buf);
	if (v->fillbuf)     free(v->fillbuf);
	free(v);
}

//合成兜底:一块可动画的测试图(update 时按时钟原地生成)。dur 默认 30s
static uint8 openSynth(VPvideo* v, int32 max_w, int32 max_h)
{
	v->is_synth = 1;
	fitSize(640, 360, max_w, max_h, &v->out_w, &v->out_h); //16:9 兜底画布
	v->fps_num = 25; v->fps_den = 1;
	v->dur_ms = 30000;
	if (!allocBuffers(v)) return 0;
	v->last_shown_pts = -1;
	gy_log_print("vp_video: synth fallback %dx%d\n", v->out_w, v->out_h);
	return 1;
}

uint8 vp_video_open(VPvideo* v, const char* path, int32 max_w, int32 max_h)
{
	if (v == NULL) return 0;
	if (max_w < 2) max_w = 2; if (max_h < 2) max_h = 2;

	if (path == NULL || path[0] == '\0') { openSynth(v, max_w, max_h); return 0; }
	//文件在不在
	FILE* pf = fopen(path, "rb");
	if (pf == NULL) { gy_log_print("vp_video: file not found '%s' -> synth\n", path); openSynth(v, max_w, max_h); return 0; }
	fclose(pf);

	int32 sw = 0, sh = 0, fn = 25, fd = 1, dms = 0;
	if (!probe(path, &sw, &sh, &fn, &fd, &dms))
	{ gy_log_print("vp_video: ffprobe unavailable/failed -> synth\n"); openSynth(v, max_w, max_h); return 0; }

	fitSize(sw, sh, max_w, max_h, &v->out_w, &v->out_h);
	v->fps_num = fn; v->fps_den = fd;
	v->dur_ms  = dms;
	snprintf(v->path, sizeof(v->path), "%s", path);
	if (!allocBuffers(v)) { gy_log_print("vp_video: OOM -> synth\n"); freeRing(v); openSynth(v, max_w, max_h); return 0; }

	if (!startPipe(v, 0))
	{ gy_log_print("vp_video: ffmpeg popen failed -> synth\n"); openSynth(v, max_w, max_h); return 0; }

	gy_log_print("vp_video: opened '%s' src %dx%d -> out %dx%d @ %d/%d fps, %d ms\n",
	             path, sw, sh, v->out_w, v->out_h, fn, fd, dms);
	return 1;
}

int32 vp_video_width (const VPvideo* v) { return v ? v->out_w : 0; }
int32 vp_video_height(const VPvideo* v) { return v ? v->out_h : 0; }
int32 vp_video_dur_ms(const VPvideo* v) { return v ? v->dur_ms : 0; }
uint8 vp_video_is_synth(const VPvideo* v){ return v ? v->is_synth : 0; }
const GYpx* vp_video_pixels(const VPvideo* v) { return v ? v->display_buf : NULL; }

//管道帧序号 -> pts(ms):base + i * fps_den*1000 / fps_num
static int32 frameToMs(const VPvideo* v, int64 i)
{
	return v->pipe_base_ms + (int32)(i * (int64)v->fps_den * 1000 / v->fps_num);
}

//把 fillbuf 里攒满的一整帧提交进环形缓冲(满则丢最旧——update 会追时钟,预读多的直接覆盖)
static void pushFrame(VPvideo* v)
{
	int32 slot;
	if (v->ring_count < VP_RING)
	{
		slot = (v->ring_head + v->ring_count) % VP_RING;
		v->ring_count++;
	}
	else
	{
		//满:覆盖最旧(正常不会到,背压会先停读;seek 抖动时的兜底)
		slot = v->ring_head;
		v->ring_head = (v->ring_head + 1) % VP_RING;
	}
	memcpy(v->ring[slot].px, v->fillbuf, (size_t)v->frame_bytes);
	v->ring[slot].pts_ms = frameToMs(v, v->pipe_frame_i);
	v->ring[slot].valid = 1;
	v->pipe_frame_i++;
}

void vp_video_read_tick(VPvideo* v)
{
	if (v == NULL || v->is_synth || v->pipe == NULL || v->fd < 0) return;
	//环形缓冲满 → 停读(背压:ffmpeg 写管道阻塞,自然限速)
	//每 tick 最多补读到把 ring 填满,避免一次 tick 卡太久
	while (v->ring_count < VP_RING)
	{
		int64 need = v->frame_bytes - v->fill_bytes;
		ssize_t n = read(v->fd, v->fillbuf + v->fill_bytes, (size_t)need);
		if (n > 0)
		{
			v->fill_bytes += n;
			if (v->fill_bytes >= v->frame_bytes) //攒满一帧
			{
				pushFrame(v);
				v->fill_bytes = 0;
			}
		}
		else if (n == 0) { v->eof = 1; break; } //管道关闭 = 解码结束
		else //n<0
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK) break; //暂无数据,下 tick 再来
			v->eof = 1; break; //真错误,当结束
		}
	}
}

//合成动画:一块随 now_ms 变化的渐变 + 移动竖条测试图,原地写 display_buf
static void renderSynth(VPvideo* v, int32 now_ms)
{
	int32 w = v->out_w, h = v->out_h;
	int32 t = now_ms / 16; //相位
	int32 bar = (now_ms / 8) % (w > 0 ? w : 1); //移动竖条位置
	int32 x, y;
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			int32 r = (x * 255 / (w > 1 ? w - 1 : 1));
			int32 g = (y * 255 / (h > 1 ? h - 1 : 1));
			int32 b = (t + x + y) & 0xFF;
			int32 near = x - bar; if (near < 0) near = -near;
			if (near < 3) { r = g = b = 0xFF; } //竖条画白
			v->display_buf[y * w + x] = GY_ColorToPx(GY_ARGB(0xFF, r, g, b));
		}
	}
}

uint8 vp_video_update(VPvideo* v, int32 now_ms)
{
	if (v == NULL) return 0;
	if (v->is_synth)
	{
		//每 ~33ms 更新一帧(约 30fps),避免每帧全绘
		if (v->last_shown_pts >= 0 && now_ms / 33 == v->last_shown_pts / 33
		    && v->last_ms <= now_ms)
		{ v->last_ms = now_ms; return 0; }
		renderSynth(v, now_ms);
		v->last_shown_pts = now_ms;
		v->last_ms = now_ms;
		return 1;
	}

	//REAL:丢掉所有 pts <= now 的旧帧,只留最新一张 <= now 显示(追时钟);
	//若最旧帧 pts 都 > now(视频超前),则保持当前显示。
	int32 best = -1; //选中槽
	while (v->ring_count > 0)
	{
		int32 head = v->ring_head;
		if (v->ring[head].pts_ms <= now_ms)
		{
			//此帧到期:成为候选,弹出继续看下一张是否也到期
			best = head;
			v->ring_head = (v->ring_head + 1) % VP_RING;
			v->ring_count--;
			//若下一张也 <= now,当前候选就是"过期帧",继续丢
			if (v->ring_count > 0 && v->ring[v->ring_head].pts_ms <= now_ms)
				continue;
			break;
		}
		else break; //最旧帧还在未来,未到显示时刻
	}
	if (best < 0) return 0; //无到期帧:保持
	if (v->ring[best].pts_ms == v->last_shown_pts) return 0; //同一帧:免重绘
	memcpy(v->display_buf, v->ring[best].px, (size_t)v->frame_bytes);
	v->last_shown_pts = v->ring[best].pts_ms;
	return 1;
}

void vp_video_seek_ms(VPvideo* v, int32 ms)
{
	if (v == NULL) return;
	if (ms < 0) ms = 0;
	if (v->dur_ms > 0 && ms > v->dur_ms) ms = v->dur_ms;
	v->last_shown_pts = -1;
	if (v->is_synth)
	{
		v->last_ms = ms;
		renderSynth(v, ms);
		v->last_shown_pts = ms;
		return;
	}
	//REAL:清环形缓冲 + 以 -ss 重启管道
	closePipe(v);
	freeRing(v);
	//freeRing 释放了 ring[].px,重新分配
	{
		int i; int64 npx = (int64)v->out_w * v->out_h;
		for (i = 0; i < VP_RING; i++)
		{
			v->ring[i].px = (GYpx*)malloc((size_t)npx * sizeof(GYpx));
			v->ring[i].valid = 0;
		}
	}
	startPipe(v, ms);
}

uint8 vp_video_is_eof(const VPvideo* v)
{
	if (v == NULL) return 0;
	if (v->is_synth) return v->last_ms >= v->dur_ms;
	return v->eof && v->ring_count == 0 && v->fill_bytes == 0;
}
