#ifdef NDEBUG
#undef NDEBUG
#endif
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static size_t allocated, copied, allocations;
static void* counted_malloc(size_t n)
{
	allocated += n;
	++allocations;
	return malloc(n);
}
static void* counted_calloc(size_t n, size_t s)
{
	allocated += n * s;
	++allocations;
	return calloc(n, s);
}
static void* counted_memcpy(void* d, const void* s, size_t n)
{
	copied += n;
	return memcpy(d, s, n);
}
static FILE* fixture_popen(const char* command, const char* mode)
{
	(void)command;
	(void)mode;
	return tmpfile();
}
static int fixture_pclose(FILE* file)
{
	return fclose(file);
}
#define popen fixture_popen
#define pclose fixture_pclose
#define malloc counted_malloc
#define calloc counted_calloc
#define memcpy counted_memcpy
#ifndef PIPELINE_SOURCE
#define PIPELINE_SOURCE "../vp_video.c"
#endif
#include PIPELINE_SOURCE
#undef malloc
#undef calloc
#undef memcpy
int main(void)
{
	VPvideo* v = vp_video_create();
	v->out_w = 320;
	v->out_h = 240;
	v->fps_num = 25;
	v->fps_den = 1;
	allocated = copied = allocations = 0;
	assert(allocBuffers(v));
	printf("frame pool: %zu bytes, %zu allocations\n", allocated, allocations);
	vp_video_destroy(v);
	v = vp_video_create();
	v->out_w = 4;
	v->out_h = 2;
	v->fps_num = 25;
	v->fps_den = 1;
	assert(allocBuffers(v));
	int fds[2];
	assert(pipe(fds) == 0);
	v->fd = fds[0];
	v->pipe = stdout;
	assert(fcntl(v->fd, F_SETFL, O_NONBLOCK) >= 0);
	GYpx expected[8];
	for (unsigned i = 0; i < 8; ++i)
		expected[i] = GY_ColorToPx(0xff113355u + i * 6739);
	const GYpx* stable = vp_video_pixels(v);
	assert(write(fds[1], expected, 3) == 3);
	vp_video_read_tick(v);
	assert(v->fill_bytes == 3 && v->ring_count == 0 && !vp_video_update(v, 0));
	assert(write(fds[1], (char*)expected + 3, sizeof(expected) - 3) == sizeof(expected) - 3);
	copied = 0;
	vp_video_read_tick(v);
	assert(v->ring_count == 1);
	assert(vp_video_update(v, 0));
	assert(!memcmp(stable, expected, sizeof(expected)));
	printf("per frame engine memcpy: %zu bytes for %zu-byte frame\n", copied, sizeof(expected));
	for (int frame = 1; frame < 100; ++frame)
	{
		for (unsigned i = 0; i < 8; ++i)
			expected[i] = GY_ColorToPx(0xff223344u + frame * 371 + i * 1337);
		assert(write(fds[1], expected, sizeof(expected)) == sizeof(expected));
		vp_video_read_tick(v);
		assert(vp_video_update(v, frame * 40));
		assert(vp_video_pixels(v) == stable && !memcmp(stable, expected, sizeof(expected)));
		assert(v->ring_count == 0);
	}
	assert(!vp_video_update(v, 3960));
	for (unsigned i = 0; i < 8; ++i)
		expected[i] = GY_ColorToPx(0xff556677u + i * 6739);
	assert(write(fds[1], expected, sizeof(expected)) == sizeof(expected));
	assert(write(fds[1], expected, 3) == 3);
	vp_video_read_tick(v);
	assert(v->ring_count == 1 && v->fill_bytes == 3);
	assert(vp_video_update(v, 4000) && !memcmp(stable, expected, sizeof(expected)));
	assert(v->fill_bytes == 3 && v->ring_count == 0);
	assert(write(fds[1], (char*)expected + 3, sizeof(expected) - 3) == sizeof(expected) - 3);
	vp_video_read_tick(v);
	assert(!vp_video_update(v, 4000));
	assert(v->ring_count == 1);
	assert(vp_video_update(v, 4040));
	assert(!memcmp(stable, expected, sizeof(expected)));
	close(fds[1]);
	vp_video_read_tick(v);
	assert(vp_video_is_eof(v));
	close(fds[0]);
	v->pipe = NULL;
	allocated = allocations = 0;
	vp_video_seek_ms(v, 1000);
	printf("seek frame allocations: %zu (%zu bytes)\n", allocations, allocated);
#ifndef VIDEO_BUFFER_BASELINE
	assert(allocations == 0 && allocated == 0);
#endif
	assert(vp_video_pixels(v) == stable && v->ring_count == 0 && v->fill_bytes == 0);
	vp_video_destroy(v);
	puts("PASS: partial reads, 100 frames across ring wrap, stable displayed pixels, idle update and EOF");
}
