#include "model/project.h"
#include "model/history.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void)
{
	StProject *p = malloc(sizeof(*p)), *loaded = malloc(sizeof(*p)), *before = malloc(sizeof(*p));
	assert(p && loaded && before);
	st_project_init(p);
	p->media_count = 1;
	p->media[0] = (StMedia){.path = "/tmp/含 空格'和\n换行.mp4", .frames = 250, .width = 1920, .height = 1080};
	assert(st_project_valid(p));
	StHistory h = {0};
	assert(st_history_init(&h, p));
	assert(st_project_edit(p, (StEdit){.kind = ST_ADD, .track = 0, .media = 0, .at = 25, .length = 100}));
	st_history_push(&h, p);
	assert(st_duration(p) == 125);
	assert(!st_visible(p, 24));
	assert(st_visible(p, 25));
	assert(!st_visible(p, 125));
	assert(st_project_edit(p, (StEdit){.kind = ST_SPLIT, .track = 0, .id = 1, .at = 60}));
	st_history_push(&h, p);
	assert(p->tracks[0].clips[0].length == 35 && p->tracks[0].clips[1].in == 35);
	assert(st_history_step(&h, p, -1));
	assert(p->tracks[0].count == 1);
	assert(st_history_step(&h, p, 1));
	assert(p->tracks[0].count == 2);
	*before = *p;
	assert(!st_project_edit(p, (StEdit){.kind = ST_MOVE, .track = 0, .id = 2, .at = 45}));
	assert(!memcmp(before, p, sizeof(*p)));
	assert(st_project_edit(p, (StEdit){.kind = ST_MOVE, .track = 1, .id = 2, .at = 40}));
	assert(st_visible(p, 50)->id == 2);
	assert(st_project_edit(p, (StEdit){.kind = ST_TRIM, .track = 1, .id = 2, .in = 45, .length = 30}));
	assert(st_clip(p, 2, NULL)->start == 50);
	assert(st_project_edit(p, (StEdit){.kind = ST_TRACK_HIDE, .track = 1}));
	assert(st_visible(p, 50)->id == 1);
	assert(st_project_edit(p, (StEdit){.kind = ST_TRACK_LOCK, .track = 1}));
	*before = *p;
	assert(!st_project_edit(p, (StEdit){.kind = ST_LIFT, .track = 1, .id = 2}));
	assert(!memcmp(before, p, sizeof(*p)));
	char path[] = "/tmp/ymstudio-model-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	close(fd);
	assert(st_project_save(p, path));
	assert(st_project_load(loaded, path));
	assert(!memcmp(loaded, p, sizeof(*p)));
	FILE* f = fopen(path, "a");
	assert(f);
	fputs("garbage", f);
	fclose(f);
	*before = *loaded;
	assert(!st_project_load(loaded, path));
	assert(!memcmp(before, loaded, sizeof(*p)));
	unlink(path);
	for (int i = 0; i < 50; i++)
	{
		assert(st_project_edit(p, (StEdit){.kind = ST_TRACK_HIDE, .track = 0}));
		st_history_push(&h, p);
	}
	assert(h.cursor == ST_UNDO);
	int n = 0;
	while (st_history_step(&h, p, -1))
		n++;
	assert(n == ST_UNDO);
	st_history_free(&h);
	free(before);
	free(loaded);
	free(p);
	puts("model: frame boundaries, atomic edits, layers, history, project roundtrip passed");
	return 0;
}
