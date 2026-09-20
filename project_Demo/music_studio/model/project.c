#include "project.h"

/* 文件中的音色编号只追加，保留最初的 0 / 1 / 2。 */
const char* const ms_instrument_names[MS_INSTRUMENTS] = {
	"钢琴采样", "电钢琴采样", "贝斯采样", "吉他采样",
	"弦乐采样", "管风琴采样", "长笛采样", "电子主音采样",
	"圆号采样", "铜管采样", "定音鼓采样", "人声元音采样", "合唱采样"
};
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char* drum_names[MS_DRUMS] = {"底鼓", "军鼓", "闭镲", "开镲", "拍手", "低嗵", "高嗵", "吊镲", "响指"};
const char* ms_drum_name(int i)
{
	return i >= 0 && i < MS_DRUMS ? drum_names[i] : "";
}
double ms_tick_frame(const MsSong* s, double t)
{
	return t * MS_RATE * 60.0 / (s->bpm * MS_PPQ);
}
double ms_frame_tick(const MsSong* s, double f)
{
	return f * s->bpm * MS_PPQ / (MS_RATE * 60.0);
}
int ms_song_end(const MsSong* s)
{
	int end = s->beats * MS_PPQ;
	for (int i = 0; i < s->clips; ++i)
		if (s->clip[i].start + s->clip[i].length > end)
			end = s->clip[i].start + s->clip[i].length;
	return end;
}
int ms_track_add(MsSong* s, int kind)
{
	static const uint32_t colors[] = {0xd5a35d, 0x68b7a5, 0x778ed0, 0xc685a8, 0x8db675, 0xa298d1, 0x61aabd, 0xbd8e77};
	if (s->tracks >= MS_TRACKS || kind < MS_DRUM || kind > MS_AUDIO)
		return -1;
	int i = s->tracks++;
	MsTrack* t = &s->track[i];
	memset(t, 0, sizeof(*t));
	t->kind = kind;
	t->instrument = 0;
	t->volume = .72f;
	t->color = colors[i];
	snprintf(t->name, sizeof(t->name), "%s %d", kind == MS_DRUM ? "鼓组" : kind == MS_SYNTH ? "旋律"
																							: "音频",
			 i + 1);
	return i;
}
void ms_clip_delete(MsSong* s, int i)
{
	if (i < 0 || i >= s->clips)
		return;
	memmove(&s->clip[i], &s->clip[i + 1], (size_t)(s->clips - i - 1) * sizeof(MsClip));
	memset(&s->clip[--s->clips], 0, sizeof(MsClip));
}
void ms_track_delete(MsSong* s, int t)
{
	if (t < 0 || t >= s->tracks)
		return;
	for (int i = s->clips - 1; i >= 0; --i)
		if (s->clip[i].track == t)
			ms_clip_delete(s, i);
		else if (s->clip[i].track > t)
			--s->clip[i].track;
	memmove(&s->track[t], &s->track[t + 1], (size_t)(s->tracks - t - 1) * sizeof(MsTrack));
	memset(&s->track[--s->tracks], 0, sizeof(MsTrack));
}
void ms_track_move(MsSong* s, int from, int to)
{
	if (from < 0 || to < 0 || from >= s->tracks || to >= s->tracks)
		return;
	MsTrack tmp = s->track[from];
	s->track[from] = s->track[to];
	s->track[to] = tmp;
	for (int i = 0; i < s->clips; ++i)
		if (s->clip[i].track == from)
			s->clip[i].track = to;
		else if (s->clip[i].track == to)
			s->clip[i].track = from;
}
int ms_pattern_add(MsSong* s, int kind, int copy)
{
	if (s->patterns >= MS_PATTERNS || kind < MS_DRUM || kind > MS_SYNTH)
		return -1;
	int n = s->patterns++;
	MsPattern* p = &s->pattern[n];
	if (copy >= 0 && copy < n && s->pattern[copy].kind == kind)
		*p = s->pattern[copy];
	else
	{
		memset(p, 0, sizeof(*p));
		p->kind = kind;
		p->steps = 16;
	}
	snprintf(p->name, sizeof(p->name), "%s %d", kind == MS_DRUM ? "节奏型" : "旋律段", n + 1);
	return n;
}
int ms_clip_add(MsSong* s, int track, int source, int start, int length)
{
	if (s->clips == MS_CLIPS || track < 0 || track >= s->tracks || start < 0 || length < 1 || start > MS_END - length)
		return -1;
	if (s->track[track].kind != MS_AUDIO && (source < 0 || source >= s->patterns || s->pattern[source].kind != s->track[track].kind))
		return -1;
	int i = s->clips++;
	s->clip[i] = (MsClip){track, source, start, length, 0, 1.f, 0.f, 0.f};
	return i;
}
int ms_clip_split(MsSong* s, int i, int tick)
{
	if (i < 0 || i >= s->clips || s->clips == MS_CLIPS)
		return -1;
	MsClip* c = &s->clip[i];
	int left = tick - c->start;
	if (left <= 0 || left >= c->length)
		return -1;
	MsClip right = *c;
	right.start = tick;
	right.length -= left;
	right.offset += left;
	/* 切点不引入额外淡化，原边缘淡化保留。 */
	right.fade_in = 0;
	c->fade_out = 0;
	c->length = left;
	s->clip[s->clips] = right;
	return s->clips++;
}
int ms_note_toggle(MsPattern* p, int tick, int pitch, int velocity, int length)
{
	if (tick < 0 || tick >= p->steps * MS_STEP || pitch < 0 || pitch > 127)
		return -1;
	for (int i = 0; i < p->count; ++i)
		if (p->notes[i].tick == tick && p->notes[i].pitch == pitch)
		{
			memmove(&p->notes[i], &p->notes[i + 1], (size_t)(p->count - i - 1) * sizeof(MsNote));
			memset(&p->notes[--p->count], 0, sizeof(MsNote));
			return -1;
		}
	if (p->count == MS_NOTES)
		return -1;
	if (length > p->steps * MS_STEP - tick)
		length = p->steps * MS_STEP - tick;
	p->notes[p->count] = (MsNote){tick, length, pitch, velocity};
	return p->count++;
}
void ms_project_init(MsProject* p, int demo)
{
	memset(p, 0, sizeof(*p));
	MsSong* s = &p->song;
	strcpy(s->name, demo ? "午后律动" : "未命名作品");
	s->bpm = 110;
	s->beats = 4;
	s->master = .8f;
	s->loop_end = 16 * MS_PPQ;
	for (int i = 0; i < MS_DRUMS; ++i)
	{
		s->drum_volume[i] = .8f;
		s->drum_asset[i] = -1;
	}
	ms_track_add(s, MS_DRUM);
	ms_track_add(s, MS_SYNTH);
	ms_track_add(s, MS_AUDIO);
	ms_pattern_add(s, MS_DRUM, -1);
	ms_pattern_add(s, MS_SYNTH, -1);
	if (!demo)
		return;
	strcpy(s->track[0].name, "电子鼓组");
	strcpy(s->track[1].name, "采样钢琴");
	strcpy(s->track[2].name, "音频素材");
	strcpy(s->pattern[0].name, "主节奏");
	strcpy(s->pattern[1].name, "午后旋律");
	for (int j = 0; j < 16; ++j)
	{
		if (j % 4 == 0)
			s->pattern[0].drum[0][j] = 108;
		if (j % 8 == 4)
			s->pattern[0].drum[1][j] = 100;
		if (j % 2 == 0)
			s->pattern[0].drum[2][j] = j % 4 ? 62 : 88;
	}
	s->pattern[0].drum[3][14] = 67;
	int pitches[] = {60, 64, 67, 71, 69, 67, 64, 62};
	for (int i = 0; i < 8; ++i)
		ms_note_toggle(&s->pattern[1], i * 48, pitches[i], 86, 42);
	for (int i = 0; i < 4; ++i)
	{
		ms_clip_add(s, 0, 0, i * 384, 384);
		ms_clip_add(s, 1, 1, i * 384, 384);
	}
}
void ms_project_free(MsProject* p)
{
	for (int i = 0; i < p->assets; ++i)
		free(p->asset[i].pcm);
	memset(p, 0, sizeof(*p));
}
static int number(float x, float lo, float hi)
{
	return isfinite(x) && x >= lo && x <= hi;
}
int ms_song_valid(const MsSong* s, int assets)
{
	if (assets < 0 || assets > MS_ASSETS || s->bpm < 30 || s->bpm > 300 || s->beats < 1 || s->beats > 12 ||
		s->tracks < 0 || s->tracks > MS_TRACKS || s->patterns < 0 || s->patterns > MS_PATTERNS || s->clips < 0 || s->clips > MS_CLIPS ||
		s->loop_start < 0 || s->loop_end <= s->loop_start || s->loop_end > MS_END || !number(s->master, 0, 1.5f))
		return 0;
	if (!memchr(s->name, 0, MS_NAME))
		return 0;
	for (int i = 0; i < s->tracks; ++i)
	{
		const MsTrack* t = &s->track[i];
		if (!memchr(t->name, 0, MS_NAME) || t->kind < 0 || t->kind > 2 || t->mute < 0 || t->mute > 1 || t->solo < 0 || t->solo > 1 ||
			t->instrument < 0 || t->instrument >= MS_INSTRUMENTS || !number(t->volume, 0, 1.5f) || !number(t->pan, -1, 1))
			return 0;
	}
	for (int i = 0; i < s->patterns; ++i)
	{
		const MsPattern* p = &s->pattern[i];
		if (!memchr(p->name, 0, MS_NAME) || p->kind < 0 || p->kind > 1 || (p->steps != 16 && p->steps != 32) || p->count < 0 || p->count > MS_NOTES)
			return 0;
		for (int d = 0; d < MS_DRUMS; ++d)
			for (int j = 0; j < MS_STEPS; ++j)
				if (p->drum[d][j] > 127)
					return 0;
		for (int n = 0; n < p->count; ++n)
		{
			const MsNote* v = &p->notes[n];
			if (v->tick < 0 || v->tick >= p->steps * MS_STEP || v->length < 1 || v->length > p->steps * MS_STEP - v->tick || v->pitch < 0 || v->pitch > 127 || v->velocity < 1 || v->velocity > 127)
				return 0;
		}
	}
	for (int d = 0; d < MS_DRUMS; ++d)
		if (!number(s->drum_volume[d], 0, 1.5f) || !number(s->drum_pan[d], -1, 1) || s->drum_asset[d] < -1 || s->drum_asset[d] >= assets ||
			s->drum_mute[d] < 0 || s->drum_mute[d] > 1 || s->drum_solo[d] < 0 || s->drum_solo[d] > 1)
			return 0;
	for (int i = 0; i < s->clips; ++i)
	{
		const MsClip* c = &s->clip[i];
		if (c->track < 0 || c->track >= s->tracks || c->start < 0 || c->length < 1 || c->length > MS_END || c->start > MS_END - c->length || c->offset < 0 || c->offset > MS_END ||
			!number(c->gain, 0, 2) || !number(c->fade_in, 0, 10) || !number(c->fade_out, 0, 10))
			return 0;
		int kind = s->track[c->track].kind;
		if (c->source < 0 || c->source >= (kind == MS_AUDIO ? assets : s->patterns))
			return 0;
		if (kind != MS_AUDIO && s->pattern[c->source].kind != kind)
			return 0;
	}
	return 1;
}
void ms_history_push(MsHistory* h, const MsSong* s)
{
	if (h->undos == MS_HISTORY)
	{
		memmove(h->undo, h->undo + 1, (MS_HISTORY - 1) * sizeof(MsSong));
		--h->undos;
	}
	h->undo[h->undos++] = *s;
	h->redos = 0;
}
int ms_history_undo(MsHistory* h, MsSong* s)
{
	if (!h->undos)
		return 0;
	h->redo[h->redos++] = *s;
	*s = h->undo[--h->undos];
	return 1;
}
int ms_history_redo(MsHistory* h, MsSong* s)
{
	if (!h->redos)
		return 0;
	h->undo[h->undos++] = *s;
	*s = h->redo[--h->redos];
	return 1;
}

void ms_asset_peaks(MsAsset* a)
{
	memset(a->peaks, 0, sizeof(a->peaks));
	for (int i = 0; i < a->frames; ++i)
	{
		int bin = (int)((int64_t)i * 512 / a->frames);
		float v = fmaxf(fabsf(a->pcm[i * 2]), fabsf(a->pcm[i * 2 + 1]));
		if (v > a->peaks[bin])
			a->peaks[bin] = v;
	}
}
