/* Visible clip in/out thumbnails loaded on demand. */
#include "app/studio.h"
#include "YMGUI_Invalidate.h"
#include <SDL2/SDL.h>
#include <string.h>
static void schedule(Studio* s, int media, int frame)
{
	const char* path = s->project.media[media].path;
	int oldest = -1;
	for (int i = 0; i < 32; i++)
	{
		if (s->clip_thumbs[i].state && s->clip_thumbs[i].frame == frame && !strcmp(s->clip_thumbs[i].path, path))
		{
			s->clip_thumbs[i].used = ++s->thumb_clock;
			return;
		}
		if (s->clip_thumbs[i].state != 1 && (oldest < 0 || s->clip_thumbs[i].used < s->clip_thumbs[oldest].used))
			oldest = i;
	}
	if (oldest < 0 || !st_engine_thumbnail(s->engine, path, frame))
		return;
	strcpy(s->clip_thumbs[oldest].path, path);
	s->clip_thumbs[oldest].frame = frame;
	s->clip_thumbs[oldest].state = 1;
	s->clip_thumbs[oldest].used = ++s->thumb_clock;
}
GYIMG studio_clip_thumbnail(Studio* s, int media, int frame)
{
	const char* path = s->project.media[media].path;
	for (int i = 0; i < 32; i++)
		if (s->clip_thumbs[i].state == 2 && s->clip_thumbs[i].frame == frame && !strcmp(s->clip_thumbs[i].path, path))
			return &s->clip_thumbs[i].image;
	return NULL;
}
void studio_thumbnail_tick(Studio* s)
{
	StThumbnailResult result;
	while (st_engine_poll_thumbnail(s->engine, &result))
	{
		for (int i = 0; i < 32; i++)
			if (s->clip_thumbs[i].frame == result.frame && !strcmp(s->clip_thumbs[i].path, result.path))
			{
				s->clip_thumbs[i].state = result.ok ? 2 : -1;
				if (result.ok)
				{
					memcpy(s->clip_thumbs[i].pixels, result.pixels, sizeof(result.pixels));
					s->clip_thumbs[i].image = (GYimg){.data = s->clip_thumbs[i].pixels, .w = 80, .h = 45};
				}
				YMGUI_Obj_Invalidate(s->timeline);
				break;
			}
	}
	if (SDL_GetTicks() - s->thumb_tick < 200 || YMGUI_State_GetBool(&s->playing) || s->scrubbing)
		return;
	s->thumb_tick = SDL_GetTicks();
	int left = YMGUI_State_GetInt(&s->scroll);
	double scale = YMGUI_State_GetInt(&s->zoom) / (double)ST_FPS;
	int right = left + (int)((ST_W - ST_HEADER) / scale);
	for (int row = 0; row < ST_VISIBLE_TRACKS; row++)
	{
		int t = s->project.track_count - 1 - s->track_scroll - row;
		if (t < 0)
			break;
		for (int i = 0; i < s->project.tracks[t].count; i++)
		{
			StClip* c = &s->project.tracks[t].clips[i];
			if (s->proxy_states[c->media] != 2 || c->start + c->length <= left || c->start >= right)
				continue;
			if (c->start >= left)
				schedule(s, c->media, c->in);
			if (c->start + c->length <= right && c->length * scale > 150)
				schedule(s, c->media, c->in + c->length - 1);
		}
	}
}
