#ifndef MS_AUDIO_H
#define MS_AUDIO_H
#include "model/project.h"
#include <SDL.h>

#define MS_SAMPLE_ZONES 13

typedef struct
{
	float* instrument[MS_INSTRUMENTS][MS_SAMPLE_ZONES];
	float* drums[MS_DRUMS];
	int lengths[MS_DRUMS];
	float sine[4096];
} MsSounds;
typedef struct MsPlayback MsPlayback;
typedef struct
{
	/* 主线程持有控制字段；后台只访问自己的只读工程快照。 */
	MsPlayback* playback;
	SDL_AudioDeviceID device;
	int playing, loop, pattern_mode, pattern, track, clip, metronome;
	int64_t cursor;
	int64_t loop_first, loop_last;
	int looping;
	float meters[MS_TRACKS + 1];
} MsTransport;
/* 无状态 PCM 插值采样，试听、编排、离线导出共享；没有实时乐器合成。 */
float ms_instrument_sample(const MsSounds* sounds, double hz, int instrument, int position, int length);
int ms_preview_project(MsProject* preview, const MsProject* source, int pattern, int track, int clip);
int ms_sample_bank_load(float** zones, const char* path);
float ms_sample_bank_play(float* const* zones, double hz, int position, int length, int loop);
int ms_sounds_init(MsSounds* sounds);
void ms_sounds_free(MsSounds* sounds);
int ms_asset_import(MsAsset* a, const char* path, char* error, size_t cap);
void ms_render(const MsProject* p, const MsSounds* sounds, int64_t frame, int frames, float* output, float* meters, int metronome);
int ms_export(const MsProject* p, const MsSounds* sounds, const char* path, int loop_only, char* error, size_t cap);
void ms_transport_init(MsTransport* t);
void ms_transport_close(MsTransport* t);
int64_t ms_transport_position(const MsTransport* t);
void ms_transport_seek(MsTransport* t, int64_t frame);
void ms_transport_tick(MsTransport* t, const MsProject* p, const MsSounds* sounds);
void ms_audition(MsTransport* t, const MsProject* p, const MsSounds* sounds, int drum, int pitch);
#endif
