#ifndef VP_VIDEO_H
#define VP_VIDEO_H

#include "YMGUI_PubType.h"
#include "YMGUI_DrawImg.h" //GYpx

//===========================================================================
// vp_video —— 视频解码引擎(全部视频/解码语义,app 侧,绝不进 GUI 库)。
//   解码:ffmpeg CLI(popen)把视频流式解成 rgb565le 裸帧(= GYpx,小端零转换),
//         缩放到不超过 max_w×max_h 的偶数尺寸(保宽高比);ffprobe 探尺寸/帧率/时长。
//         管道 fd 设 O_NONBLOCK,单线程非阻塞读进帧环形缓冲(满则停读 → ffmpeg 写阻塞
//         形成天然背压)。无 ffmpeg/ffprobe 或文件不可解 → 合成动画测试图兜底(仍可跑)。
//   显示:模块持有一块稳定 display_buf(out_w×out_h GYpx);vp_video_update(now_ms) 按
//         音频主时钟把"当前应显示帧"拷进 display_buf(丢过期帧/持未来帧),返回是否有更新。
//         app 把 GYimg.data 指向 vp_video_pixels() 一次,更新时只 Invalidate。
//   A/V 同步:音频为主钟(mp_audio),视频跟随 —— 语义全在 app;库只提供缩放 blit 图元。
//   SDL/ffmpeg/解码 全在本文件与 mp_audio.c,GUI 库零音视频依赖。
//===========================================================================

typedef struct VPvideo VPvideo;

//新建引擎(未打开)。失败返回 NULL
VPvideo* vp_video_create(void);
//销毁(关管道、释放缓冲)
void     vp_video_destroy(VPvideo* v);

//打开视频文件:ffprobe 探参 + 启动 ffmpeg 流式解码,输出缩放到 <= max_w×max_h(保比、偶数)。
//成功返回 1(REAL 模式);无 ffmpeg/ffprobe/文件不可解 → 合成动画兜底并返回 0(引擎仍可"播")。
uint8    vp_video_open(VPvideo* v, const char* path, int32 max_w, int32 max_h);

//解码输出帧尺寸(display_buf 尺寸)
int32    vp_video_width (const VPvideo* v);
int32    vp_video_height(const VPvideo* v);
//总时长(毫秒;探测不到时为兜底值)
int32    vp_video_dur_ms(const VPvideo* v);

//稳定显示缓冲指针(out_w×out_h GYpx)。app 把 GYimg.data 指向它(一次),后续原地更新
const GYpx* vp_video_pixels(const VPvideo* v);

//每帧调用:从管道非阻塞补读入环形缓冲。应在 update 前每帧调一次
void     vp_video_read_tick(VPvideo* v);

//按主时钟 now_ms 选出应显示帧拷进 display_buf。返回 1=display_buf 有更新(需 Invalidate),0=保持
uint8    vp_video_update(VPvideo* v, int32 now_ms);

//跳转到 ms(REAL:以 -ss 重启 ffmpeg 管道;SYNTH:仅重置时钟锚)
void     vp_video_seek_ms(VPvideo* v, int32 ms);

//是否已到流尾(管道读到 EOF 且环形缓冲排空)
uint8    vp_video_is_eof(const VPvideo* v);
//是否走合成兜底(非真实解码)
uint8    vp_video_is_synth(const VPvideo* v);

#endif // !VP_VIDEO_H
