/* Copyright (c) 2026. FFmpeg contexts are owned exclusively by the media thread. */
#include "engine.h"
#include "proxy.h"
#include "decoder.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define DECODERS 4
#define CACHE 24
#define THUMB_QUEUE 32

typedef struct
{
	char path[1024];
	int frame;
	uint64_t used;
	uint16_t* pixels;
} Cached;
struct StEngine
{
	pthread_t thread;
	StProxy* proxy;
	pthread_mutex_t mutex;
	pthread_cond_t wake;
	atomic_int stop;
	StDecodeControl control;
	char imports[ST_MEDIA][1024];
	int in_head, in_count;
	struct
	{
		char path[1024];
		int frame;
	} thumbnails[THUMB_QUEUE];
	StThumbnailResult thumbnail_results[THUMB_QUEUE];
	int thumb_head, thumb_count, thumb_out, thumb_ready;
	StImportResult results[ST_MEDIA];
	int out_head, out_count;
	char pending_path[1024];
	int pending_frame;
	uint64_t serial, handled;
	uint16_t* output;
	int ready;
	StFrameResult result;
	StDecoder decoders[DECODERS];
	Cached cache[CACHE];
	uint64_t clock;
};
static StDecoder* decoder_for(StEngine* e, const char* path, int* error)
{
	StDecoder* oldest = &e->decoders[0];
	for (int i = 0; i < DECODERS; i++)
	{
		StDecoder* d = &e->decoders[i];
		if (!strcmp(d->path, path))
		{
			d->used = ++e->clock;
			return d;
		}
		if (d->used < oldest->used)
			oldest = d;
	}
	*error = st_decoder_open(&e->control, oldest, path);
	if (*error < 0)
	{
		st_decoder_close(oldest);
		return NULL;
	}
	oldest->used = ++e->clock;
	return oldest;
}
static int render(StEngine* e, const char* path, int frame, uint16_t* pixels)
{
	char proxy_path[1024];
	if (st_proxy_lookup(e->proxy, path, proxy_path, sizeof(proxy_path)) == 2)
		path = proxy_path;
	if (!path[0])
	{
		memset(pixels, 0, ST_PIXELS * sizeof(*pixels));
		return 0;
	}
	Cached* oldest = &e->cache[0];
	for (int i = 0; i < CACHE; i++)
	{
		Cached* c = &e->cache[i];
		if (c->used && c->frame == frame && !strcmp(c->path, path))
		{
			memcpy(pixels, c->pixels, ST_PIXELS * sizeof(*pixels));
			c->used = ++e->clock;
			return 0;
		}
		if (c->used < oldest->used)
			oldest = c;
	}
	int r = 0;
	StDecoder* d = decoder_for(e, path, &r);
	if (!d)
		return r;
	if ((r = st_decoder_read(&e->control, d, frame, (uint8_t*)pixels, ST_PREVIEW_W, ST_PREVIEW_H, AV_PIX_FMT_RGB565LE)) < 0)
		return r;
	if (!oldest->pixels)
		oldest->pixels = malloc(ST_PIXELS * sizeof(*pixels));
	if (oldest->pixels)
	{
		memcpy(oldest->pixels, pixels, ST_PIXELS * sizeof(*pixels));
		oldest->frame = frame;
		snprintf(oldest->path, sizeof(oldest->path), "%s", path);
		oldest->used = ++e->clock;
	}
	return 0;
}
static void probe(StEngine* e, const char* path, StImportResult* out, uint16_t* pixels)
{
	memset(out, 0, sizeof(*out));
	snprintf(out->media.path, sizeof(out->media.path), "%s", path);
	int r = 0;
	StDecoder* d = decoder_for(e, path, &r);
	if (d)
	{
		AVStream* s = d->fmt->streams[d->stream];
		int64_t us = d->fmt->duration;
		if (us <= 0 || us == AV_NOPTS_VALUE)
			us = s->duration == AV_NOPTS_VALUE ? 0 : av_rescale_q(s->duration, s->time_base, AV_TIME_BASE_Q);
		int64_t frames = av_rescale_rnd(us, ST_FPS, AV_TIME_BASE, AV_ROUND_UP);
		if (frames <= 0 || frames > ST_MAX_FRAME)
			r = AVERROR_INVALIDDATA;
		else
		{
			out->media.frames = (int)frames;
			out->media.width = d->codec->width;
			out->media.height = d->codec->height;
			r = render(e, path, 0, pixels);
		}
	}
	out->ok = r >= 0;
	if (r < 0)
		av_strerror(r, out->error, sizeof(out->error));
	else
		for (int y = 0; y < 90; y++)
			for (int x = 0; x < 160; x++)
				out->thumb[y * 160 + x] = pixels[y * 4 * ST_PREVIEW_W + x * 4];
}
static void* worker(void* user)
{
	StEngine* e = user;
	uint16_t* pixels = malloc(ST_PIXELS * sizeof(*pixels));
	if (!pixels)
	{
		atomic_store(&e->stop, 1);
		return NULL;
	}
	int render_streak = 0;
	for (;;)
	{
		pthread_mutex_lock(&e->mutex);
		while (!atomic_load(&e->stop) && e->serial == e->handled && (!e->in_count || e->out_count == ST_MEDIA) && (!e->thumb_count || e->thumb_ready == THUMB_QUEUE))
			pthread_cond_wait(&e->wake, &e->mutex);
		if (atomic_load(&e->stop))
		{
			pthread_mutex_unlock(&e->mutex);
			break;
		}
		char path[1024];
		int frame = 0;
		uint64_t serial = 0;
		int is_frame = e->serial != e->handled;
		int is_thumbnail = 0;
		if (e->in_count && e->out_count < ST_MEDIA && render_streak >= 2)
			is_frame = 0;
		render_streak = is_frame ? render_streak + 1 : 0;
		if (is_frame)
		{
			serial = e->serial;
			frame = e->pending_frame;
			strcpy(path, e->pending_path);
			e->handled = serial;
		}
		else if (e->in_count && e->out_count < ST_MEDIA)
		{
			strcpy(path, e->imports[e->in_head]);
			e->in_head = (e->in_head + 1) % ST_MEDIA;
			e->in_count--;
		}
		else
		{
			is_thumbnail = 1;
			strcpy(path, e->thumbnails[e->thumb_head].path);
			frame = e->thumbnails[e->thumb_head].frame;
			e->thumb_head = (e->thumb_head + 1) % THUMB_QUEUE;
			e->thumb_count--;
		}
		pthread_mutex_unlock(&e->mutex);
		e->control.deadline = av_gettime_relative() + 5000000;
		if (is_thumbnail)
		{
			StThumbnailResult result = {.frame = frame};
			strcpy(result.path, path);
			result.ok = render(e, path, frame, pixels) >= 0;
			if (result.ok)
				for (int y = 0; y < ST_THUMB_H; y++)
					for (int x = 0; x < ST_THUMB_W; x++)
						result.pixels[y * ST_THUMB_W + x] = pixels[y * 8 * ST_PREVIEW_W + x * 8];
			pthread_mutex_lock(&e->mutex);
			e->thumbnail_results[(e->thumb_out + e->thumb_ready) % THUMB_QUEUE] = result;
			e->thumb_ready++;
			pthread_mutex_unlock(&e->mutex);
		}
		else if (is_frame)
		{
			int64_t started = av_gettime_relative();
			int r = render(e, path, frame, pixels);
			StFrameResult result = {.serial = serial, .frame = frame, .ok = r >= 0, .elapsed_ms = (av_gettime_relative() - started) / 1000.0};
			if (r < 0)
			{
				av_strerror(r, result.error, sizeof(result.error));
				memset(pixels, 0, ST_PIXELS * sizeof(*pixels));
			}
			pthread_mutex_lock(&e->mutex);
			memcpy(e->output, pixels, ST_PIXELS * sizeof(*pixels));
			e->result = result;
			e->ready = 1;
			pthread_mutex_unlock(&e->mutex);
		}
		else
		{
			StImportResult result;
			probe(e, path, &result, pixels);
			if (result.ok)
				st_proxy_queue(e->proxy, path);
			pthread_mutex_lock(&e->mutex);
			e->results[(e->out_head + e->out_count) % ST_MEDIA] = result;
			e->out_count++;
			pthread_mutex_unlock(&e->mutex);
		}
	}
	free(pixels);
	for (int i = 0; i < DECODERS; i++)
		st_decoder_close(&e->decoders[i]);
	for (int i = 0; i < CACHE; i++)
		free(e->cache[i].pixels);
	return NULL;
}
StEngine* st_engine_create(void)
{
	StEngine* e = calloc(1, sizeof(*e));
	if (!e)
		return NULL;
	atomic_init(&e->stop, 0);
	e->control.stop = &e->stop;
	e->proxy = st_proxy_create();
	e->output = calloc(ST_PIXELS, sizeof(uint16_t));
	if (!e->output)
	{
		st_proxy_destroy(e->proxy);
		free(e);
		return NULL;
	}
	if (pthread_mutex_init(&e->mutex, NULL))
		goto fail;
	if (pthread_cond_init(&e->wake, NULL))
	{
		pthread_mutex_destroy(&e->mutex);
		goto fail;
	}
	if (pthread_create(&e->thread, NULL, worker, e))
	{
		pthread_cond_destroy(&e->wake);
		pthread_mutex_destroy(&e->mutex);
		goto fail;
	}
	return e;
fail:
	st_proxy_destroy(e->proxy);
	free(e->output);
	free(e);
	return NULL;
}
void st_engine_destroy(StEngine* e)
{
	if (!e)
		return;
	atomic_store(&e->stop, 1);
	pthread_mutex_lock(&e->mutex);
	pthread_cond_signal(&e->wake);
	pthread_mutex_unlock(&e->mutex);
	pthread_join(e->thread, NULL);
	st_proxy_destroy(e->proxy);
	pthread_cond_destroy(&e->wake);
	pthread_mutex_destroy(&e->mutex);
	free(e->output);
	free(e);
}
int st_engine_import(StEngine* e, const char* path)
{
	if (!path || !path[0] || strlen(path) >= 1024 || atomic_load(&e->stop))
		return 0;
	pthread_mutex_lock(&e->mutex);
	int ok = e->in_count < ST_MEDIA;
	if (ok)
	{
		strcpy(e->imports[(e->in_head + e->in_count) % ST_MEDIA], path);
		e->in_count++;
		pthread_cond_signal(&e->wake);
	}
	pthread_mutex_unlock(&e->mutex);
	return ok;
}
int st_engine_poll_import(StEngine* e, StImportResult* out)
{
	pthread_mutex_lock(&e->mutex);
	int ok = e->out_count > 0;
	if (ok)
	{
		*out = e->results[e->out_head];
		e->out_head = (e->out_head + 1) % ST_MEDIA;
		e->out_count--;
		pthread_cond_signal(&e->wake);
	}
	pthread_mutex_unlock(&e->mutex);
	return ok;
}
uint64_t st_engine_request(StEngine* e, const char* path, int frame)
{
	if (!path || strlen(path) >= 1024 || frame < 0 || frame > ST_MAX_FRAME)
		return 0;
	pthread_mutex_lock(&e->mutex);
	strcpy(e->pending_path, path);
	e->pending_frame = frame;
	uint64_t serial = ++e->serial;
	pthread_cond_signal(&e->wake);
	pthread_mutex_unlock(&e->mutex);
	return serial;
}
int st_engine_poll_frame(StEngine* e, uint16_t* pixels, StFrameResult* out)
{
	pthread_mutex_lock(&e->mutex);
	int ok = e->ready;
	if (ok)
	{
		*out = e->result;
		memcpy(pixels, e->output, ST_PIXELS * sizeof(*pixels));
		e->ready = 0;
	}
	pthread_mutex_unlock(&e->mutex);
	return ok;
}

int st_engine_proxy_state(StEngine* e, const char* source)
{
	return st_proxy_lookup(e->proxy, source, NULL, 0);
}

int st_engine_thumbnail(StEngine* e, const char* path, int frame)
{
	if (!path || strlen(path) >= 1024 || frame < 0 || frame > ST_MAX_FRAME)
		return 0;
	pthread_mutex_lock(&e->mutex);
	int ok = e->thumb_count < THUMB_QUEUE;
	if (ok)
	{
		int i = (e->thumb_head + e->thumb_count) % THUMB_QUEUE;
		strcpy(e->thumbnails[i].path, path);
		e->thumbnails[i].frame = frame;
		e->thumb_count++;
		pthread_cond_signal(&e->wake);
	}
	pthread_mutex_unlock(&e->mutex);
	return ok;
}
int st_engine_poll_thumbnail(StEngine* e, StThumbnailResult* result)
{
	pthread_mutex_lock(&e->mutex);
	int ok = e->thumb_ready > 0;
	if (ok)
	{
		*result = e->thumbnail_results[e->thumb_out];
		e->thumb_out = (e->thumb_out + 1) % THUMB_QUEUE;
		e->thumb_ready--;
		pthread_cond_signal(&e->wake);
	}
	pthread_mutex_unlock(&e->mutex);
	return ok;
}
