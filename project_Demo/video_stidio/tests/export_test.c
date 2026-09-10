#include "app/studio.h"
#include "model/render_plan.h"
#include "media/decoder.h"
#include "YMGUI_Bar.h"
#include <libavutil/time.h>
#include <SDL2/SDL.h>
#include <assert.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern char** environ;
static void generate(const char* path, char* filter)
{
	char* args[] = {STUDIO_PROXY_FFMPEG, "-nostdin", "-v", "error", "-f", "lavfi", "-i", filter, "-c:v", "mpeg4", "-g", "1", "-threads", "1", "-y", (char*)path, NULL};
	pid_t child;
	assert(!posix_spawnp(&child, STUDIO_PROXY_FFMPEG, NULL, NULL, args, environ));
	int status;
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
static StExportStatus wait_job(Studio* s)
{
	uint32_t start = SDL_GetTicks();
	StExportStatus status;
	do
	{
		studio_tick(s);
		st_export_status(s->exporter, &status);
		SDL_Delay(2);
	} while (status.state == ST_EXPORT_RUNNING && SDL_GetTicks() - start < 25000);
	assert(status.state != ST_EXPORT_RUNNING);
	return status;
}
static void no_temporary_files(const char* path)
{
	DIR* d = opendir(path);
	assert(d);
	struct dirent* entry;
	while ((entry = readdir(d)))
		assert(!strstr(entry->d_name, ".partial-") && !strstr(entry->d_name, ".log-"));
	closedir(d);
}
static void write_guard(const char* path)
{
	FILE* f = fopen(path, "wb");
	assert(f);
	assert(fwrite("KEEP", 1, 4, f) == 4);
	fclose(f);
}
static void check_guard(const char* path)
{
	char b[8] = {0};
	FILE* f = fopen(path, "rb");
	assert(f);
	assert(fread(b, 1, 8, f) == 4);
	assert(!strcmp(b, "KEEP"));
	fclose(f);
}
static void check_pixel(StDecoder* d, StDecodeControl* control, uint8_t* rgb, int frame, int kind)
{
	control->deadline = av_gettime_relative() + 5000000;
	assert(!st_decoder_read(control, d, frame, rgb, 1280, 720, AV_PIX_FMT_RGB24));
	uint8_t* p = rgb + (360 * 1280 + 640) * 3;
	if (kind == 0)
		assert(p[0] < 8 && p[1] < 8 && p[2] < 8);
	else if (kind == 1)
		assert(p[0] > 180 && p[1] > 180 && p[2] < 50);
	else
	{
		assert(p[2] > 180 && p[0] < 50 && p[1] < 50);
		p = rgb + 360 * 1280 * 3;
		assert(p[0] < 8 && p[1] < 8 && p[2] < 8);
	}
}
int main(void)
{
	char dir[] = "/tmp/ymstudio-export-XXXXXX";
	assert(mkdtemp(dir));
	char first[1024], second[1024], output[1024], guard[1024], alias[1024], race[1024];
	snprintf(first, sizeof(first), "%s/原素材 ' red.avi", dir);
	snprintf(second, sizeof(second), "%s/blue.avi", dir);
	snprintf(output, sizeof(output), "%s/成片 ' example.mp4", dir);
	snprintf(guard, sizeof(guard), "%s/keep.mp4", dir);
	snprintf(alias, sizeof(alias), "%s/source-alias.mp4", dir);
	snprintf(race, sizeof(race), "%s/race.mp4", dir);
	generate(first, "color=red:s=160x90:r=25:d=1[a];color=yellow:s=160x90:r=25:d=1[b];[a][b]concat=n=2:v=1:a=0");
	generate(second, "color=blue:s=128x128:r=25:d=2");
	Studio* s = studio_create();
	assert(s);
	StProject* p = &s->project;
	p->media_count = 2;
	p->track_count = 3;
	p->media[0] = (StMedia){.frames = 50, .width = 160, .height = 90};
	p->media[1] = (StMedia){.frames = 50, .width = 128, .height = 128};
	strcpy(p->media[0].path, first);
	strcpy(p->media[1].path, second);
	assert(st_project_edit(p, (StEdit){.kind = ST_ADD, .track = 0, .media = 0, .at = 0, .in = 25, .length = 25}));
	assert(st_project_edit(p, (StEdit){.kind = ST_ADD, .track = 0, .media = 0, .at = 50, .in = 25, .length = 25}));
	assert(st_project_edit(p, (StEdit){.kind = ST_ADD, .track = 1, .media = 1, .at = 10, .in = 5, .length = 10}));
	assert(st_project_edit(p, (StEdit){.kind = ST_ADD, .track = 2, .media = 1, .at = 90, .length = 10}));
	assert(st_project_edit(p, (StEdit){.kind = ST_TRACK_HIDE, .track = 2}));
	st_history_reset(&s->history, p);
	studio_changed(s);
	StRenderPlan plan;
	assert(st_render_plan(p, &plan) && plan.frames == 100);
	for (int i = 0; i < plan.count; i++)
		for (int f = plan.spans[i].start; f < plan.spans[i].start + plan.spans[i].length; f++)
		{
			const StClip* c = st_visible(p, f);
			assert(plan.spans[i].media == (c ? c->media : -1));
			if (c)
				assert(plan.spans[i].in + f - plan.spans[i].start == c->in + f - c->start);
		}
	assert(studio_export_start(s, output, 0));
	assert(YMGUI_State_GetBool(&s->export_running));
	/* Editing after dispatch changes the monitor, never the render snapshot. */
	assert(studio_edit(s, (StEdit){.kind = ST_TRACK_HIDE, .track = 0}));
	assert(!st_visible(p, 0));
	StExportStatus status = wait_job(s);
	if (status.state != ST_EXPORT_DONE)
		fprintf(stderr, "export error: %s\n", status.message);
	assert(status.state == ST_EXPORT_DONE && status.frames == 100);
	assert(!YMGUI_State_GetBool(&s->export_running));
	assert(YMGUI_Bar_GetValue(s->export_bar) == 1000);
	studio_action(s, ACT_UNDO);
	atomic_int stop = 0;
	StDecodeControl control = {&stop, av_gettime_relative() + 5000000};
	StDecoder decoder = {0};
	assert(!st_decoder_open(&control, &decoder, output));
	assert(decoder.codec->codec_id == AV_CODEC_ID_H264 && decoder.codec->width == 1280 && decoder.codec->height == 720);
	assert(decoder.fmt->streams[decoder.stream]->nb_frames == 100);
	assert(decoder.fmt->nb_streams == 1);
	uint8_t* rgb = malloc(1280 * 720 * 3);
	assert(rgb);
	int frames[] = {0, 9, 10, 19, 20, 24, 25, 49, 50, 74, 75, 99};
	int kinds[] = {1, 1, 2, 2, 1, 1, 0, 0, 1, 1, 0, 0};
	for (int i = 0; i < 12; i++)
		check_pixel(&decoder, &control, rgb, frames[i], kinds[i]);
	st_decoder_close(&decoder);
	free(rgb);
	no_temporary_files(dir);
	/* Reject overwriting original media through a hard-link alias. */
	assert(!link(first, alias));
	assert(!studio_export_start(s, alias, 1));
	write_guard(guard);
	assert(!studio_export_start(s, guard, 0));
	check_guard(guard);
	/* Cancellation and decoder failure preserve a previously existing destination. */
	assert(studio_export_start(s, guard, 1));
	studio_action(s, ACT_EXPORT_CANCEL);
	status = wait_job(s);
	assert(status.state == ST_EXPORT_CANCELLED);
	check_guard(guard);
	no_temporary_files(dir);
	strcpy(p->media[0].path, "/tmp/ymstudio-export-source-missing");
	assert(studio_export_start(s, guard, 1));
	status = wait_job(s);
	assert(status.state == ST_EXPORT_FAILED && status.message[0]);
	check_guard(guard);
	strcpy(p->media[0].path, first);
	no_temporary_files(dir);
	/* Do not clobber a file another program creates while export is in progress. */
	assert(studio_export_start(s, race, 0));
	write_guard(race);
	status = wait_job(s);
	assert(status.state == ST_EXPORT_FAILED);
	check_guard(race);
	no_temporary_files(dir);
	studio_destroy(s);
	unlink(first);
	unlink(second);
	unlink(output);
	unlink(guard);
	unlink(alias);
	unlink(race);
	assert(!rmdir(dir));
	puts("export: H.264 profile, exact frame count, layer/source boundaries, gaps, snapshot, bindings, cancel, failure and output protection passed");
	return 0;
}
