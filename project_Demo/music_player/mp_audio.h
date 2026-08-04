#ifndef MP_AUDIO_H
#define MP_AUDIO_H

#include "YMGUI_PubType.h"

//===========================================================================
// mp_audio —— 播放引擎(全部音乐语义,app 侧,绝不进 GUI 库)。
//   解码:ffmpeg CLI(popen)把任意音频整曲解成定格式 PCM 存内存 —— 库无音视频依赖,
//         开发机无 ffmpeg 开发库也能用(走命令行)。解不出/无文件时合成一段旋律兜底。
//   输出:SDL2 声卡(SDL_QueueAudio)。开设备失败(如 headless)→ 静音兜底,
//         播放时钟改由 wall-clock 推进,UI 逻辑不变。SDL 音频部分只在本文件,不污染库。
//   格式:统一 44100Hz / 双声道 / s16le。seek/进度/频谱都在这块整曲缓冲上做。
//===========================================================================

#define MP_AUD_RATE     44100
#define MP_AUD_CHANNELS 2

typedef struct MPaudio MPaudio;

//新建引擎(未加载)。失败返回 NULL
MPaudio* mp_audio_create(void);
//销毁(关设备、释放缓冲)
void     mp_audio_destroy(MPaudio* a);

//加载音频文件(ffmpeg 解码整曲进内存)。成功返回 1;
//失败(无文件/ffmpeg 不可用/解码空)则合成兜底旋律并返回 0(引擎仍可播)
uint8    mp_audio_load(MPaudio* a, const char* path);
//直接装一段合成旋律(秒数);返回 1
uint8    mp_audio_load_synth(MPaudio* a, int32 seconds);

//开始/继续播放(装了曲子才有效)
void     mp_audio_play(MPaudio* a);
//暂停(保留位置)
void     mp_audio_pause(MPaudio* a);
//切换播放/暂停
void     mp_audio_toggle(MPaudio* a);
//是否正在播放
uint8    mp_audio_is_playing(const MPaudio* a);
//是否已播完(到尾且队列排空)
uint8    mp_audio_is_finished(const MPaudio* a);

//跳到指定毫秒(钳制到 [0,时长])。清声卡队列,位置重置
void     mp_audio_seek_ms(MPaudio* a, int32 ms);
//当前可闻播放位置(毫秒)—— 已推进减去声卡队列里没放的,静音态用 wall-clock
int32    mp_audio_pos_ms(const MPaudio* a);
//总时长(毫秒)
int32    mp_audio_dur_ms(const MPaudio* a);

//每帧推进:向声卡补数据、更新可闻位置。应每帧调用一次
void     mp_audio_tick(MPaudio* a);

//取当前播放点起的一窗 mono 样本(供频谱)。写入 out(int16),返回实际样本数
int32    mp_audio_peek_mono(const MPaudio* a, int16* out, int32 want);

//是否走静音兜底(没开成声卡)。1=静音(位置靠 wall-clock)
uint8    mp_audio_is_silent(const MPaudio* a);
//是否在播合成兜底旋律(非真实文件)
uint8    mp_audio_is_synth(const MPaudio* a);

#endif // !MP_AUDIO_H
