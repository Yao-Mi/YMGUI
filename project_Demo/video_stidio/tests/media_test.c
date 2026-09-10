#include "media/engine.h"
#include <assert.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern char** environ;
static long now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
static void generate(const char* path, char* source)
{
	char* args[] = {STUDIO_PROXY_FFMPEG, "-nostdin", "-v", "error", "-f", "lavfi", "-i", source, "-t", "2", "-c:v", "mpeg4", "-g", "1", "-threads", "1", "-y", (char*)path, NULL};
	pid_t child;
	assert(!posix_spawnp(&child, STUDIO_PROXY_FFMPEG, NULL, NULL, args, environ));
	int status;
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
static StImportResult imported(StEngine* e)
{
	StImportResult result;
	long end = now() + 8000;
	while (now() < end)
	{
		if (st_engine_poll_import(e, &result))
			return result;
		usleep(1000);
	}
	assert(0);
	return result;
}
static void frame(StEngine* e, uint64_t serial, uint16_t* pixels)
{
	StFrameResult r;
	long end = now() + 6000;
	while (now() < end)
	{
		if (st_engine_poll_frame(e, pixels, &r) && r.serial >= serial)
		{
			assert(r.ok);
			return;
		}
		usleep(1000);
	}
	assert(0);
}
int main(void)
{
	char dir[] = "/tmp/ymstudio-media-XXXXXX";
	assert(mkdtemp(dir));
	char red[256], blue[256];
	snprintf(red, sizeof(red), "%s/red ' 素材.avi", dir);
	snprintf(blue, sizeof(blue), "%s/blue.avi", dir);
	generate(red, "color=red:size=160x90:rate=25");
	generate(blue, "color=blue:size=128x128:rate=25");
	StEngine* e = st_engine_create();
	assert(e);
	assert(st_engine_import(e, red));
	assert(st_engine_import(e, blue));
	StImportResult a = imported(e), b = imported(e);
	assert(a.ok && b.ok && a.media.frames == 50 && b.media.frames == 50);
	assert(a.media.width == 160 && b.media.width == 128);
	uint16_t* pixels = malloc(ST_PIXELS * sizeof(*pixels));
	assert(pixels);
	frame(e, st_engine_request(e, red, 49), pixels);
	uint16_t c = pixels[180 * 640 + 320];
	assert((c >> 11) > 25 && (c & 31) < 5);
	st_engine_request(e, red, 0);
	st_engine_request(e, blue, 20);
	uint64_t newest = st_engine_request(e, blue, 49);
	frame(e, newest, pixels);
	c = pixels[180 * 640 + 320];
	assert((c & 31) > 25 && (c >> 11) < 5);
	assert(pixels[180 * 640] == 0); /* square source is letterboxed */
	assert(st_engine_import(e, "/tmp/ymstudio-missing-no-video"));
	StImportResult missing = imported(e);
	assert(!missing.ok && missing.error[0]);
	frame(e, st_engine_request(e, "", 0), pixels);
	for (int i = 0; i < ST_PIXELS; i++)
		assert(!pixels[i]);
	st_engine_destroy(e);
	free(pixels);
	unlink(red);
	unlink(blue);
	rmdir(dir);
	puts("media: multiple sources, final-frame decode, aspect ratio, latest request, failures and blank passed");
	return 0;
}
