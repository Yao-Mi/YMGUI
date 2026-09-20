#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>

typedef struct
{
	FILE* file;
	int read, ok;
} Stream;
static void bytes(Stream* f, void* p, size_t n)
{
	if (!f->ok)
		return;
	f->ok = (f->read ? fread(p, 1, n, f->file) : fwrite(p, 1, n, f->file)) == n;
}
static void integer(Stream* f, int* n)
{
	uint32_t v = (uint32_t)*n;
	unsigned char b[4] = {v, v >> 8, v >> 16, v >> 24};
	bytes(f, b, 4);
	if (f->read)
		*n = (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
}
static void real(Stream* f, float* value)
{
	int v;
	memcpy(&v, value, 4);
	integer(f, &v);
	if (f->read)
		memcpy(value, &v, 4);
}
static void name(Stream* f, char* value)
{
	bytes(f, value, MS_NAME);
	if (!memchr(value, 0, MS_NAME))
		f->ok = 0;
}
static void song(Stream* f, MsSong* s, int drums)
{
	name(f, s->name);
	integer(f, &s->bpm);
	integer(f, &s->beats);
	integer(f, &s->loop_start);
	integer(f, &s->loop_end);
	real(f, &s->master);
	integer(f, &s->tracks);
	integer(f, &s->patterns);
	integer(f, &s->clips);
	if (s->tracks < 0 || s->tracks > MS_TRACKS || s->patterns < 0 || s->patterns > MS_PATTERNS || s->clips < 0 || s->clips > MS_CLIPS)
	{
		f->ok = 0;
		return;
	}
	for (int i = 0; i < s->tracks; ++i)
	{
		MsTrack* t = &s->track[i];
		int c = (int)t->color;
		name(f, t->name);
		integer(f, &t->kind);
		integer(f, &t->mute);
		integer(f, &t->solo);
		integer(f, &t->instrument);
		/* 上一版 13–16 为四类采样，17–20 为其特征合成；统一到采样编号。 */
		if (f->read && t->instrument >= 13 && t->instrument <= 20)
		{
			static const int legacy[] = {0, 4, 9, 6};
			t->instrument = legacy[(t->instrument - 13) % 4];
		}
		real(f, &t->volume);
		real(f, &t->pan);
		integer(f, &c);
		t->color = (uint32_t)c;
	}
	for (int d = 0; d < drums; ++d)
	{
		real(f, &s->drum_volume[d]);
		real(f, &s->drum_pan[d]);
		integer(f, &s->drum_mute[d]);
		integer(f, &s->drum_solo[d]);
		integer(f, &s->drum_asset[d]);
	}
	for (int i = 0; i < s->patterns; ++i)
	{
		MsPattern* p = &s->pattern[i];
		name(f, p->name);
		integer(f, &p->kind);
		integer(f, &p->steps);
		integer(f, &p->count);
		bytes(f, p->drum, (size_t)drums * MS_STEPS);
		if (p->count < 0 || p->count > MS_NOTES)
		{
			f->ok = 0;
			return;
		}
		for (int n = 0; n < p->count; ++n)
		{
			MsNote* v = &p->notes[n];
			integer(f, &v->tick);
			integer(f, &v->length);
			integer(f, &v->pitch);
			integer(f, &v->velocity);
		}
	}
	for (int i = 0; i < s->clips; ++i)
	{
		MsClip* c = &s->clip[i];
		integer(f, &c->track);
		integer(f, &c->source);
		integer(f, &c->start);
		integer(f, &c->length);
		integer(f, &c->offset);
		real(f, &c->gain);
		real(f, &c->fade_in);
		real(f, &c->fade_out);
	}
}
static void pcm(Stream* f, float* samples, int count)
{
	unsigned char b[4096];
	for (int off = 0; off < count && f->ok;)
	{
		int n = count - off;
		if (n > 1024)
			n = 1024;
		if (!f->read)
			for (int i = 0; i < n; ++i)
			{
				uint32_t v;
				memcpy(&v, &samples[off + i], 4);
				b[i * 4] = v;
				b[i * 4 + 1] = v >> 8;
				b[i * 4 + 2] = v >> 16;
				b[i * 4 + 3] = v >> 24;
			}
		bytes(f, b, (size_t)n * 4);
		if (!f->ok)
			break;
		if (f->read)
			for (int i = 0; i < n; ++i)
			{
				uint32_t v = (uint32_t)b[i * 4] | (uint32_t)b[i * 4 + 1] << 8 | (uint32_t)b[i * 4 + 2] << 16 | (uint32_t)b[i * 4 + 3] << 24;
				memcpy(&samples[off + i], &v, 4);
				if (!isfinite(samples[off + i]) || fabsf(samples[off + i]) > 1.f)
					f->ok = 0;
			}
		off += n;
	}
}
int ms_project_save(const MsProject* p, const char* path, char* error, size_t cap)
{
	if (!ms_song_valid(&p->song, p->assets))
	{
		snprintf(error, cap, "工程数据不合法");
		return 0;
	}
	char temp[MS_PATH + 32];
	if (strlen(path) >= MS_PATH)
	{
		snprintf(error, cap, "路径太长");
		return 0;
	}
	snprintf(temp, sizeof(temp), "%s.tmp.XXXXXX", path);
	int fd = mkstemp(temp);
	FILE* file = fd >= 0 ? fdopen(fd, "wb") : NULL;
	if (!file)
	{
		if (fd >= 0)
		{
			close(fd);
			unlink(temp);
		}
		snprintf(error, cap, "无法创建工程文件");
		return 0;
	}
	Stream f = {file, 0, 1};
	char magic[] = "YMUSIC02";
	bytes(&f, magic, 8);
	MsSong copy = p->song;
	song(&f, &copy, MS_DRUMS);
	int count = p->assets, total = 0;
	integer(&f, &count);
	for (int i = 0; i < count && f.ok; ++i)
	{
		MsAsset a = p->asset[i];
		total += a.frames;
		if (!a.pcm || a.frames < 1 || a.frames > MS_ASSET_FRAMES || total > MS_TOTAL_FRAMES)
		{
			f.ok = 0;
			break;
		}
		name(&f, a.name);
		integer(&f, &a.frames);
		pcm(&f, a.pcm, a.frames * 2);
	}
	int footer = 0x1234abcd;
	integer(&f, &footer);
	int ok = f.ok && fflush(file) == 0 && fsync(fd) == 0;
	if (fclose(file))
		ok = 0;
	if (ok && rename(temp, path) == 0)
		return 1;
	unlink(temp);
	snprintf(error, cap, "保存失败，原工程未替换");
	return 0;
}
int ms_project_load(MsProject* p, const char* path, char* error, size_t cap)
{
	FILE* file = fopen(path, "rb");
	if (!file)
	{
		snprintf(error, cap, "无法打开工程");
		return 0;
	}
	MsProject* candidate = calloc(1, sizeof(*candidate));
	if (!candidate)
	{
		fclose(file);
		snprintf(error, cap, "内存不足");
		return 0;
	}
	Stream f = {file, 1, 1};
	char magic[8];
	bytes(&f, magic, 8);
	int drums = f.ok && !memcmp(magic, "YMUSIC01", 8) ? 8 : MS_DRUMS;
	if (!f.ok || (memcmp(magic, "YMUSIC01", 8) && memcmp(magic, "YMUSIC02", 8)))
		f.ok = 0;
	for (int d = 0; d < MS_DRUMS; ++d)
	{
		candidate->song.drum_volume[d] = .8f;
		candidate->song.drum_asset[d] = -1;
	}
	if (f.ok)
		song(&f, &candidate->song, drums);
	integer(&f, &candidate->assets);
	if (!ms_song_valid(&candidate->song, candidate->assets))
		f.ok = 0;
	if (candidate->assets < 0 || candidate->assets > MS_ASSETS)
		candidate->assets = 0;
	int total = 0;
	struct stat st;
	if (fstat(fileno(file), &st) || !S_ISREG(st.st_mode))
		f.ok = 0;
	for (int i = 0; i < candidate->assets && f.ok; ++i)
	{
		MsAsset* a = &candidate->asset[i];
		name(&f, a->name);
		integer(&f, &a->frames);
		if (!f.ok || a->frames < 1 || a->frames > MS_ASSET_FRAMES || a->frames > MS_TOTAL_FRAMES - total || (int64_t)a->frames * 8 > st.st_size - ftell(file))
		{
			f.ok = 0;
			break;
		}
		total += a->frames;
		a->pcm = malloc((size_t)a->frames * 2 * sizeof(float));
		if (!a->pcm)
		{
			f.ok = 0;
			break;
		}
		pcm(&f, a->pcm, a->frames * 2);
		ms_asset_peaks(a);
	}
	int footer = 0;
	integer(&f, &footer);
	if (footer != 0x1234abcd || fgetc(file) != EOF || ferror(file))
		f.ok = 0;
	fclose(file);
	if (f.ok)
	{
		ms_project_free(p);
		*p = *candidate;
		free(candidate);
		return 1;
	}
	ms_project_free(candidate);
	free(candidate);
	snprintf(error, cap, "工程损坏、版本不兼容或素材超出限制；当前工程已保留");
	return 0;
}
