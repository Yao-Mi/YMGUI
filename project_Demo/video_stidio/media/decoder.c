/* Copyright (c) 2026. Shared source-frame sampling for preview and export. */
#include "decoder.h"
#include "model/project.h"
#include <libavutil/time.h>
#include <stdio.h>
#include <string.h>
static int interrupt_io(void* opaque)
{
	StDecodeControl* e = opaque;
	return atomic_load(e->stop) || av_gettime_relative() > e->deadline;
}
void st_decoder_close(StDecoder* d)
{
	av_packet_free(&d->packet);
	av_frame_free(&d->frame);
	av_frame_free(&d->scratch);
	avcodec_free_context(&d->codec);
	avformat_close_input(&d->fmt);
	sws_freeContext(d->scale);
	memset(d, 0, sizeof(*d));
}
int st_decoder_open(StDecodeControl* e, StDecoder* d, const char* path)
{
	if (!e || !e->stop || !d || !path || !path[0] || strlen(path) >= sizeof(d->path))
		return AVERROR(EINVAL);
	st_decoder_close(d);
	d->last_us = -1;
	d->fmt = avformat_alloc_context();
	if (!d->fmt)
		return AVERROR(ENOMEM);
	d->fmt->interrupt_callback = (AVIOInterruptCB){interrupt_io, e};
	int r = avformat_open_input(&d->fmt, path, NULL, NULL);
	if (r < 0)
		return r;
	if ((r = avformat_find_stream_info(d->fmt, NULL)) < 0)
		return r;
	const AVCodec* codec = NULL;
	r = av_find_best_stream(d->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (r < 0)
		return r;
	d->stream = r;
	codec = avcodec_find_decoder(d->fmt->streams[r]->codecpar->codec_id);
	if (!codec)
		return AVERROR_DECODER_NOT_FOUND;
	d->codec = avcodec_alloc_context3(codec);
	if (!d->codec)
		return AVERROR(ENOMEM);
	if ((r = avcodec_parameters_to_context(d->codec, d->fmt->streams[r]->codecpar)) < 0)
		return r;
	d->codec->thread_count = 2;
	d->codec->thread_type = FF_THREAD_SLICE;
	if ((r = avcodec_open2(d->codec, codec, NULL)) < 0)
		return r;
	d->frame = av_frame_alloc();
	d->scratch = av_frame_alloc();
	d->packet = av_packet_alloc();
	if (!d->frame || !d->scratch || !d->packet)
		return AVERROR(ENOMEM);
	AVStream* s = d->fmt->streams[d->stream];
	d->origin = s->start_time == AV_NOPTS_VALUE ? 0 : s->start_time;
	snprintf(d->path, sizeof(d->path), "%s", path);
	return 0;
}
static int next_frame(StDecoder* d)
{
	for (;;)
	{
		int r = avcodec_receive_frame(d->codec, d->scratch);
		if (r >= 0)
		{
			av_frame_unref(d->frame);
			av_frame_move_ref(d->frame, d->scratch);
			return 0;
		}
		if (r != AVERROR(EAGAIN))
			return r;
		if (d->eof)
			return AVERROR_EOF;
		while ((r = av_read_frame(d->fmt, d->packet)) >= 0)
		{
			if (d->packet->stream_index != d->stream)
			{
				av_packet_unref(d->packet);
				continue;
			}
			r = avcodec_send_packet(d->codec, d->packet);
			av_packet_unref(d->packet);
			if (r < 0)
				return r;
			break;
		}
		if (r == AVERROR_EOF)
		{
			d->eof = 1;
			r = avcodec_send_packet(d->codec, NULL);
			if (r < 0)
				return r;
		}
		else if (r < 0)
			return r;
	}
}
int st_decoder_read(StDecodeControl* e, StDecoder* d, int frame, uint8_t* pixels, int width, int height, enum AVPixelFormat output)
{
	if (!e || !e->stop || !d || !d->fmt || !d->codec)
		return AVERROR(EINVAL);
	if (width < 1 || height < 1 || width > 7680 || height > 4320 || frame < 0 || frame > ST_MAX_FRAME || !pixels || (output != AV_PIX_FMT_RGB24 && output != AV_PIX_FMT_RGB565LE))
		return AVERROR(EINVAL);
	AVStream* s = d->fmt->streams[d->stream];
	int64_t target = (int64_t)frame * AV_TIME_BASE / ST_FPS;
	if (d->last_us < 0 || target < d->last_us || target - d->last_us > 500000)
	{
		int64_t ts = av_rescale_q(target, AV_TIME_BASE_Q, s->time_base) + d->origin;
		int r = av_seek_frame(d->fmt, d->stream, ts, AVSEEK_FLAG_BACKWARD);
		if (r < 0)
			return r;
		avcodec_flush_buffers(d->codec);
		d->eof = 0;
		d->last_us = -1;
	}
	int r = 0;
	if (d->last_us < target || d->last_us < 0)
	{
		do
		{
			if (interrupt_io(e))
				return AVERROR_EXIT;
			r = next_frame(d);
			if (r < 0)
				break;
			int64_t pts = d->frame->best_effort_timestamp;
			d->last_us = pts == AV_NOPTS_VALUE ? (d->last_us < 0 ? 0 : d->last_us + AV_TIME_BASE / ST_FPS) : av_rescale_q(pts - d->origin, s->time_base, AV_TIME_BASE_Q);
		} while (d->last_us < target);
	}
	if (r < 0 && !(r == AVERROR_EOF && d->last_us >= 0))
		return r;
	int sw = d->frame->width, sh = d->frame->height;
	if (sw <= 0 || sh <= 0)
		return AVERROR_INVALIDDATA;
	AVRational sar = av_guess_sample_aspect_ratio(d->fmt, s, d->frame);
	double aspect = (double)sw / sh;
	if (sar.num > 0 && sar.den > 0)
		aspect *= av_q2d(sar);
	int w = width, h = (int)(w / aspect + 0.5);
	if (h > height)
	{
		h = height;
		w = (int)(h * aspect + 0.5);
	}
	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;
	enum AVPixelFormat format = d->frame->format;
	int full_range = d->frame->color_range == AVCOL_RANGE_JPEG;
	if (format == AV_PIX_FMT_YUVJ420P)
	{
		format = AV_PIX_FMT_YUV420P;
		full_range = 1;
	}
	if (format == AV_PIX_FMT_YUVJ422P)
	{
		format = AV_PIX_FMT_YUV422P;
		full_range = 1;
	}
	if (format == AV_PIX_FMT_YUVJ444P)
	{
		format = AV_PIX_FMT_YUV444P;
		full_range = 1;
	}
	d->scale = sws_getCachedContext(d->scale, sw, sh, format, w, h, output, SWS_BILINEAR, NULL, NULL, NULL);
	if (!d->scale)
		return AVERROR(ENOMEM);
	const int* coeff = sws_getCoefficients(d->frame->colorspace == AVCOL_SPC_BT709 ? SWS_CS_ITU709 : SWS_CS_DEFAULT);
	sws_setColorspaceDetails(d->scale, coeff, full_range, coeff, 1, 0, 1 << 16, 1 << 16);
	int bytes = output == AV_PIX_FMT_RGB24 ? 3 : 2;
	memset(pixels, 0, (size_t)width * height * bytes);
	uint8_t* dst[4] = {(uint8_t*)(pixels + ((height - h) / 2 * width + (width - w) / 2) * bytes), NULL, NULL, NULL};
	int lines[4] = {width * bytes, 0, 0, 0};
	r = sws_scale(d->scale, (const uint8_t* const*)d->frame->data, d->frame->linesize, 0, sh, dst, lines);
	return r > 0 ? 0 : AVERROR_INVALIDDATA;
}
