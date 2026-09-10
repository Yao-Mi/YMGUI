#include "app/studio.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_Font.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static double scale(Studio* s)
{
	return YMGUI_State_GetInt(&s->zoom) / (double)ST_FPS;
}
static int screen_x(Studio* s, int frame)
{
	return ST_HEADER + (int)((frame - (double)YMGUI_State_GetInt(&s->scroll)) * scale(s));
}
int studio_frame_at(Studio* s, int x)
{
	double f = (x - ST_HEADER) / scale(s) + YMGUI_State_GetInt(&s->scroll);
	if (f < 0)
		f = 0;
	if (f > ST_MAX_FRAME)
		f = ST_MAX_FRAME;
	return (int)llround(f);
}
int studio_track_at(Studio* s, int y)
{
	if (y < ST_TL_Y + ST_RULER || y >= ST_TL_Y + ST_TL_H)
		return -1;
	int row = (y - ST_TL_Y - ST_RULER) / ST_ROW + s->track_scroll;
	return row < s->project.track_count ? s->project.track_count - 1 - row : -1;
}
static int track_y(Studio* s, int track)
{
	return ST_TL_Y + ST_RULER + (s->project.track_count - 1 - track - s->track_scroll) * ST_ROW;
}
static int intersect(GYsurface* out, GYSURFACE in, GYrect r)
{
	*out = *in;
	int l = r.x > in->clip.x ? r.x : in->clip.x, t = r.y > in->clip.y ? r.y : in->clip.y;
	int right = r.x + r.w < in->clip.x + in->clip.w ? r.x + r.w : in->clip.x + in->clip.w;
	int b = r.y + r.h < in->clip.y + in->clip.h ? r.y + r.h : in->clip.y + in->clip.h;
	if (right <= l || b <= t)
		return 0;
	out->clip = (GYrect){l, t, right - l, b - t};
	return 1;
}
static void border(GYSURFACE s, GYrect r, unsigned color)
{
	GYcolor c = ST_RGB(color);
	YMGUI_Draw_Fill(s, &(GYrect){r.x, r.y, r.w, 2}, c, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &(GYrect){r.x, r.y + r.h - 2, r.w, 2}, c, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &(GYrect){r.x, r.y, 2, r.h}, c, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &(GYrect){r.x + r.w - 2, r.y, 2, r.h}, c, GY_OPA_COVER);
}
static void head_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	YMGUI_Draw_Fill(s, &(GYrect){a->x + 5, a->y, 1, a->h}, ST_RGB(0xf6f7fa), GY_OPA_COVER);
	for (int y = 0; y < 8; y++)
		YMGUI_Draw_Fill(s, &(GYrect){a->x + y / 2, a->y + y, 11 - y, 1}, ST_RGB(0xf3b858), GY_OPA_COVER);
}
void studio_head_apply(GYOBJ o, const GYval* v)
{
	Studio* s = studio_of(o);
	o->draw_cb = head_draw;
	int x = screen_x(s, v->u.i);
	YMGUI_Obj_Invalidate(o);
	YMGUI_Obj_SetHidden(o, x < ST_HEADER || x >= ST_W);
	if (x >= ST_HEADER && x < ST_W)
	{
		o->area.x = x - 5;
		YMGUI_Obj_Invalidate(o);
	}
}
void studio_timeline_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	Studio* s = studio_of(obj);
	YMGUI_Draw_Fill(surface, area, ST_RGB(0x202226), GY_OPA_COVER);
	YMGUI_Draw_Fill(surface, &(GYrect){0, ST_TL_Y, ST_W, ST_RULER}, ST_RGB(0x32353b), GY_OPA_COVER);
	YMGUI_Draw_Text(surface, &YMGUI_Font_Default, 12, ST_TL_Y + 7, "视频轨道", ST_RGB(0xc9d1da));
	int step = ST_FPS;
	while (step * scale(s) < 82)
		step *= 2;
	while (step > 1 && step * scale(s) > 220)
		step = (step + 1) / 2;
	int first = YMGUI_State_GetInt(&s->scroll) / step * step;
	for (int f = first; f <= ST_MAX_FRAME; f += step)
	{
		int x = screen_x(s, f);
		if (x > ST_W)
			break;
		if (x < ST_HEADER)
			continue;
		char text[32];
		studio_timecode(text, sizeof(text), f);
		YMGUI_Draw_Text(surface, &YMGUI_Font_Default, x + 4, ST_TL_Y + 4, text, ST_RGB(0xb6c0cb));
		YMGUI_Draw_Line(surface, x, ST_TL_Y + 21, x, ST_TL_Y + ST_TL_H, ST_RGB(0x373b41));
	}
	for (int t = 0; t < s->project.track_count; t++)
	{
		int y = track_y(s, t);
		if (y < ST_TL_Y + ST_RULER || y + ST_ROW > ST_TL_Y + ST_TL_H)
			continue;
		StTrack* tr = &s->project.tracks[t];
		YMGUI_Draw_Fill(surface, &(GYrect){0, y, ST_HEADER - 1, ST_ROW - 1}, ST_RGB(t == s->selected_track ? 0x46505a : 0x32363c), GY_OPA_COVER);
		char title[32];
		snprintf(title, sizeof(title), "V%d", t + 1);
		YMGUI_Draw_Text(surface, &YMGUI_Font_Default, 12, y + 9, title, ST_RGB(0xe5e9ef));
		snprintf(title, sizeof(title), "%d 片段", tr->count);
		YMGUI_Draw_Text(surface, &YMGUI_Font_Default, 58, y + 9, title, ST_RGB(0xaeb9c4));
		YMGUI_Draw_Text(surface, &YMGUI_Font_Default, 12, y + 39, tr->visible ? "显示" : "隐藏", ST_RGB(tr->visible ? 0x83c3dc : 0xb57979));
		YMGUI_Draw_Text(surface, &YMGUI_Font_Default, 84, y + 39, tr->locked ? "已锁定" : "未锁定", ST_RGB(tr->locked ? 0xe8b75f : 0x909da8));
		YMGUI_Draw_Line(surface, ST_HEADER, y + ST_ROW - 1, ST_W, y + ST_ROW - 1, ST_RGB(0x101216));
		for (int i = 0; i < tr->count; i++)
		{
			StClip* c = &tr->clips[i];
			int left = screen_x(s, c->start), right = screen_x(s, c->start + c->length);
			if (right <= ST_HEADER || left >= ST_W)
				continue;
			int x = left < ST_HEADER ? ST_HEADER : left;
			int end = right > ST_W ? ST_W : right;
			GYrect rect = {x, y + 2, end - x, ST_ROW - 5};
			GYsurface clipped;
			if (!intersect(&clipped, surface, rect))
				continue;
			unsigned color = !tr->visible ? 0x414247 : t % 2 ? 0x4b4662
															 : 0x265c72;
			YMGUI_Draw_Fill(&clipped, &rect, ST_RGB(color), GY_OPA_COVER);
			YMGUI_Draw_Text(&clipped, &YMGUI_Font_Default, x + 7, y + 5, st_basename(s->project.media[c->media].path), ST_RGB(0xe4edf3));
			GYIMG in = studio_clip_thumbnail(s, c->media, c->in);
			GYIMG out = studio_clip_thumbnail(s, c->media, c->in + c->length - 1);
			if (in && left >= ST_HEADER)
				YMGUI_Draw_ImgScaled(&clipped, in, (GYrect){left + 5, y + 25, 64, 34});
			if (out && right <= ST_W && right - left > 150)
				YMGUI_Draw_ImgScaled(&clipped, out, (GYrect){right - 69, y + 25, 64, 34});
			if (right - left > 250)
			{
				char length[32];
				studio_timecode(length, sizeof(length), c->length);
				YMGUI_Draw_Text(&clipped, &YMGUI_Font_Default, x + 80, y + 36, length, ST_RGB(0xb9cbd6));
			}

			if (c->id == s->selected_clip)
				border(&clipped, rect, 0xf4ba5d);
			if (left >= ST_HEADER)
				YMGUI_Draw_Fill(&clipped, &(GYrect){left + 2, y + 24, 3, 18}, ST_RGB(0xa5c2d0), GY_OPA_COVER);
			if (right <= ST_W)
				YMGUI_Draw_Fill(&clipped, &(GYrect){right - 5, y + 24, 3, 18}, ST_RGB(0xa5c2d0), GY_OPA_COVER);
		}
	}
	if (s->ghost_track >= 0 && s->drag_moved)
	{
		int y = track_y(s, s->ghost_track), x = screen_x(s, s->ghost_at), r = screen_x(s, s->ghost_at + s->ghost_length);
		if (x < ST_HEADER)
			x = ST_HEADER;
		if (r > ST_W)
			r = ST_W;
		if (r > x && y >= ST_TL_Y + ST_RULER && y + ST_ROW <= ST_TL_Y + ST_TL_H)
		{
			GYrect rect = {x, y + 2, r - x, ST_ROW - 5};
			border(surface, rect, s->ghost_valid ? 0x7ce2b4 : 0xed7777);
		}
	}
	if (!st_duration(&s->project))
		YMGUI_Draw_Text(surface, &YMGUI_Font_Default, ST_HEADER + 28, ST_TL_Y + 100, "将素材拖到这里，开始多轨剪辑", ST_RGB(0x7f8a97));
}
static int magnet(Studio* s, int at, int length, int skip)
{
	if (s->alt)
		return at;
	int best = at;
	double dist = 7.0 / scale(s);
	int candidates[ST_TRACKS * ST_CLIPS * 2 + 2], n = 0;
	candidates[n++] = 0;
	candidates[n++] = YMGUI_State_GetInt(&s->position);
	for (int t = 0; t < s->project.track_count; t++)
		for (int i = 0; i < s->project.tracks[t].count; i++)
		{
			StClip c = s->project.tracks[t].clips[i];
			if (c.id != skip)
			{
				candidates[n++] = c.start;
				candidates[n++] = c.start + c.length;
			}
		}
	for (int i = 0; i < n; i++)
		for (int edge = 0; edge < 2; edge++)
		{
			int value = candidates[i] - (edge ? length : 0);
			double d = fabs(value - (double)at);
			if (value >= 0 && d < dist)
			{
				best = value;
				dist = d;
			}
		}
	return best;
}
static int placement_ok(Studio* s, int track, int at, int length, int skip)
{
	if (track < 0 || track >= s->project.track_count || s->project.tracks[track].locked || at < 0 || length <= 0 || at > ST_MAX_FRAME - length)
		return 0;
	StTrack* t = &s->project.tracks[track];
	for (int i = 0; i < t->count; i++)
	{
		StClip c = t->clips[i];
		if (c.id != skip && at < c.start + c.length && at + length > c.start)
			return 0;
	}
	return 1;
}
static void drag_update(Studio* s)
{
	int x = s->ctx->point_x, y = s->ctx->point_y;
	if (abs(x - s->press_x) + abs(y - s->press_y) > 4)
		s->drag_moved = 1;
	int delta = studio_frame_at(s, x) - s->press_frame, at = s->drag_start + delta, length = s->drag_length;
	int track = studio_track_at(s, y);
	if (s->drag_kind == 3)
	{
		at = magnet(s, s->drag_start + delta, 0, s->drag_id);
		length = s->drag_start + s->drag_length - at;
		track = s->drag_track;
	}
	else if (s->drag_kind == 4)
	{
		at = s->drag_start;
		length = magnet(s, s->drag_start + s->drag_length + delta, 0, s->drag_id) - s->drag_start;
		track = s->drag_track;
	}
	else
	{
		at = magnet(s, at, length, s->drag_id);
	}
	s->ghost_track = track;
	s->ghost_at = at;
	s->ghost_length = length;
	s->ghost_valid = placement_ok(s, track, at, length, s->drag_id) && !s->project.tracks[s->drag_track].locked;
	if (s->drag_kind == 3 || s->drag_kind == 4)
	{
		const StClip* c = st_clip(&s->project, s->drag_id, NULL);
		int in = s->drag_in + at - s->drag_start;
		if (!c || in < 0 || in > s->project.media[c->media].frames - length)
			s->ghost_valid = 0;
	}
	YMGUI_Obj_Invalidate(s->timeline);
}
static void reset_drag(Studio* s)
{
	s->drag_kind = 0;
	s->ghost_track = -1;
	s->drag_moved = 0;
	s->scrubbing = 0;
	YMGUI_Obj_Invalidate(s->timeline);
}
static int edge_scroll(Studio* s)
{
	if (!s->drag_kind || SDL_GetTicks() - s->edge_tick < 90)
		return 0;
	int x = s->ctx->point_x, y = s->ctx->point_y;
	if (y < ST_TL_Y || y > ST_TL_Y + ST_TL_H)
		return 0;
	int direction = x > ST_W - 28 ? 1 : x >= ST_HEADER && x < ST_HEADER + 28 ? -1
																			 : 0;
	if (!direction)
		return 0;
	int frame = YMGUI_State_GetInt(&s->scroll) + direction * (int)(24 / scale(s) + 1);
	int maximum = st_duration(&s->project) + ST_FPS * 10;
	if (frame < 0)
		frame = 0;
	if (frame > maximum)
		frame = maximum;
	YMGUI_State_SetInt(&s->scroll, frame);
	s->edge_tick = SDL_GetTicks();
	return 1;
}
void studio_timeline_event(GYOBJ o, GYEvent e)
{
	Studio* s = studio_of(o);
	int x = s->ctx->point_x, y = s->ctx->point_y;
	int t = studio_track_at(s, y);
	if (e == GY_EVENT_ReleasedOff && !s->ctx->pressed_obj)
	{
		reset_drag(s);
		return;
	}
	if (e == GY_EVENT_Tick)
	{
		if (!edge_scroll(s))
			return;
		e = GY_EVENT_Pressing;
	}
	if (e == GY_EVENT_Wheel)
	{
		if (SDL_GetModState() & KMOD_CTRL)
			studio_action(s, s->ctx->wheel_y > 0 ? ACT_ZOOM_IN : ACT_ZOOM_OUT);
		else if (x < ST_HEADER)
		{
			s->track_scroll -= s->ctx->wheel_y;
			if (s->track_scroll < 0)
				s->track_scroll = 0;
			studio_ui_refresh(s);
		}
		else
		{
			int scroll = YMGUI_State_GetInt(&s->scroll) - (s->ctx->wheel_y + s->ctx->wheel_x) * ST_FPS * 2;
			int max = st_duration(&s->project) + ST_FPS * 10;
			if (scroll < 0)
				scroll = 0;
			if (scroll > max)
				scroll = max;
			s->internal_change = 1;
			YMGUI_State_SetInt(&s->scroll, scroll);
			s->internal_change = 0;
		}
		return;
	}
	if (e == GY_EVENT_Pressed)
	{
		reset_drag(s);
		s->press_x = x;
		s->press_y = y;
		s->press_frame = studio_frame_at(s, x);
		if (x < ST_HEADER && t >= 0)
		{
			s->selected_track = t;
			s->drag_kind = 5;
			studio_ui_refresh(s);
			return;
		}
		if (o != s->head && y >= ST_TL_Y + ST_RULER && t >= 0)
		{
			StTrack* tr = &s->project.tracks[t];
			int frame = studio_frame_at(s, x);
			for (int i = 0; i < tr->count; i++)
			{
				StClip* c = &tr->clips[i];
				if (frame >= c->start && frame < c->start + c->length)
				{
					s->selected_track = t;
					s->selected_clip = c->id;
					s->selected_media = c->media;
					s->drag_track = t;
					s->drag_id = c->id;
					s->drag_start = c->start;
					s->drag_in = c->in;
					s->drag_length = c->length;
					s->drag_kind = abs(x - screen_x(s, c->start)) <= 7 ? 3 : abs(x - screen_x(s, c->start + c->length)) <= 7 ? 4
																															 : 2;
					studio_mode(s, 0);
					studio_ui_refresh(s);
					return;
				}
			}
		}
		s->drag_kind = 1;
		s->scrubbing = 1;
		studio_mode(s, 0);
		studio_seek(s, studio_frame_at(s, x), 0);
	}
	else if (e == GY_EVENT_Pressing)
	{
		if (s->drag_kind == 1)
			studio_seek(s, studio_frame_at(s, x), 0);
		else if (s->drag_kind >= 2 && s->drag_kind <= 4)
			drag_update(s);
	}
	else if (e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff)
	{
		if (s->drag_kind == 1)
			studio_seek(s, studio_frame_at(s, x), 1);
		else if (s->drag_kind >= 2 && s->drag_kind <= 4)
		{
			drag_update(s);
			if (s->drag_moved && s->ghost_valid)
			{
				StEdit edit = {.kind = ST_MOVE, .track = s->ghost_track, .id = s->drag_id, .at = s->ghost_at};
				if (s->drag_kind != 2)
				{
					/* Only trimming changes the source range; moving preserves it. */
					edit.kind = ST_TRIM;
					edit.in = s->drag_in + s->ghost_at - s->drag_start;
					edit.length = s->ghost_length;
				}
				if (studio_edit(s, edit))
					s->selected_track = s->ghost_track;
			}
			else if (s->drag_moved)
				studio_status(s, "不能放置：重叠、锁定或超出素材边界");
		}
		else if (s->drag_kind == 5 && x < ST_HEADER && t == s->selected_track && y >= track_y(s, t) + 32)
		{
			StEdit edit = {.kind = x < 75 ? ST_TRACK_HIDE : ST_TRACK_LOCK, .track = t};
			studio_edit(s, edit);
		}
		reset_drag(s);
		studio_ui_refresh(s);
	}
}
GYIMG studio_thumbnail(Studio* s, int media)
{
	if (media < 0 || media >= s->project.media_count)
		return NULL;
	for (int i = 0; i < s->thumb_count; i++)
		if (!strcmp(s->thumbs[i].path, s->project.media[media].path))
			return &s->thumbs[i].image;
	return NULL;
}
void studio_bin_draw(GYOBJ o, GYSURFACE surf, const GYrect* a)
{
	Studio* s = studio_of(o);
	YMGUI_Draw_Fill(surf, a, ST_RGB(0x25282d), GY_OPA_COVER);
	for (int row = 0; row < ST_BIN_ROWS; row++)
	{
		int m = s->bin_scroll + row;
		if (m >= s->project.media_count)
			break;
		GYrect rect = {a->x + 3, a->y + row * 65 + 2, a->w - 6, 62};
		GYsurface clipped;
		if (!intersect(&clipped, surf, rect))
			continue;
		YMGUI_Draw_Fill(&clipped, &rect, ST_RGB(m == s->selected_media ? 0x3b5669 : 0x2c3036), GY_OPA_COVER);
		GYIMG img = studio_thumbnail(s, m);
		if (img)
			YMGUI_Draw_ImgScaled(&clipped, img, (GYrect){rect.x + 4, rect.y + 7, 80, 45});
		YMGUI_Draw_Text(&clipped, &YMGUI_Font_Default, rect.x + 91, rect.y + 10, st_basename(s->project.media[m].path), ST_RGB(0xe1e7ed));
		char text[32];
		studio_timecode(text, sizeof(text), s->project.media[m].frames);
		YMGUI_Draw_Text(&clipped, &YMGUI_Font_Default, rect.x + 91, rect.y + 34, text, ST_RGB(0x9fafbf));
		if (s->proxy_states[m])
			YMGUI_Draw_Fill(&clipped, &(GYrect){rect.x + rect.w - 6, rect.y + 3, 3, rect.h - 6}, ST_RGB(s->proxy_states[m] == 2 ? 0x72bf9c : s->proxy_states[m] == 1 ? 0xc8a766
																																									 : 0xb46c6c),
							GY_OPA_COVER);
	}
	if (!s->project.media_count)
	{
		YMGUI_Draw_Text(surf, &YMGUI_Font_Default, a->x + 22, a->y + 116, "素材库为空", ST_RGB(0xa5b1bf));
		YMGUI_Draw_Text(surf, &YMGUI_Font_Default, a->x + 22, a->y + 148, "打开视频或拖入文件", ST_RGB(0x76828e));
	}
}
void studio_bin_event(GYOBJ o, GYEvent e)
{
	Studio* s = studio_of(o);
	GYrect a;
	YMGUI_Obj_GetAbsArea(o, &a);
	int x = s->ctx->point_x, y = s->ctx->point_y;
	if (e == GY_EVENT_ReleasedOff && !s->ctx->pressed_obj)
	{
		reset_drag(s);
		return;
	}
	if (e == GY_EVENT_Tick)
	{
		if (!edge_scroll(s))
			return;
		e = GY_EVENT_Pressing;
	}
	if (e == GY_EVENT_Wheel)
	{
		s->bin_scroll -= s->ctx->wheel_y;
		int max = s->project.media_count - ST_BIN_ROWS;
		if (max < 0)
			max = 0;
		if (s->bin_scroll < 0)
			s->bin_scroll = 0;
		if (s->bin_scroll > max)
			s->bin_scroll = max;
		YMGUI_Obj_Invalidate(o);
		return;
	}
	if (e == GY_EVENT_Pressed)
	{
		int media = (y - a.y) / 65 + s->bin_scroll;
		if (media < 0 || media >= s->project.media_count)
			return;
		s->selected_media = media;
		if (YMGUI_State_GetInt(&s->mode))
			studio_seek(s, 0, 1);
		s->selected_clip = -1;
		s->drag_kind = 6;
		s->drag_moved = 0;
		s->press_x = x;
		s->press_y = y;
		s->press_frame = studio_frame_at(s, x);
		studio_ui_refresh(s);
	}
	else if (e == GY_EVENT_DoubleClicked)
	{
		studio_mode(s, 1);
		studio_seek(s, 0, 1);
	}
	else if ((e == GY_EVENT_Pressing || e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff) && s->drag_kind == 6)
	{
		if (abs(x - s->press_x) + abs(y - s->press_y) > 4)
			s->drag_moved = 1;
		s->ghost_track = studio_track_at(s, y);
		s->ghost_length = s->project.media[s->selected_media].frames;
		s->ghost_at = magnet(s, studio_frame_at(s, x), s->ghost_length, -1);
		s->ghost_valid = x >= ST_HEADER && placement_ok(s, s->ghost_track, s->ghost_at, s->ghost_length, -1);
		YMGUI_Obj_Invalidate(s->timeline);
		if (e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff)
		{
			if (s->drag_moved && s->ghost_valid)
			{
				StEdit edit = {.kind = ST_ADD, .track = s->ghost_track, .media = s->selected_media, .at = s->ghost_at, .length = s->ghost_length};
				if (studio_edit(s, edit))
				{
					s->selected_track = s->ghost_track;
					studio_mode(s, 0);
					studio_seek(s, s->ghost_at, 1);
				}
			}
			reset_drag(s);
		}
	}
}
