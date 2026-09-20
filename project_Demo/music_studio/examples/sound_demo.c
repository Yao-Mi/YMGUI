#include "audio/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 输出专用试听文件到调用方指定目录；不修改用户已有工程。 */
int main(int argc, char** argv)
{
	if (argc != 2)
		return 2;
	MsProject* p = calloc(1, sizeof(*p));
	MsSounds sounds;
	if (!p || !ms_sounds_init(&sounds))
	{
		free(p);
		return 1;
	}
	char path[MS_PATH], error[256];
	int ok = 1;
	for (int group = 0; ok && group < 2; ++group)
	{
		ms_project_free(p);
		ms_project_init(p, 0);
		MsSong* s = &p->song;
		s->tracks = s->patterns = s->clips = 0;
		s->bpm = 120;
		snprintf(s->name, MS_NAME, "全采样乐器试听 %d", group + 1);
		for (int instrument = group * 8; instrument < MS_INSTRUMENTS && instrument < (group + 1) * 8; ++instrument)
		{
			int t = ms_track_add(s, MS_SYNTH), pattern = ms_pattern_add(s, MS_SYNTH, -1);
			s->track[t].instrument = instrument;
			snprintf(s->track[t].name, MS_NAME, "%s", ms_instrument_names[instrument]);
			snprintf(s->pattern[pattern].name, MS_NAME, "%s", ms_instrument_names[instrument]);
			ms_note_toggle(&s->pattern[pattern], 0, 60, 100, 72);
			ms_note_toggle(&s->pattern[pattern], 96, 64, 100, 72);
			ms_note_toggle(&s->pattern[pattern], 192, 67, 100, 192);
			ms_clip_add(s, t, pattern, t * 384, 384);
		}
		s->loop_start = 0;
		s->loop_end = s->tracks * 384;
		snprintf(path, sizeof(path), "%s/全采样乐器试听%d.ymmusic", argv[1], group + 1);
		ok = ms_project_save(p, path, error, sizeof(error));
		if (ok)
		{
			snprintf(path, sizeof(path), "%s/全采样乐器试听%d.wav", argv[1], group + 1);
			ok = ms_export(p, &sounds, path, 0, error, sizeof(error));
		}
	}
	MsSong* s = &p->song;
	ms_project_free(p);
	ms_project_init(p, 0);
	s->bpm = 120;
	s->patterns = s->tracks = 1;
	s->pattern[0].steps = 32;
	for (int step = 0; step < 32; step += 4)
		s->pattern[0].drum[step < 16 ? 4 : 8][step] = 100;
	ms_clip_add(s, 0, 0, 0, 768);
	if (ok)
	{
		snprintf(path, sizeof(path), "%s/拍手与响指.wav", argv[1]);
		ok = ms_export(p, &sounds, path, 0, error, sizeof(error));
	}
	if (!ok)
		fprintf(stderr, "%s\n", error);
	ms_sounds_free(&sounds);
	ms_project_free(p);
	free(p);
	return ok ? 0 : 1;
}
