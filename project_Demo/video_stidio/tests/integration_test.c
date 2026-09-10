#include "app/studio.h"
#include "YMGUI_Invalidate.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void tick(Studio* s)
{
	studio_tick(s);
	SDL_Delay(2);
}
static void wait_frame(Studio* s)
{
	uint32_t start = SDL_GetTicks();
	while (s->shown_serial < s->wanted_serial && SDL_GetTicks() - start < 6000)
		tick(s);
	assert(s->shown_serial >= s->wanted_serial);
	assert(s->preview_ok);
}
static unsigned hash(Studio* s)
{
	unsigned h = 2166136261u;
	for (int i = 0; i < ST_PIXELS; i++)
		h = (h ^ s->pixels[i]) * 16777619u;
	return h;
}
static void pointer(int x, int y, int down)
{
	YMGUI_Inject_Pointer(x, y, down);
}
static void move_clip(Studio* s, int id, int destination_track, int destination_start)
{
	int track;
	StClip before = *st_clip(&s->project, id, &track);
	int x = ST_HEADER + (before.start + 25) * 70 / ST_FPS;
	int y = ST_TL_Y + ST_RULER + (s->project.track_count - 1 - track) * ST_ROW + 33;
	int to_x = ST_HEADER + (destination_start + 25) * 70 / ST_FPS;
	int to_y = ST_TL_Y + ST_RULER + (s->project.track_count - 1 - destination_track) * ST_ROW + 33;
	pointer(x, y, 1);
	assert(s->drag_kind == 2);
	pointer(to_x, to_y, 1);
	assert(s->ghost_valid && s->ghost_at == destination_start);
	pointer(to_x, to_y, 0);
	const StClip* after = st_clip(&s->project, id, &track);
	assert(after && track == destination_track && after->start == destination_start);
	assert(after->in == before.in && after->length == before.length && after->media == before.media);
}
int main(int argc, char** argv)
{
	assert(argc > 1);
	Studio* s = studio_create();
	assert(s);
	studio_import(s, argv[1]);
	uint32_t begin = SDL_GetTicks();
	while (s->pending_imports && SDL_GetTicks() - begin < 10000)
		tick(s);
	assert(s->project.media_count == 1 && s->thumb_count == 1);
	/* Reproduce moving a freshly dropped clip left: source in is still zero. */
	GYrect bin;
	YMGUI_Obj_GetAbsArea(s->bin, &bin);
	int drop_x = ST_HEADER + 100 * 70 / ST_FPS;
	int drop_y = ST_TL_Y + ST_RULER + ST_ROW + 33;
	pointer(bin.x + 20, bin.y + 30, 1);
	pointer(drop_x, drop_y, 1);
	pointer(drop_x, drop_y, 0);
	assert(s->project.tracks[0].count == 1);
	int dropped = s->project.tracks[0].clips[0].id;
	assert(st_clip(&s->project, dropped, NULL)->start == 100);
	assert(st_clip(&s->project, dropped, NULL)->in == 0);
	move_clip(s, dropped, 0, 50);
	studio_action(s, ACT_UNDO);
	assert(st_clip(&s->project, dropped, NULL)->start == 100);
	studio_action(s, ACT_REDO);
	assert(st_clip(&s->project, dropped, NULL)->start == 50);
	move_clip(s, dropped, 0, 100);
	move_clip(s, dropped, 1, 0);
	move_clip(s, dropped, 0, 0);
	studio_seek(s, 25, 1);
	wait_frame(s);
	unsigned a = hash(s);
	studio_seek(s, 750, 1);
	wait_frame(s);
	unsigned b = hash(s);
	assert(a != b);
	studio_seek(s, 25, 1);
	wait_frame(s);
	assert(a == hash(s));
	studio_seek(s, 100, 1);
	studio_action(s, ACT_SPLIT);
	assert(s->project.tracks[0].count == 2);
	studio_action(s, ACT_UNDO);
	assert(s->project.tracks[0].count == 1);
	studio_action(s, ACT_REDO);
	assert(s->project.tracks[0].count == 2);
	/* Cross-track actual pointer drag, body hit on the second clip. */
	s->selected_clip = 2;
	pointer(ST_HEADER + 400, ST_TL_Y + ST_RULER + ST_ROW + 33, 1);
	pointer(ST_HEADER + 470, ST_TL_Y + ST_RULER + 33, 1);
	pointer(ST_HEADER + 470, ST_TL_Y + ST_RULER + 33, 0);
	assert(s->project.tracks[1].count == 1 && s->project.tracks[0].count == 1);
	/* Moving a split clip left must also preserve its nonzero source in. */
	StClip split = s->project.tracks[1].clips[0];
	move_clip(s, split.id, 1, 0);
	move_clip(s, split.id, 1, split.start);
	/* Click-to-select must not create an edit through magnet snapping. */
	int cursor = s->history.cursor;
	StClip moved = s->project.tracks[1].clips[0];
	pointer(ST_HEADER + 500, ST_TL_Y + ST_RULER + 33, 1);
	pointer(ST_HEADER + 500, ST_TL_Y + ST_RULER + 33, 0);
	assert(s->history.cursor == cursor);
	assert(st_clip(&s->project, moved.id, NULL)->start == moved.start);
	/* Real left-edge trim preserves the right endpoint and advances source in. */
	s->alt = 1;
	int edge = ST_HEADER + moved.start * 70 / ST_FPS;
	pointer(edge + 3, ST_TL_Y + ST_RULER + 33, 1);
	pointer(edge + 31, ST_TL_Y + ST_RULER + 33, 1);
	pointer(edge + 31, ST_TL_Y + ST_RULER + 33, 0);
	s->alt = 0;
	const StClip* trimmed = st_clip(&s->project, moved.id, NULL);
	assert(trimmed->in == moved.in + 10);
	assert(trimmed->start + trimmed->length == moved.start + moved.length);
	studio_action(s, ACT_UNDO);
	/* Pointer cancellation (window focus loss / Esc) discards tentative geometry. */
	cursor = s->history.cursor;
	pointer(ST_HEADER + 500, ST_TL_Y + ST_RULER + 33, 1);
	pointer(ST_HEADER + 540, ST_TL_Y + ST_RULER + 33, 1);
	YMGUI_Inject_PointerCancel();
	assert(s->history.cursor == cursor);
	assert(st_clip(&s->project, moved.id, NULL)->start == moved.start);
	/* Binding: one state write immediately moves the independent playhead object. */
	YMGUI_State_SetInt(&s->position, 50);
	assert(s->head->area.x == ST_HEADER + 140 - 5);
	/* Top track wins, hidden top exposes the lower track/gap. */
	int top = s->project.tracks[1].clips[0].id;
	assert(st_visible(&s->project, 200)->id == top);
	assert(studio_edit(s, (StEdit){.kind = ST_TRACK_HIDE, .track = 1}));
	assert(!st_visible(&s->project, 200));
	studio_action(s, ACT_UNDO);
	/* Proxies build independently. Verify actual readiness before measuring scrub latency. */
	begin = SDL_GetTicks();
	while (st_engine_proxy_state(s->engine, s->project.media[0].path) == 1 && SDL_GetTicks() - begin < 80000)
		tick(s);
	assert(st_engine_proxy_state(s->engine, s->project.media[0].path) == 2);
	printf("proxy preparation wait: %u ms\n", SDL_GetTicks() - begin);
	/* Scrubbing uses native decoder contexts and never starts subprocesses. */
	studio_action(s, ACT_FIT);
	double total = 0, max = 0;
	int targets[] = {25, 1000, 50, 2250, 750, 75, 1500, 200};
	for (unsigned i = 0; i < sizeof(targets) / sizeof(targets[0]); i++)
	{
		uint32_t started = SDL_GetTicks();
		studio_seek(s, targets[i], 1);
		wait_frame(s);
		double ms = SDL_GetTicks() - started;
		total += ms;
		if (ms > max)
			max = ms;
	}
	uint64_t old = s->shown_serial;
	int updates = 0;
	for (int i = 0; i < 30; i++)
	{
		studio_seek(s, 350 + i * 7, 0);
		uint32_t until = SDL_GetTicks() + 32;
		while (SDL_GetTicks() < until)
			tick(s);
		if (s->shown_serial != old)
		{
			old = s->shown_serial;
			updates++;
		}
	}
	studio_seek(s, 562, 1);
	wait_frame(s);
	assert(studio_position(s) == 562);
	assert(updates >= 10);
	/* Native slider binding must establish a final-result barrier on release too. */
	GYrect slider;
	YMGUI_Obj_GetAbsArea(s->seek, &slider);
	pointer(slider.x + 210, slider.y + slider.h / 2, 1);
	pointer(slider.x + 310, slider.y + slider.h / 2, 1);
	pointer(slider.x + 310, slider.y + slider.h / 2, 0);
	assert(s->min_serial == s->wanted_serial);
	wait_frame(s);
	studio_seek(s, 562, 1);
	wait_frame(s);
	studio_action(s, ACT_PLAY);
	begin = SDL_GetTicks();
	while (SDL_GetTicks() - begin < 300)
		tick(s);
	assert(YMGUI_State_GetBool(&s->playing));
	assert(studio_position(s) > 562);
	studio_action(s, ACT_PLAY);
	studio_seek(s, studio_duration(s) - 1, 1);
	wait_frame(s);
	assert(s->preview_ok);
	studio_seek(s, 55, 1);
	wait_frame(s);
	studio_ui_refresh(s);
	begin = SDL_GetTicks();
	while (!studio_clip_thumbnail(s, 0, s->project.media[0].frames - 1) && SDL_GetTicks() - begin < 6000)
		tick(s);
	assert(studio_clip_thumbnail(s, 0, 0));
	assert(studio_clip_thumbnail(s, 0, s->project.media[0].frames - 1));
	studio_tick(s);
	printf("native preview: %.1f ms mean, %.1f ms max, %d visible updates / 30 drag steps\n", total / 8, max, updates);
	studio_destroy(s);
	puts("integration passed");
	return 0;
}
