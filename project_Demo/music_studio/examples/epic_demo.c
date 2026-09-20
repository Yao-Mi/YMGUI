#include "model/project.h"
#include "audio/audio.h"
#include <stdio.h>
#include <stdlib.h>

/* 生成可编辑的小样，演示相同钢琴卷帘驱动不同乐器；合唱使用应用自带的采样资源。 */
int main(int argc, char** argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "用法：music_epic_demo 工程.ymmusic 试听.wav\n");
		return 2;
	}
	MsProject* p = calloc(1, sizeof(*p));
	MsSounds sounds;
	if (!p)
		return 1;
	ms_project_init(p, 0);
	MsSong* s = &p->song;
	s->tracks = s->patterns = s->clips = 0;
	s->bpm = 90;
	s->loop_start = 0;
	s->loop_end = 6 * 4 * MS_PPQ;
	snprintf(s->name, MS_NAME, "山海序章");
	const int instruments[] = {4, 8, 9, 10, 12};
	const float volumes[] = {.55f, .85f, .50f, .95f, .65f};
	const float pans[] = {-.30f, -.15f, .25f, 0, .30f};
	const int roots[] = {48, 44, 46};
	const int thirds[] = {3, 4, 4};
	for (int t = 0; t < 5; ++t)
	{
		int track = ms_track_add(s, MS_SYNTH);
		s->track[track].instrument = instruments[t];
		s->track[track].volume = volumes[t];
		s->track[track].pan = pans[t];
		snprintf(s->track[track].name, MS_NAME, "%s", ms_instrument_names[instruments[t]]);
		for (int section = 0; section < 3; ++section)
		{
			int index = ms_pattern_add(s, MS_SYNTH, -1);
			MsPattern* pat = &s->pattern[index];
			pat->steps = 32;
			snprintf(pat->name, MS_NAME, "%s %d", ms_instrument_names[instruments[t]], section + 1);
			int root = roots[section], duration = 8 * MS_PPQ;
			if (t == 1)
			{
				const int melody[] = {0, 7, 12, 7};
				for (int i = 0; i < 4; ++i)
					ms_note_toggle(pat, i * MS_PPQ, root + 12 + melody[i], 90 + section * 8, MS_PPQ - 6);
				ms_note_toggle(pat, 4 * MS_PPQ, root + 12 + thirds[section], 100, 4 * MS_PPQ - 12);
			}
			else if (t == 3)
			{
				const int beats[] = {0, 3, 4, 6, 7};
				for (int i = 0; i < 5; ++i)
					ms_note_toggle(pat, beats[i] * MS_PPQ, root - 12, i == 0 || i == 2 ? 120 : 85, MS_PPQ - 4);
			}
			else
			{
				int chord[] = {root, root + thirds[section], root + 7};
				for (int note = 0; note < 3; ++note)
				{
					int pitch = chord[note] + (t == 4 ? 12 : 0);
					if (t == 2)
					{
						ms_note_toggle(pat, 0, pitch, 90, 2 * MS_PPQ);
						ms_note_toggle(pat, 4 * MS_PPQ, pitch, 110, 4 * MS_PPQ - 12);
					}
					else
						ms_note_toggle(pat, 0, pitch, 85 + section * 8, duration - 12);
				}
			}
			ms_clip_add(s, track, index, section * duration, duration);
		}
	}
	char error[256];
	int ok = ms_song_valid(s, p->assets) && ms_sounds_init(&sounds);
	if (ok)
	{
		ok = ms_project_save(p, argv[1], error, sizeof(error)) && ms_export(p, &sounds, argv[2], 0, error, sizeof(error));
		if (!ok)
			fprintf(stderr, "%s\n", error);
		ms_sounds_free(&sounds);
	}
	ms_project_free(p);
	free(p);
	return ok ? 0 : 1;
}
