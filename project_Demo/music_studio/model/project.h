#ifndef MS_PROJECT_H
#define MS_PROJECT_H
#include <stdint.h>
#include <stddef.h>

/* 音乐时间采用 96 PPQ；音频始终使用 48 kHz 立体声。模型不依赖 GUI/SDL。 */
#define MS_RATE 48000
#define MS_PPQ 96
#define MS_STEP 24
#define MS_TRACKS 8
#define MS_PATTERNS 16
#define MS_DRUMS 9
#define MS_INSTRUMENTS 13
extern const char* const ms_instrument_names[MS_INSTRUMENTS];
#define MS_STEPS 32
#define MS_NOTES 128
#define MS_CLIPS 128
#define MS_ASSETS 16
#define MS_HISTORY 32
#define MS_END (256 * 4 * MS_PPQ)
#define MS_NAME 64
#define MS_PATH 1024
#define MS_ASSET_FRAMES (MS_RATE * 120)
#define MS_TOTAL_FRAMES (MS_RATE * 600)

typedef enum
{
	MS_DRUM,
	MS_SYNTH,
	MS_AUDIO
} MsKind;
typedef struct
{
	int tick, length, pitch, velocity;
} MsNote;
typedef struct
{
	char name[MS_NAME];
	int kind, steps, count;
	uint8_t drum[MS_DRUMS][MS_STEPS];
	MsNote notes[MS_NOTES];
} MsPattern;
typedef struct
{
	char name[MS_NAME];
	int kind, mute, solo, instrument;
	float volume, pan;
	uint32_t color;
} MsTrack;
typedef struct
{
	int track, source, start, length, offset;
	float gain, fade_in, fade_out; /* 淡入淡出以秒计。 */
} MsClip;
typedef struct
{
	char name[MS_NAME];
	int bpm, beats, loop_start, loop_end;
	int tracks, patterns, clips;
	float master;
	MsTrack track[MS_TRACKS];
	MsPattern pattern[MS_PATTERNS];
	MsClip clip[MS_CLIPS];
	float drum_volume[MS_DRUMS], drum_pan[MS_DRUMS];
	int drum_mute[MS_DRUMS], drum_solo[MS_DRUMS], drum_asset[MS_DRUMS];
} MsSong;
typedef struct
{
	char name[MS_NAME];
	float* pcm;
	int frames;
	float peaks[512];
} MsAsset;
typedef struct
{
	MsSong song;
	int assets;
	MsAsset asset[MS_ASSETS];
} MsProject;
typedef struct
{
	MsSong undo[MS_HISTORY], redo[MS_HISTORY];
	int undos, redos;
} MsHistory;

void ms_project_init(MsProject* p, int demo);
void ms_project_free(MsProject* p);
void ms_asset_peaks(MsAsset* a);
int ms_song_valid(const MsSong* s, int assets);
int ms_track_add(MsSong* s, int kind);
void ms_track_delete(MsSong* s, int track);
void ms_track_move(MsSong* s, int from, int to);
int ms_pattern_add(MsSong* s, int kind, int copy);
int ms_clip_add(MsSong* s, int track, int source, int start, int length);
int ms_clip_split(MsSong* s, int index, int tick);
void ms_clip_delete(MsSong* s, int index);
int ms_note_toggle(MsPattern* p, int tick, int pitch, int velocity, int length);
int ms_song_end(const MsSong* s);
double ms_tick_frame(const MsSong* s, double tick);
double ms_frame_tick(const MsSong* s, double frame);
void ms_history_push(MsHistory* h, const MsSong* s);
int ms_history_undo(MsHistory* h, MsSong* s);
int ms_history_redo(MsHistory* h, MsSong* s);
const char* ms_drum_name(int index);

/* 保存为单文件工程，显式小端序编码，包含素材 PCM；加载校验后才替换当前工程。 */
int ms_project_save(const MsProject* p, const char* path, char* error, size_t cap);
int ms_project_load(MsProject* p, const char* path, char* error, size_t cap);
#endif
