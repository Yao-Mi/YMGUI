/* Copyright (c) 2026. Native C edit model; timeline units are project frames. */
#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
void st_project_init(StProject* p)
{
	memset(p, 0, sizeof(*p));
	p->track_count = 2;
	p->next_id = 1;
	for (int t = 0; t < ST_TRACKS; t++)
		p->tracks[t].visible = 1;
}
const char* st_basename(const char* s)
{
	const char* b = strrchr(s, '/');
	return b ? b + 1 : s;
}
int st_duration(const StProject* p)
{
	int end = 0;
	for (int t = 0; t < p->track_count; t++)
		for (int i = 0; i < p->tracks[t].count; i++)
		{
			StClip c = p->tracks[t].clips[i];
			if (c.start + c.length > end)
				end = c.start + c.length;
		}
	return end;
}
const StClip* st_clip(const StProject* p, int id, int* track)
{
	for (int t = 0; t < p->track_count; t++)
		for (int i = 0; i < p->tracks[t].count; i++)
			if (p->tracks[t].clips[i].id == id)
			{
				if (track)
					*track = t;
				return &p->tracks[t].clips[i];
			}
	return NULL;
}
const StClip* st_visible(const StProject* p, int frame)
{
	for (int t = p->track_count - 1; t >= 0; t--)
		if (p->tracks[t].visible)
			for (int i = 0; i < p->tracks[t].count; i++)
			{
				const StClip* c = &p->tracks[t].clips[i];
				if (frame >= c->start && frame < c->start + c->length)
					return c;
			}
	return NULL;
}
int st_project_valid(const StProject* p)
{
	if (p->media_count < 0 || p->media_count > ST_MEDIA || p->track_count < 1 || p->track_count > ST_TRACKS || p->next_id < 1)
		return 0;
	for (int m = 0; m < p->media_count; m++)
	{
		const StMedia* a = &p->media[m];
		if (!memchr(a->path, 0, sizeof(a->path)) || !a->path[0] || a->frames <= 0 || a->frames > ST_MAX_FRAME || a->width <= 0 || a->height <= 0 || a->width > 32768 || a->height > 32768)
			return 0;
	}
	int ids[ST_TRACKS * ST_CLIPS], n = 0;
	for (int t = 0; t < p->track_count; t++)
	{
		const StTrack* tr = &p->tracks[t];
		int end = 0;
		if (tr->count < 0 || tr->count > ST_CLIPS || (tr->visible != 0 && tr->visible != 1) || (tr->locked != 0 && tr->locked != 1))
			return 0;
		for (int i = 0; i < tr->count; i++)
		{
			const StClip* c = &tr->clips[i];
			if (c->id <= 0 || c->id >= p->next_id || c->media < 0 || c->media >= p->media_count || c->start < end || c->start > ST_MAX_FRAME || c->in < 0 || c->length <= 0 || c->length > ST_MAX_FRAME - c->start || c->in > p->media[c->media].frames - c->length)
				return 0;
			for (int j = 0; j < n; j++)
				if (ids[j] == c->id)
					return 0;
			ids[n++] = c->id;
			end = c->start + c->length;
		}
	}
	return 1;
}
static int cmp(const void* a, const void* b)
{
	const StClip* x = a;
	const StClip* y = b;
	return (x->start > y->start) - (x->start < y->start);
}
static void erase(StTrack* t, int i)
{
	memmove(t->clips + i, t->clips + i + 1, (size_t)(t->count - i - 1) * sizeof(StClip));
	memset(&t->clips[--t->count], 0, sizeof(StClip));
}
static int mutate(StProject* p, StEdit e)
{
	if (e.kind == ST_TRACK_ADD)
	{
		if (p->track_count == ST_TRACKS)
			return 0;
		p->tracks[p->track_count++].visible = 1;
		return 1;
	}
	if (e.track < 0 || e.track >= p->track_count)
		return 0;
	StTrack* dst = &p->tracks[e.track];
	if (e.kind == ST_TRACK_LOCK)
	{
		dst->locked = !dst->locked;
		return 1;
	}
	if (dst->locked)
		return 0;
	if (e.kind == ST_TRACK_HIDE)
	{
		dst->visible = !dst->visible;
		return 1;
	}
	if (e.kind == ST_ADD)
	{
		if (e.media < 0 || e.media >= p->media_count || dst->count == ST_CLIPS || p->next_id == INT_MAX)
			return 0;
		dst->clips[dst->count++] = (StClip){p->next_id++, e.media, e.at, e.in, e.length};
	}
	else
	{
		int t = -1;
		const StClip* found = st_clip(p, e.id, &t);
		if (!found || p->tracks[t].locked)
			return 0;
		StTrack* src = &p->tracks[t];
		int i = (int)(found - src->clips);
		StClip c = *found;
		switch (e.kind)
		{
		case ST_MOVE:
			if (t == e.track && c.start == e.at)
				return 0;
			if (t != e.track && dst->count == ST_CLIPS)
				return 0;
			erase(src, i);
			c.start = e.at;
			dst->clips[dst->count++] = c;
			break;
		case ST_TRIM:
			if (c.in == e.in && c.length == e.length)
				return 0;
			src->clips[i].start += e.in - c.in;
			src->clips[i].in = e.in;
			src->clips[i].length = e.length;
			break;
		case ST_SPLIT:
		{
			int offset = e.at - c.start;
			if (offset <= 0 || offset >= c.length || src->count == ST_CLIPS || p->next_id == INT_MAX)
				return 0;
			src->clips[i].length = offset;
			c.id = p->next_id++;
			c.start += offset;
			c.in += offset;
			c.length -= offset;
			src->clips[src->count++] = c;
			break;
		}
		case ST_LIFT:
		case ST_RIPPLE:
			erase(src, i);
			if (e.kind == ST_RIPPLE)
				for (int j = i; j < src->count; j++)
					src->clips[j].start -= c.length;
			break;
		default:
			return 0;
		}
		qsort(src->clips, src->count, sizeof(StClip), cmp);
	}
	qsort(dst->clips, dst->count, sizeof(StClip), cmp);
	return 1;
}
int st_project_edit(StProject* p, StEdit e)
{
	if (!st_project_valid(p) || e.at < 0 || e.at > ST_MAX_FRAME || e.in < 0 || e.in > ST_MAX_FRAME || e.length < 0 || e.length > ST_MAX_FRAME)
		return 0;
	StProject* next = malloc(sizeof(*next));
	if (!next)
		return 0;
	*next = *p;
	int ok = mutate(next, e) && st_project_valid(next);
	if (ok)
		*p = *next;
	free(next);
	return ok;
}
/* Text format, hex paths preserve whitespace and non-ASCII file names. Atomic replace. */
int st_project_save(const StProject* p, const char* path)
{
	if (!st_project_valid(p) || strlen(path) > 4000)
		return 0;
	char temp[4096];
	snprintf(temp, sizeof(temp), "%s.tmp-XXXXXX", path);
	int fd = mkstemp(temp);
	if (fd < 0)
		return 0;
	FILE* f = fdopen(fd, "w");
	if (!f)
	{
		close(fd);
		unlink(temp);
		return 0;
	}
	fprintf(f, "VIDEO_STIDIO 1 %d %d %d %d\n", ST_FPS, p->media_count, p->track_count, p->next_id);
	for (int m = 0; m < p->media_count; m++)
	{
		const StMedia* a = &p->media[m];
		fprintf(f, "M %d %d %d ", a->frames, a->width, a->height);
		for (const unsigned char* s = (const unsigned char*)a->path; *s; s++)
			fprintf(f, "%02x", *s);
		fputc('\n', f);
	}
	for (int t = 0; t < p->track_count; t++)
	{
		const StTrack* tr = &p->tracks[t];
		fprintf(f, "T %d %d %d\n", tr->count, tr->visible, tr->locked);
		for (int i = 0; i < tr->count; i++)
		{
			StClip c = tr->clips[i];
			fprintf(f, "C %d %d %d %d %d\n", c.id, c.media, c.start, c.in, c.length);
		}
	}
	int ok = !ferror(f);
	if (fflush(f) || fsync(fd))
		ok = 0;
	if (fclose(f))
		ok = 0;
	if (ok)
		ok = rename(temp, path) == 0;
	if (!ok)
		unlink(temp);
	return ok;
}
static int unhex(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}
static int read_tag(FILE* f, const char* tag)
{
	char token[32];
	return fscanf(f, "%31s", token) == 1 && !strcmp(token, tag);
}
static int read_int(FILE* f, int* value)
{
	char token[64], *end;
	if (fscanf(f, "%63s", token) != 1)
		return 0;
	errno = 0;
	long n = strtol(token, &end, 10);
	if (errno || *end || end == token || n < INT_MIN || n > INT_MAX)
		return 0;
	*value = (int)n;
	return 1;
}
int st_project_load(StProject* p, const char* path)
{
	FILE* f = fopen(path, "r");
	if (!f)
		return 0;
	StProject* q = calloc(1, sizeof(*q));
	int version, fps;
	int ok = 0;
	if (!q)
		goto done;
	st_project_init(q);
	if (!read_tag(f, "VIDEO_STIDIO") || !read_int(f, &version) || !read_int(f, &fps) || !read_int(f, &q->media_count) || !read_int(f, &q->track_count) || !read_int(f, &q->next_id) || version != 1 || fps != ST_FPS || q->media_count < 0 || q->media_count > ST_MEDIA || q->track_count < 1 || q->track_count > ST_TRACKS)
		goto done;
	for (int m = 0; m < q->media_count; m++)
	{
		StMedia* a = &q->media[m];
		char hex[2049];
		if (!read_tag(f, "M") || !read_int(f, &a->frames) || !read_int(f, &a->width) || !read_int(f, &a->height) || fscanf(f, "%2048s", hex) != 1)
			goto done;
		size_t n = strlen(hex);
		if (n % 2 || n / 2 >= sizeof(a->path))
			goto done;
		for (size_t i = 0; i < n; i += 2)
		{
			int x = unhex(hex[i]), y = unhex(hex[i + 1]);
			if (x < 0 || y < 0 || !(x * 16 + y))
				goto done;
			a->path[i / 2] = (char)(x * 16 + y);
		}
	}
	for (int t = 0; t < q->track_count; t++)
	{
		StTrack* tr = &q->tracks[t];
		if (!read_tag(f, "T") || !read_int(f, &tr->count) || !read_int(f, &tr->visible) || !read_int(f, &tr->locked) || tr->count < 0 || tr->count > ST_CLIPS)
			goto done;
		for (int i = 0; i < tr->count; i++)
		{
			StClip* c = &tr->clips[i];
			if (!read_tag(f, "C") || !read_int(f, &c->id) || !read_int(f, &c->media) || !read_int(f, &c->start) || !read_int(f, &c->in) || !read_int(f, &c->length))
				goto done;
		}
	}
	{
		char extra;
		if (fscanf(f, " %c", &extra) != EOF || !st_project_valid(q))
			goto done;
	}
	*p = *q;
	ok = 1;
done:
	free(q);
	fclose(f);
	return ok;
}
