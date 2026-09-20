#include "model/project.h"
#include "audio/audio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

static void test_instruments(const MsSounds* sounds, const char* directory)
{
	MsProject* p = calloc(1, sizeof(*p));
	MsProject* preview = calloc(1, sizeof(*preview));
	MsProject* loaded = calloc(1, sizeof(*loaded));
	assert(p && preview && loaded);
	ms_project_init(p, 1);
	int track = ms_track_add(&p->song, MS_SYNTH);
	assert(track >= 0);
	p->song.track[track].mute = 1;
	p->song.track[0].solo = 1;
	float* samples = calloc(MS_INSTRUMENTS * 4096 * 2, sizeof(float));
	assert(samples);
	char path[MS_PATH], error[256];
	snprintf(path, sizeof(path), "%s/音色往返.ymmusic", directory);
	for (int instrument = 0; instrument < MS_INSTRUMENTS; ++instrument)
	{
		p->song.track[track].instrument = instrument;
		assert(ms_song_valid(&p->song, p->assets));
		assert(ms_project_save(p, path, error, sizeof(error)));
		assert(ms_project_load(loaded, path, error, sizeof(error)));
		assert(loaded->song.track[track].instrument == instrument);
		assert(ms_preview_project(preview, loaded, 1, track, -1));
		assert(preview->song.clips == 1 && preview->song.clip[0].track == track);
		assert(loaded->song.track[track].mute == 1 && loaded->song.track[0].solo == 1);
		float meters[MS_TRACKS + 1];
		float* output = samples + instrument * 8192;
		ms_render(preview, sounds, 0, 4096, output, meters, 0);
		double energy = 0;
		for (int i = 0; i < 8192; ++i)
		{
			assert(isfinite(output[i]) && fabsf(output[i]) <= 1);
			energy += output[i] * output[i];
		}
		assert(energy > .01 && meters[track] > 0);
		for (int i = 0; i < p->song.tracks; ++i)
			if (i != track)
				assert(meters[i] == 0);
		for (int prior = 0; prior < instrument; ++prior)
		{
			double difference = 0;
			for (int i = 0; i < 8192; ++i)
				difference += fabsf(output[i] - samples[prior * 8192 + i]);
			assert(difference > 1);
		}
		/* 所有采样在低音和最高 MIDI 音域也必须有限且有输出。 */
		const int pitches[] = {24, 84, 127};
		for (int k = 0; k < 3; ++k)
		{
			double hz = 440 * pow(2, (pitches[k] - 69) / 12.0), band_energy = 0;
			for (int frame = 0; frame < 2048; ++frame)
			{
				float sample = ms_instrument_sample(sounds, hz, instrument, frame, MS_RATE);
				assert(isfinite(sample) && fabsf(sample) <= 1);
				band_energy += sample * sample;
			}
			assert(band_energy > .00001);
		}
		/* 任意块边界必须与整段一致，不依赖采样播放游标状态。 */
		float split[8192];
		ms_render(preview, sounds, 0, 777, split, NULL, 0);
		ms_render(preview, sounds, 777, 4096 - 777, split + 1554, NULL, 0);
		assert(!memcmp(output, split, sizeof(split)));
	}
	/* 从实际旧版本字段读入 13–20，映射为对应的四类采样。 */
	const int mapped[] = {0, 4, 9, 6};
	for (int legacy = 13; legacy <= 20; ++legacy)
	{
		p->song.track[track].instrument = 0;
		assert(ms_project_save(p, path, error, sizeof(error)));
		FILE* file = fopen(path, "r+b");
		assert(file && !fseek(file, 180 + track * 92, SEEK_SET));
		unsigned char encoded[4] = {(unsigned char)legacy, 0, 0, 0};
		assert(fwrite(encoded, 1, 4, file) == 4);
		fclose(file);
		assert(ms_project_load(loaded, path, error, sizeof(error)));
		assert(loaded->song.track[track].instrument == mapped[(legacy - 13) % 4]);
	}
	p->song.track[track].instrument = MS_INSTRUMENTS;
	assert(!ms_song_valid(&p->song, p->assets));
	assert(!ms_preview_project(preview, p, -1, track, -1));
	unlink(path);
	/* preview 只借用加载工程的素材，不单独释放。 */
	ms_project_free(loaded);
	ms_project_free(p);
	free(loaded);
	free(preview);
	free(p);
	free(samples);
}

static void test_choir_bank(const MsSounds* sounds, const char* directory)
{
	/* 跨过五秒采样循环边界，不能产生跳变或依赖渲染块顺序。 */
	double hz = 440 * pow(2, (60 - 69) / 12.0);
	int length = MS_RATE * 12, edge = MS_RATE * 5;
	float before = ms_instrument_sample(sounds, hz, 12, edge - 1, length);
	float after = ms_instrument_sample(sounds, hz, 12, edge, length);
	assert(fabsf(after - before) < .08f);
	assert(ms_instrument_sample(sounds, hz, 12, length, length) == 0);
	assert(ms_instrument_sample(sounds, hz, 12, -1, length) == 0);
	for (int pos = edge - 200; pos < edge + 200; ++pos)
	{
		float sample = ms_instrument_sample(sounds, hz, 12, pos, length);
		assert(isfinite(sample) && fabsf(sample) < 1);
	}
	MsProject* p = calloc(1, sizeof(*p));
	assert(p);
	ms_project_init(p, 0);
	p->song.bpm = 60;
	p->song.tracks = p->song.patterns = 1;
	p->song.clips = 0;
	p->song.track[0].kind = MS_SYNTH;
	p->song.track[0].instrument = 12;
	memset(&p->song.pattern[0], 0, sizeof(MsPattern));
	p->song.pattern[0].kind = MS_SYNTH;
	p->song.pattern[0].steps = 32;
	ms_note_toggle(&p->song.pattern[0], 0, 60, 100, 8 * MS_PPQ);
	ms_clip_add(&p->song, 0, 0, 0, 8 * MS_PPQ);
	int count = MS_RATE * 8, cut = edge - 777;
	float* whole = calloc((size_t)count * 2, sizeof(float));
	float* split = calloc((size_t)count * 2, sizeof(float));
	assert(whole && split);
	ms_render(p, sounds, 0, count, whole, NULL, 0);
	ms_render(p, sounds, 0, cut, split, NULL, 0);
	ms_render(p, sounds, cut, count - cut, split + cut * 2, NULL, 0);
	assert(!memcmp(whole, split, (size_t)count * 8));
	ms_project_free(p);
	free(p);
	free(whole);
	free(split);
	MsSounds empty = {0};
	for (int instrument = 0; instrument < MS_INSTRUMENTS; ++instrument)
	{
		assert(ms_instrument_sample(&empty, hz, instrument, 1000, length) == 0);
		/* 非循环采样在自然结束后保持静音，不能重新触发起音。 */
		if (instrument <= 3 || instrument == 10)
			assert(ms_instrument_sample(sounds, hz, instrument, MS_RATE * 7, MS_RATE * 9) == 0);
	}
	assert(ms_instrument_sample(&empty, hz, 12, 1000, length) == 0);
	char path[MS_PATH];
	snprintf(path, sizeof(path), "%s/损坏合唱.bank", directory);
	FILE* f = fopen(path, "wb");
	assert(f);
	fwrite("MSCHOIR1", 1, 8, f);
	fclose(f);
	assert(!ms_sample_bank_load(empty.instrument[12], path));
	for (int zone = 0; zone < MS_SAMPLE_ZONES; ++zone)
		assert(empty.instrument[12][zone] == NULL);
	unlink(path);
	assert(!ms_sample_bank_load(empty.instrument[12], path));
}

static void test_all_fades(const MsSounds* sounds)
{
	MsProject* p = calloc(1, sizeof(*p));
	MsProject* preview = calloc(1, sizeof(*preview));
	const int frames = MS_RATE * 2;
	float* dry = calloc((size_t)frames * 2, sizeof(float));
	float* wet = calloc((size_t)frames * 2, sizeof(float));
	assert(p && preview && dry && wet);
	ms_project_init(p, 0);
	MsSong* s = &p->song;
	s->bpm = 120;
	s->tracks = s->patterns = 1;
	p->assets = 1;
	p->asset[0].frames = MS_RATE * 3;
	p->asset[0].pcm = calloc(MS_RATE * 6, sizeof(float));
	assert(p->asset[0].pcm);
	for (int i = 0; i < MS_RATE * 3; ++i)
		p->asset[0].pcm[i * 2] = p->asset[0].pcm[i * 2 + 1] = .4f * sinf(i * .027f);
	for (int mode = 0; mode < MS_INSTRUMENTS + MS_DRUMS + 2; ++mode)
	{
		/* 全部音色、九种内置鼓、自定义鼓采样、导入 PCM 全走同一淡化规则。 */
		s->clips = 0;
		int kind = mode < MS_INSTRUMENTS ? MS_SYNTH : mode == MS_INSTRUMENTS + MS_DRUMS + 1 ? MS_AUDIO : MS_DRUM;
		s->track[0].kind = kind;
		s->track[0].instrument = mode < MS_INSTRUMENTS ? mode : 0;
		memset(&s->pattern[0], 0, sizeof(MsPattern));
		s->pattern[0].kind = kind == MS_AUDIO ? MS_SYNTH : kind;
		s->pattern[0].steps = 16;
		ms_note_toggle(&s->pattern[0], 0, 60, 100, 4 * MS_PPQ);
		int drum = mode >= MS_INSTRUMENTS && mode < MS_INSTRUMENTS + MS_DRUMS ? mode - MS_INSTRUMENTS : 0;
		for (int d = 0; d < MS_DRUMS; ++d)
			s->drum_asset[d] = -1;
		s->drum_asset[drum] = mode == MS_INSTRUMENTS + MS_DRUMS ? 0 : -1;
		for (int step = 0; step < 16; ++step)
			s->pattern[0].drum[drum][step] = 90;
		int c = ms_clip_add(s, 0, 0, 0, 4 * MS_PPQ);
		assert(c == 0);
		ms_render(p, sounds, 0, frames, dry, NULL, 0);
		s->clip[0].fade_in = .6f;
		s->clip[0].fade_out = .7f;
		ms_render(p, sounds, 0, frames, wet, NULL, 0);
		double head_dry = 0, head_wet = 0, tail_dry = 0, tail_wet = 0;
		for (int frame = 0; frame < frames; ++frame)
		{
			float gain = fminf(1, frame / (.6f * MS_RATE)) * fminf(1, (frames - frame) / (.7f * MS_RATE));
			for (int ch = 0; ch < 2; ++ch)
				assert(fabsf(wet[frame * 2 + ch] - dry[frame * 2 + ch] * gain) < .000002f);
			if (frame < MS_RATE / 4)
			{
				head_dry += dry[frame * 2] * dry[frame * 2];
				head_wet += wet[frame * 2] * wet[frame * 2];
			}
			if (frame >= frames - MS_RATE / 4)
			{
				tail_dry += dry[frame * 2] * dry[frame * 2];
				tail_wet += wet[frame * 2] * wet[frame * 2];
			}
		}
		assert(head_dry > 0 && tail_dry > 0 && head_wet < head_dry * .3 && tail_wet < tail_dry * .3);
		/* 已编排试听保留长度、裁剪、增益和淡化；与整曲中该片段的结果一致。 */
		s->clip[0].gain = .6f;
		s->clip[0].offset = MS_STEP;
		s->clip[0].length -= MS_STEP;
		s->clip[0].start = MS_PPQ;
		assert(ms_preview_project(preview, p, 0, 0, 0));
		assert(preview->song.clip[0].offset == MS_STEP && preview->song.clip[0].fade_out == .7f);
		assert(preview->song.clip[0].gain == .6f && preview->song.loop_end == 4 * MS_PPQ - MS_STEP);
		ms_render(p, sounds, MS_RATE / 2, frames, dry, NULL, 0);
		ms_render(preview, sounds, 0, frames, wet, NULL, 0);
		assert(!memcmp(dry, wet, (size_t)frames * 8));
	}
	/* 快节奏下长吊镲跨越多个节奏周期，和展开节奏的音频必须一致。 */
	s->bpm = 300;
	s->track[0].kind = MS_DRUM;
	s->track[0].volume = .2f;
	s->pattern[0].kind = MS_DRUM;
	s->pattern[0].steps = 16;
	memset(s->pattern[0].drum, 0, sizeof(s->pattern[0].drum));
	for (int d = 0; d < MS_DRUMS; ++d)
		s->drum_asset[d] = -1;
	s->pattern[0].drum[7][0] = 100;
	s->clip[0] = (MsClip){0, 0, 0, 20 * MS_PPQ, 0, 1, 0, 0};
	ms_render(p, sounds, MS_RATE * 33 / 10, 1024, dry, NULL, 0);
	s->pattern[0].steps = 32;
	s->pattern[0].drum[7][16] = 100;
	ms_render(p, sounds, MS_RATE * 33 / 10, 1024, wet, NULL, 0);
	for (int i = 0; i < 2048; ++i)
		assert(fabsf(dry[i] - wet[i]) < .000002f);
	ms_project_free(p);
	free(p);
	free(preview); /* borrowed PCM */
	free(dry);
	free(wet);
}

static void test_percussion(const MsSounds* sounds, const char* directory)
{
	for (int d = 0; d < MS_DRUMS; ++d)
	{
		double energy = 0;
		for (int i = 0; i < sounds->lengths[d]; ++i)
		{
			float v = sounds->drums[d][i];
			assert(isfinite(v) && fabsf(v) <= 1);
			energy += v * v;
		}
		assert(energy > .01);
	}
	/* 响指应为短瞬态，不能带着长噪声尾巴或截断爆音。 */
	double attack = 0, tail = 0;
	for (int i = 0; i < sounds->lengths[8]; ++i)
		if (i < MS_RATE / 50)
			attack += sounds->drums[8][i] * sounds->drums[8][i];
		else if (i >= MS_RATE / 20)
			tail += sounds->drums[8][i] * sounds->drums[8][i];
	assert(attack > .01 && tail < attack * .3);
	assert(fabsf(sounds->drums[8][sounds->lengths[8] - 1]) < .0001f);
	MsProject* p = calloc(1, sizeof(*p));
	MsProject* loaded = calloc(1, sizeof(*loaded));
	assert(p && loaded);
	char path[MS_PATH], error[256];
	assert(ms_project_load(p, MS_TEST_FIXTURES "/legacy_v1.ymmusic", error, sizeof(error)));
	assert(p->song.pattern[0].drum[4][4] == 100 && p->song.clips == 1 && p->assets == 0);
	assert(p->song.clip[0].fade_in == .1f && p->song.clip[0].fade_out == .2f);
	assert(p->song.drum_volume[8] == .8f && p->song.drum_asset[8] == -1);
	for (int i = 0; i < MS_STEPS; ++i)
		assert(p->song.pattern[0].drum[8][i] == 0);
	p->song.pattern[0].drum[8][15] = 115;
	p->song.drum_pan[8] = -.4f;
	snprintf(path, sizeof(path), "%s/九鼓往返.ymmusic", directory);
	assert(ms_project_save(p, path, error, sizeof(error)));
	assert(ms_project_load(loaded, path, error, sizeof(error)));
	assert(loaded->song.pattern[0].drum[8][15] == 115 && loaded->song.drum_pan[8] == -.4f);
	float whole[4096], split[4096];
	int64_t start = (int64_t)ms_tick_frame(&p->song, 15 * MS_STEP);
	ms_render(p, sounds, start, 2048, whole, NULL, 0);
	ms_render(loaded, sounds, start, 777, split, NULL, 0);
	ms_render(loaded, sounds, start + 777, 2048 - 777, split + 1554, NULL, 0);
	assert(!memcmp(whole, split, sizeof(whole)));
	unlink(path);
	ms_project_free(p);
	ms_project_free(loaded);
	free(p);
	free(loaded);
}

int main(int argc, char** argv)
{
	assert(argc == 2);
	MsProject* p = calloc(1, sizeof(*p));
	MsProject* loaded = calloc(1, sizeof(*loaded));
	MsHistory* h = calloc(1, sizeof(*h));
	MsSounds sounds;
	assert(p && loaded && h && ms_sounds_init(&sounds));
	test_instruments(&sounds, argv[1]);
	test_choir_bank(&sounds, argv[1]);
	test_all_fades(&sounds);
	test_percussion(&sounds, argv[1]);
	ms_project_init(p, 1);
	assert(ms_song_valid(&p->song, 0));
	double frame = ms_tick_frame(&p->song, 384);
	assert(fabs(ms_frame_tick(&p->song, frame) - 384) < 1e-8);
	ms_history_push(h, &p->song);
	p->song.bpm = 145;
	assert(ms_history_undo(h, &p->song) && p->song.bpm == 110);
	assert(ms_history_redo(h, &p->song) && p->song.bpm == 145);
	p->song.bpm = 110;
	int count = 65536;
	float* original = calloc((size_t)count * 2, sizeof(float));
	float* split = calloc((size_t)count * 2, sizeof(float));
	float meters[MS_TRACKS + 1];
	ms_render(p, &sounds, 0, count, original, meters, 0);
	double energy = 0;
	for (int i = 0; i < count * 2; ++i)
	{
		assert(isfinite(original[i]) && fabsf(original[i]) <= 1);
		energy += original[i] * original[i];
	}
	assert(energy > 1);
	ms_render(p, &sounds, 0, 1711, split, NULL, 0);
	ms_render(p, &sounds, 1711, count - 1711, split + 3422, NULL, 0);
	assert(!memcmp(original, split, (size_t)count * 8));
	for (int t = 0; t < p->song.tracks; ++t)
		p->song.track[t].mute = 1;
	ms_render(p, &sounds, 0, count, split, NULL, 0);
	for (int i = 0; i < count * 2; ++i)
		assert(split[i] == 0);
	for (int t = 0; t < p->song.tracks; ++t)
		p->song.track[t].mute = 0;
	/* 独奏与硬左声像：其他轨道不得泄漏到右声道。 */
	p->song.track[0].solo = 1;
	p->song.track[0].pan = -1;
	ms_render(p, &sounds, 0, count, split, NULL, 0);
	double left_energy = 0;
	for (int i = 0; i < count; ++i)
	{
		assert(split[2 * i + 1] == 0);
		left_energy += split[2 * i] * split[2 * i];
	}
	assert(left_energy > 1);
	p->song.track[0].solo = 0;
	p->song.track[0].pan = 0;
	int right = ms_clip_split(&p->song, 0, MS_PPQ);
	assert(right >= 0 && p->song.clip[right].offset == MS_PPQ);
	ms_render(p, &sounds, 0, count, split, NULL, 0);
	assert(!memcmp(original, split, (size_t)count * 8));
	/* 淡入只改变片段开头，零淡化重新恢复完全一致的音频。 */
	p->song.clip[0].fade_in = .15f;
	ms_render(p, &sounds, 0, count, split, NULL, 0);
	assert(memcmp(original, split, (size_t)count * 8));
	p->song.clip[0].fade_in = 0;
	ms_track_move(&p->song, 0, 2);
	assert(ms_song_valid(&p->song, 0));
	ms_track_delete(&p->song, 1);
	assert(ms_song_valid(&p->song, 0));
	p->assets = 1;
	p->asset[0].frames = 256;
	p->asset[0].pcm = calloc(512, sizeof(float));
	strcpy(p->asset[0].name, "中文素材");
	for (int i = 0; i < 512; ++i)
		p->asset[0].pcm[i] = .2f * sinf(i * .1f);
	p->song.drum_asset[1] = 0;
	ms_asset_peaks(&p->asset[0]);
	char path[MS_PATH], wav[MS_PATH], error[256];
	snprintf(path, sizeof(path), "%s/测试工程.ymmusic", argv[1]);
	snprintf(wav, sizeof(wav), "%s/测试混音.wav", argv[1]);
	assert(ms_project_save(p, path, error, sizeof(error)));
	assert(ms_project_load(loaded, path, error, sizeof(error)));
	assert(loaded->assets == 1 && !strcmp(loaded->asset[0].name, "中文素材"));
	assert(!memcmp(loaded->asset[0].pcm, p->asset[0].pcm, 512 * sizeof(float)));
	ms_render(p, &sounds, 0, count, original, NULL, 0);
	ms_render(loaded, &sounds, 0, count, split, NULL, 0);
	assert(!memcmp(original, split, (size_t)count * 8));
	assert(ms_export(loaded, &sounds, wav, 0, error, sizeof(error)));
	FILE* f = fopen(wav, "rb");
	char head[44];
	assert(f && fread(head, 1, 44, f) == 44 && !memcmp(head, "RIFF", 4) && !memcmp(head + 8, "WAVE", 4));
	fclose(f);
	MsAsset decoded;
	assert(ms_asset_import(&decoded, wav, error, sizeof(error)));
	assert(decoded.frames > 0);
	free(decoded.pcm);
	f = fopen(path, "wb");
	assert(f);
	fwrite("YMUSIC01", 1, 8, f);
	fclose(f);
	int bpm = loaded->song.bpm;
	assert(!ms_project_load(loaded, path, error, sizeof(error)));
	assert(loaded->song.bpm == bpm && loaded->assets == 1);
	p->song.bpm = 0;
	assert(!ms_project_save(p, path, error, sizeof(error)));
	f = fopen(path, "rb");
	assert(f);
	fseek(f, 0, SEEK_END);
	assert(ftell(f) == 8);
	fclose(f);
	unlink(path);
	ms_project_free(p);
	ms_project_free(loaded);
	ms_sounds_free(&sounds);
	free(p);
	free(loaded);
	free(h);
	free(original);
	free(split);
	puts("模型、时钟、撤销、分割、混音、静音、工程素材往返、损坏保护、真实 WAV 导入导出通过");
	return 0;
}
