#include "mp_audio.h"
#include "YMGUI_Trig.h"
#include "YMGUI_Debug.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    mp_audio.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 播放引擎(app 侧,音乐语义全在此)。ffmpeg(popen)整曲解码成 44100/立体声/s16le
  *	              存内存;SDL2 声卡输出(SDL_QueueAudio);开设备失败走静音兜底(wall-clock 推进)。
  *	              seek/进度/频谱都在整曲缓冲上做。SDL 音频只出现在这个文件 —— GUI 库零音频依赖。
  *	@Version:     1.0
  ***************************************************************************************************************************/

//一帧 = 每声道一个样本(立体声 = 2 个 int16)
#define FRAME_INT16 MP_AUD_CHANNELS

struct MPaudio
{
	int16*  pcm;        //交织 PCM(帧*声道 个 int16)
	int64   frames;     //总帧数(每声道样本数)
	int64   cap_frames; //缓冲容量(帧)

	int64   cur;        //已"推送"到声卡的帧游标(下一个要送的帧)
	uint8   playing;    //1=播放中
	uint8   silent;     //1=没开成声卡,靠 wall-clock
	uint8   is_synth;   //1=当前是合成兜底旋律

	SDL_AudioDeviceID dev;
	uint32  audio_inited; //1=本引擎 init 了 AUDIO 子系统(销毁时要 quit)

	//静音兜底时钟:play 时记基准
	uint32  wall_base_ms;   //SDL_GetTicks at last play/seek
	int64   wall_base_frame;//cur 帧数 at last play/seek
};

//---------------------------------------------------------------------------
// 缓冲管理
//---------------------------------------------------------------------------
static void freePcm(MPaudio* a)
{
	if (a->pcm) { free(a->pcm); a->pcm = NULL; }
	a->frames = a->cap_frames = 0;
	a->cur = 0;
}

//确保容量至少 need 帧(几何增长)
static uint8 ensureCap(MPaudio* a, int64 need_frames)
{
	if (need_frames <= a->cap_frames) return 1;
	int64 nc = (a->cap_frames > 0) ? a->cap_frames : (int64)MP_AUD_RATE; //起步 1s
	while (nc < need_frames) nc *= 2;
	int16* np = (int16*)realloc(a->pcm, (size_t)nc * FRAME_INT16 * sizeof(int16));
	if (np == NULL) return 0;
	a->pcm = np;
	a->cap_frames = nc;
	return 1;
}

MPaudio* mp_audio_create(void)
{
	MPaudio* a = (MPaudio*)calloc(1, sizeof(MPaudio));
	if (a == NULL) return NULL;
	//确保 AUDIO 子系统已起(SDL_LCD 只起了 VIDEO)
	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
			a->audio_inited = 1;
	}
	a->dev = 0;
	a->silent = 1; //装载时再尝试开设备
	return a;
}

void mp_audio_destroy(MPaudio* a)
{
	if (a == NULL) return;
	if (a->dev) { SDL_CloseAudioDevice(a->dev); a->dev = 0; }
	if (a->audio_inited) SDL_QuitSubSystem(SDL_INIT_AUDIO);
	freePcm(a);
	free(a);
}

//尝试开声卡(44100/立体声/s16)。成功 dev!=0 且 silent=0
static void openDevice(MPaudio* a)
{
	if (a->dev) return;
	if (!SDL_WasInit(SDL_INIT_AUDIO)) { a->silent = 1; return; }
	SDL_AudioSpec want, have;
	SDL_memset(&want, 0, sizeof(want));
	want.freq = MP_AUD_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = MP_AUD_CHANNELS;
	want.samples = 1024;
	want.callback = NULL; //用 queue 模式
	a->dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (a->dev == 0)
	{
		gy_log_print("mp_audio: no sound device (%s) -> silent playback\n", SDL_GetError());
		a->silent = 1;
	}
	else
	{
		a->silent = 0;
		SDL_PauseAudioDevice(a->dev, 1); //先暂停,play 时开
	}
}

//---------------------------------------------------------------------------
// 合成兜底旋律(纯整数,库 Q15 三角):一段简单音阶琶音 + 轻微包络
//---------------------------------------------------------------------------
uint8 mp_audio_load_synth(MPaudio* a, int32 seconds)
{
	if (a == NULL) return 0;
	if (seconds <= 0) seconds = 20;
	int64 nf = (int64)seconds * MP_AUD_RATE;
	freePcm(a);
	if (!ensureCap(a, nf)) return 0;

	//C 大调音阶(半音相对 A4=440 的频率,整数 Hz 近似)
	static const int32 notes[] = { 262, 294, 330, 349, 392, 440, 494, 523 };
	const int32 nnotes = (int32)(sizeof(notes) / sizeof(notes[0]));
	int32 note_ms = 320;                       //每音时值
	int64 note_frames = (int64)note_ms * MP_AUD_RATE / 1000;

	int64 i;
	for (i = 0; i < nf; i++)
	{
		int64 ni = (i / note_frames) % nnotes;  //当前音索引
		int64 into = i % note_frames;            //音内位置
		int32 freq = notes[ni];
		//相位(度):phase_deg = 360 * freq * i / rate,取模避免溢出
		int32 deg = (int32)(((int64)360 * freq * i / MP_AUD_RATE) % 360);
		int32 s = GY_Sin(deg);                   //Q15 [-32768,32767]
		//音内简单包络:起音快、尾部衰减(三角窗近似)
		int32 env = 32767;
		int64 half = note_frames / 2;
		if (into < half) env = (int32)(32767 * into / (half > 0 ? half : 1));
		else             env = (int32)(32767 * (note_frames - into) / (half > 0 ? half : 1));
		//叠一个低八度让声音厚一点
		int32 deg2 = (int32)(((int64)360 * (freq / 2) * i / MP_AUD_RATE) % 360);
		int32 s2 = GY_Sin(deg2);
		int32 mix = (s * 3 + s2) / 4;            //主音重、低八度轻
		//幅度:mix(Q15) * env(Q15) >> 15,再压到约 1/3 满幅避免刺耳
		int32 val = (int32)(((int64)mix * env) >> 15);
		val = val / 3;
		if (val > 32767) val = 32767; if (val < -32768) val = -32768;
		a->pcm[i * FRAME_INT16 + 0] = (int16)val;
		a->pcm[i * FRAME_INT16 + 1] = (int16)val;
	}
	a->frames = nf;
	a->cur = 0;
	a->is_synth = 1;
	openDevice(a);
	return 1;
}

//---------------------------------------------------------------------------
// ffmpeg 整曲解码(popen):任意音频 -> 44100/立体声/s16le 裸流,读进内存
//---------------------------------------------------------------------------
uint8 mp_audio_load(MPaudio* a, const char* path)
{
	if (a == NULL) return 0;
	if (path == NULL || path[0] == '\0')
		return mp_audio_load_synth(a, 20) ? 0 : 0; //无文件:兜底(load 语义返回 0=非真实文件)

	//先探一下文件在不在(ffmpeg 也会报,但这样日志更清楚)
	FILE* probe = fopen(path, "rb");
	if (probe == NULL)
	{
		gy_log_print("mp_audio: file not found '%s' -> synth fallback\n", path);
		mp_audio_load_synth(a, 20);
		return 0;
	}
	fclose(probe);

	//构造命令:-nostdin 防抢终端,-v error 静音日志,裸 s16le 到 stdout
	//注意:路径用引号包裹,简单转义单引号
	char cmd[2048];
	//把 path 里的单引号替换为 '\'' 序列,防命令注入/断裂
	char esc[1024];
	{
		int32 k = 0, j = 0;
		while (path[k] != '\0' && j < (int32)sizeof(esc) - 5)
		{
			if (path[k] == '\'') { esc[j++]='\''; esc[j++]='\\'; esc[j++]='\''; esc[j++]='\''; }
			else esc[j++] = path[k];
			k++;
		}
		esc[j] = '\0';
	}
	snprintf(cmd, sizeof(cmd),
	         "ffmpeg -nostdin -v error -i '%s' -f s16le -acodec pcm_s16le -ac %d -ar %d - 2>/dev/null",
	         esc, MP_AUD_CHANNELS, MP_AUD_RATE);

	FILE* pipe = popen(cmd, "r");
	if (pipe == NULL)
	{
		gy_log_print("mp_audio: popen failed -> synth fallback\n");
		mp_audio_load_synth(a, 20);
		return 0;
	}

	freePcm(a);
	//分块读:每块若干帧
	const int64 CHUNK = 16384; //帧
	int16 tmp[16384 * FRAME_INT16];
	int64 total = 0;
	uint8 oom = 0;
	for (;;)
	{
		size_t got = fread(tmp, sizeof(int16) * FRAME_INT16, (size_t)CHUNK, pipe);
		if (got == 0) break;
		if (!ensureCap(a, total + (int64)got)) { oom = 1; break; }
		memcpy(a->pcm + total * FRAME_INT16, tmp, got * FRAME_INT16 * sizeof(int16));
		total += (int64)got;
	}
	int rc = pclose(pipe);

	if (oom || total <= 0 || rc != 0)
	{
		gy_log_print("mp_audio: decode empty/err(rc=%d,frames=%lld) -> synth fallback\n",
		             rc, (long long)total);
		mp_audio_load_synth(a, 20);
		return 0;
	}

	a->frames = total;
	a->cur = 0;
	a->is_synth = 0;
	openDevice(a);
	gy_log_print("mp_audio: loaded '%s' %lld frames (%d ms)\n",
	             path, (long long)total, (int)(total * 1000 / MP_AUD_RATE));
	return 1;
}

//---------------------------------------------------------------------------
// 播放控制
//---------------------------------------------------------------------------
//静音时钟基准复位(以当前 cur 为锚)
static void wallRebase(MPaudio* a)
{
	a->wall_base_ms = SDL_GetTicks();
	a->wall_base_frame = a->cur;
}

void mp_audio_play(MPaudio* a)
{
	if (a == NULL || a->frames <= 0) return;
	a->playing = 1;
	wallRebase(a);
	if (a->dev) SDL_PauseAudioDevice(a->dev, 0);
}

void mp_audio_pause(MPaudio* a)
{
	if (a == NULL) return;
	a->playing = 0;
	if (a->dev) SDL_PauseAudioDevice(a->dev, 1);
}

void mp_audio_toggle(MPaudio* a)
{
	if (a == NULL) return;
	if (a->playing) mp_audio_pause(a);
	else            mp_audio_play(a);
}

uint8 mp_audio_is_playing(const MPaudio* a) { return (a != NULL) ? a->playing : 0; }
uint8 mp_audio_is_silent (const MPaudio* a) { return (a != NULL) ? a->silent  : 1; }
uint8 mp_audio_is_synth  (const MPaudio* a) { return (a != NULL) ? a->is_synth: 0; }

//声卡队列里还没播的帧数
static int64 queuedFrames(const MPaudio* a)
{
	if (a->dev == 0) return 0;
	Uint32 bytes = SDL_GetQueuedAudioSize(a->dev);
	return (int64)bytes / (FRAME_INT16 * (int64)sizeof(int16));
}

uint8 mp_audio_is_finished(const MPaudio* a)
{
	if (a == NULL || a->frames <= 0) return 0;
	if (a->cur < a->frames) return 0;
	return queuedFrames(a) <= 0;
}

void mp_audio_seek_ms(MPaudio* a, int32 ms)
{
	if (a == NULL || a->frames <= 0) return;
	if (ms < 0) ms = 0;
	int64 f = (int64)ms * MP_AUD_RATE / 1000;
	if (f > a->frames) f = a->frames;
	a->cur = f;
	if (a->dev) SDL_ClearQueuedAudio(a->dev);
	wallRebase(a);
}

int32 mp_audio_dur_ms(const MPaudio* a)
{
	if (a == NULL || a->frames <= 0) return 0;
	return (int32)(a->frames * 1000 / MP_AUD_RATE);
}

//可闻位置:有声卡=已推送减队列剩余;静音=wall-clock 从基准推进
int32 mp_audio_pos_ms(const MPaudio* a)
{
	if (a == NULL || a->frames <= 0) return 0;
	int64 audible;
	if (a->silent || a->dev == 0)
	{
		if (a->playing)
		{
			uint32 dt = SDL_GetTicks() - a->wall_base_ms;
			audible = a->wall_base_frame + (int64)dt * MP_AUD_RATE / 1000;
		}
		else audible = a->cur;
	}
	else
	{
		audible = a->cur - queuedFrames(a);
	}
	if (audible < 0) audible = 0;
	if (audible > a->frames) audible = a->frames;
	return (int32)(audible * 1000 / MP_AUD_RATE);
}

//---------------------------------------------------------------------------
// 每帧推进:向声卡补足队列;静音态只推进 cur(供 finished 判定)
//---------------------------------------------------------------------------
void mp_audio_tick(MPaudio* a)
{
	if (a == NULL || a->frames <= 0 || !a->playing) return;

	if (a->silent || a->dev == 0)
	{
		//静音:按 wall-clock 把 cur 顶到当前可闻位置
		uint32 dt = SDL_GetTicks() - a->wall_base_ms;
		int64 want = a->wall_base_frame + (int64)dt * MP_AUD_RATE / 1000;
		if (want > a->frames) want = a->frames;
		if (want > a->cur) a->cur = want;
		return;
	}

	//有声卡:维持约 0.3s 队列水位
	const int64 target_q = MP_AUD_RATE / 3;
	int64 q = queuedFrames(a);
	while (q < target_q && a->cur < a->frames)
	{
		int64 push = target_q - q;
		int64 avail = a->frames - a->cur;
		if (push > avail) push = avail;
		if (push > 8192) push = 8192;
		SDL_QueueAudio(a->dev, a->pcm + a->cur * FRAME_INT16,
		               (Uint32)(push * FRAME_INT16 * sizeof(int16)));
		a->cur += push;
		q += push;
	}
}

//---------------------------------------------------------------------------
// 频谱取样:从当前可闻位置起取 want 帧,混成 mono int16
//---------------------------------------------------------------------------
int32 mp_audio_peek_mono(const MPaudio* a, int16* out, int32 want)
{
	if (a == NULL || out == NULL || want <= 0 || a->frames <= 0) return 0;
	int32 pos_ms = mp_audio_pos_ms(a);
	int64 start = (int64)pos_ms * MP_AUD_RATE / 1000;
	if (start < 0) start = 0;
	if (start >= a->frames) return 0;
	int64 n = a->frames - start;
	if (n > want) n = want;
	int64 i;
	for (i = 0; i < n; i++)
	{
		int32 l = a->pcm[(start + i) * FRAME_INT16 + 0];
		int32 r = a->pcm[(start + i) * FRAME_INT16 + 1];
		out[i] = (int16)((l + r) / 2);
	}
	return (int32)n;
}
