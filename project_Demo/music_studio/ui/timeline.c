#include "app/studio.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tick_at(MsApp* a, int x)
{
	int t = a->scroll + (int)((x - MS_TIMEX) / a->zoom);
	int grid = a->snap ? MS_STEP : 1;
	return ms_clamp((int)lround((double)t / grid) * grid, 0, MS_END);
}
static int pixel(MsApp* a, int tick)
{
	return MS_TIMEX + (int)((tick - a->scroll) * a->zoom);
}
void ms_timeline_zoom_at(MsApp* a, int x, int steps)
{
	double anchor = ms_clamp(x - MS_TIMEX, 0, MS_W - MS_TIMEX - 13);
	double tick = a->scroll + anchor / a->zoom;
	a->zoom = fmax(.12, fmin(12, a->zoom * pow(1.5, ms_clamp(steps, -32, 32))));
	int end = MS_END - (int)ceil((MS_W - MS_TIMEX - 12) / a->zoom);
	a->scroll = ms_clamp((int)lround(tick - anchor / a->zoom), 0, end);
	YMGUI_Obj_Invalidate(a->timeline);
	ms_status(a, "时间线：Ctrl＋滚轮横向缩放，普通滚轮左右移动；实际时长不变");
}
void ms_timeline_draw(GYOBJ o, GYSURFACE surface, const GYrect* r)
{
	MsApp* a = o->user_data;
	MsSong* s = &a->project.song;
	char b[64];
	ms_fill(surface, r->x, r->y, r->w, r->h, 0x22262b);
	ms_fill(surface, r->x, r->y, 176, 32, 0x30363d);
	ms_text(surface, r->x + 12, r->y + 8, "轨道 / 静音 / 独奏", MS_DIM);
	for (int t = 0; t < s->tracks; ++t)
	{
		int y = MS_TIMEY + t * MS_ROWH;
		ms_fill(surface, r->x, y, 176, MS_ROWH - 1, a->track == t ? 0x3c484e : 0x2e3339);
		ms_fill(surface, r->x, y, 4, MS_ROWH - 1, s->track[t].color);
		GYsurface header = *surface;
		header.clip.x = r->x + 6;
		header.clip.w = 106;
		ms_text(&header, r->x + 10, y + 9, s->track[t].name, MS_TEXT);
		ms_fill(surface, r->x + 114, y + 6, 25, 23, s->track[t].mute ? 0x99734a : 0x424a53);
		ms_fill(surface, r->x + 144, y + 6, 25, 23, s->track[t].solo ? 0x548a70 : 0x424a53);
		ms_text(surface, r->x + 118, y + 9, "静", MS_TEXT);
		ms_text(surface, r->x + 148, y + 9, "独", MS_TEXT);
		ms_fill(surface, MS_TIMEX, y, MS_W - MS_TIMEX - 12, MS_ROWH - 1, t % 2 ? 0x272c32 : 0x2b3036);
	}
	GYsurface area = *surface;
	area.clip.x = MS_TIMEX;
	area.clip.w = MS_W - MS_TIMEX - 12;
	surface = &area;
	int bar = s->beats * MS_PPQ;
	for (int tick = a->scroll / MS_PPQ * MS_PPQ; tick < MS_END && pixel(a, tick) < MS_W - 12; tick += MS_PPQ)
	{
		int x = pixel(a, tick);
		ms_fill(surface, x, r->y, 1, r->h, tick % bar ? 0x333a42 : 0x4b535c);
		if (tick % bar == 0)
		{
			snprintf(b, sizeof(b), "%d", tick / bar + 1);
			ms_text(surface, x + 6, r->y + 9, b, MS_TEXT);
		}
	}
	int lx = pixel(a, s->loop_start), rx = pixel(a, s->loop_end);
	ms_fill(surface, lx, r->y + 27, rx - lx, 4, 0x5da895);
	for (int i = 0; i < s->clips; ++i)
	{
		MsClip* c = &s->clip[i];
		MsTrack* track = &s->track[c->track];
		int x = pixel(a, c->start), end = pixel(a, c->start + c->length), y = MS_TIMEY + c->track * MS_ROWH + 3;
		if (end <= MS_TIMEX || x >= MS_W - 12)
			continue;
		int left = x < MS_TIMEX ? MS_TIMEX : x, right = end > MS_W - 12 ? MS_W - 12 : end;
		unsigned color = track->color;
		unsigned dark = ((color >> 16) * .42);
		dark = (dark << 16) | ((unsigned)(((color >> 8) & 255) * .42) << 8) | (unsigned)((color & 255) * .42);
		ms_fill(surface, left, y, right - left, 27, dark);
		ms_fill(surface, left, y, right - left, 3, color);
		if (track->kind == MS_AUDIO && c->source < a->project.assets)
		{
			MsAsset* asset = &a->project.asset[c->source];
			for (int px = left; px < right; px += 2)
			{
				double local = ms_tick_frame(s, (px - x) / a->zoom + c->offset);
				int bin = (int)(local * 512 / asset->frames);
				int height = bin >= 0 && bin < 512 ? (int)(asset->peaks[bin] * 10) : 0;
				ms_fill(surface, px, y + 15 - height, 1, height * 2 + 1, color);
			}
		}
		else
		{
			MsPattern* p = &s->pattern[c->source];
			for (int step = 0; step < p->steps; ++step)
			{
				int hit = 0;
				for (int d = 0; d < MS_DRUMS; ++d)
					hit |= p->drum[d][step];
				if (track->kind == MS_DRUM && hit)
					ms_fill(surface, x + (int)(step * MS_STEP * a->zoom), y + 19, 3, 5, color);
			}
		}
		/* 淡化时长以秒保存，按当前速度换算为时间线上的斜线。 */
		double fade_in_px = c->fade_in * s->bpm * MS_PPQ / 60.0 * a->zoom;
		double fade_out_px = c->fade_out * s->bpm * MS_PPQ / 60.0 * a->zoom;
		for (int px = left; px < right; px += 2)
		{
			if (fade_in_px > 0 && px - x < fade_in_px)
				ms_fill(surface, px, y + 25 - (int)((px - x) / fade_in_px * 24), 2, 2, 0xe2d7ac);
			if (fade_out_px > 0 && end - px < fade_out_px)
				ms_fill(surface, px, y + 25 - (int)((end - px) / fade_out_px * 24), 2, 2, 0xe2d7ac);
		}
		GYsurface text = *surface;
		text.clip.x = left + 5;
		text.clip.w = right - left - 9;
		if (text.clip.w > 0)
			ms_text(&text, left + 5, y + 6, track->kind == MS_AUDIO ? a->project.asset[c->source].name : s->pattern[c->source].name, MS_TEXT);
		if (i == a->clip)
			ms_border(surface, left, y, right - left, 27, 0xf1d2a2);
		ms_fill(surface, left, y + 6, 2, 15, color);
		ms_fill(surface, right - 2, y + 6, 2, 15, color);
	}
	if (a->drag == 2 && a->drag_active && a->drop_track >= 0)
	{
		int x = pixel(a, a->drop_tick), end = pixel(a, a->drop_tick + a->drop_length);
		int y = MS_TIMEY + a->drop_track * MS_ROWH + 2;
		unsigned color = a->drop_valid ? MS_ACCENT : 0xe88980;
		ms_fill(surface, x, y, end - x, 29, a->drop_valid ? 0x304d47 : 0x593c40);
		ms_border(surface, x, y, end - x, 29, color);
		ms_fill(surface, x, MS_TIMEY, 2, MS_ROWH * s->tracks, color);
	}
	int play = pixel(a, (int)ms_frame_tick(s, ms_transport_position(&a->transport)));
	ms_fill(surface, play, r->y, 2, r->h, 0xef887d);
}
/* 片段音色属于引用它的轨道；共享片段优先当前实际引用，未编排时保留同类目标。 */
static int select_source_track(MsApp* a, int kind, int source)
{
	MsSong* s = &a->project.song;
	int chosen = -1;
	if (a->clip >= 0 && a->clip < s->clips && s->clip[a->clip].source == source &&
		s->clip[a->clip].track == a->track && s->track[a->track].kind == kind)
		chosen = a->clip;
	for (int i = 0; chosen < 0 && i < s->clips; ++i)
		if (s->clip[i].source == source && s->track[s->clip[i].track].kind == kind && s->clip[i].track == a->track)
			chosen = i;
	for (int i = 0; chosen < 0 && i < s->clips; ++i)
		if (s->clip[i].source == source && s->track[s->clip[i].track].kind == kind)
			chosen = i;
	a->clip = chosen;
	if (chosen >= 0)
		a->track = s->clip[chosen].track;
	else if (a->track < 0 || a->track >= s->tracks || s->track[a->track].kind != kind)
	{
		a->track = -1;
		for (int t = 0; t < s->tracks; ++t)
			if (s->track[t].kind == kind)
			{
				a->track = t;
				break;
			}
	}
	return chosen;
}
void ms_select_pattern(MsApp* a, int index)
{
	if (index < 0 || index >= a->project.song.patterns)
		return;
	a->pattern = index;
	select_source_track(a, a->project.song.pattern[index].kind, index);
	a->transport.pattern = index;
	ms_notes_clear_selection(a);
	a->step = -1;
	a->tab = a->project.song.pattern[index].kind == MS_DRUM ? 0 : 1;
	if (a->transport.pattern_mode)
		ms_transport_seek(&a->transport, 0);
	ms_ui_refresh(a);
}
void ms_add_source(MsApp* a, int track, int tick)
{
	MsSong* s = &a->project.song;
	if (track < 0 || track >= s->tracks)
	{
		ms_status(a, "请先选择一条轨道");
		return;
	}
	int source = a->library_tab ? a->asset : a->pattern;
	int kind = a->library_tab ? MS_AUDIO : source >= 0 && source < s->patterns ? s->pattern[source].kind
																			   : -1;
	if (kind != s->track[track].kind || source < 0 || (kind == MS_AUDIO && source >= a->project.assets))
	{
		ms_status(a, "素材类型与轨道不匹配，请选择对应的鼓轨、乐器轨或音轨");
		return;
	}
	int length = kind == MS_AUDIO ? (int)ms_frame_tick(s, a->project.asset[source].frames) : s->pattern[source].steps * MS_STEP;
	ms_checkpoint(a);
	int clip = ms_clip_add(s, track, source, tick, length);
	if (clip < 0)
	{
		ms_status(a, "片段数量或编排长度达到上限");
		return;
	}
	a->clip = clip;
	a->track = track;
	ms_changed(a);
	ms_status(a, "已加入时间线；拖动移动，两端裁剪");
}
void ms_timeline_event(GYOBJ o, GYEvent event)
{
	MsApp* a = o->user_data;
	if (a->worker)
		return;
	MsSong* s = &a->project.song;
	int x = o->ctx->point_x, y = o->ctx->point_y;
	int track = (y - MS_TIMEY) / MS_ROWH;
	if (event == GY_EVENT_Wheel)
	{
		if (a->drag)
			return;
		if (a->ctrl)
		{
			ms_timeline_zoom_at(a, x, o->ctx->wheel_y);
			return;
		}
		double scroll = a->scroll + ((double)o->ctx->wheel_x - o->ctx->wheel_y) * MS_PPQ;
		a->scroll = (int)fmax(0, fmin(MS_END - (MS_W - MS_TIMEX - 12) / a->zoom, scroll));
		YMGUI_Obj_Invalidate(o);
		return;
	}
	if (event == GY_EVENT_Pressed)
	{
		a->drag = 0;
		if (y < MS_TIMEY && x >= MS_TIMEX)
		{
			ms_transport_seek(&a->transport, (int64_t)ms_tick_frame(s, tick_at(a, x)));
			a->drag = 3;
			return;
		}
		if (track < 0 || track >= s->tracks)
			return;
		a->track = track;
		if (x < MS_TIMEX)
		{
			if (x >= 346)
			{
				ms_checkpoint(a);
				if (x < 376)
					s->track[track].mute ^= 1;
				else
					s->track[track].solo ^= 1;
				ms_changed(a);
			}
			else
				ms_ui_refresh(a);
			return;
		}
		a->clip = -1;
		for (int i = s->clips - 1; i >= 0; --i)
			if (s->clip[i].track == track && x >= pixel(a, s->clip[i].start) && x < pixel(a, s->clip[i].start + s->clip[i].length))
			{
				a->clip = i;
				break;
			}
		if (a->clip >= 0)
		{
			a->drag = 1;
			a->drag_x = x;
			a->drag_y = y;
			a->drag_clip = s->clip[a->clip];
			*a->gesture = *s;
			a->drag_mode = x - pixel(a, a->drag_clip.start) < 7 ? 1 : pixel(a, a->drag_clip.start + a->drag_clip.length) - x < 7 ? 2
																																 : 0;
			if (s->track[track].kind != MS_AUDIO)
				ms_select_pattern(a, s->clip[a->clip].source);
			else
				a->tab = 2;
		}
		else
		{
			ms_transport_seek(&a->transport, (int64_t)ms_tick_frame(s, tick_at(a, x)));
		}
		ms_ui_refresh(a);
	}
	else if (event == GY_EVENT_Pressing)
	{
		if (a->drag == 3)
		{
			ms_transport_seek(&a->transport, (int64_t)ms_tick_frame(s, tick_at(a, x)));
			YMGUI_Obj_Invalidate(o);
		}
		if (a->drag != 1 || a->clip < 0)
			return;
		MsClip c = a->drag_clip;
		int grid = a->snap ? MS_STEP : 1;
		int delta = (int)lround((x - a->drag_x) / a->zoom / grid) * grid;
		if (a->drag_mode == 1)
		{
			delta = ms_clamp(delta, -c.offset, c.length - 1);
			if (c.start + delta < 0)
				delta = -c.start;
			c.start += delta;
			c.offset += delta;
			c.length -= delta;
		}
		else if (a->drag_mode == 2)
		{
			int limit = MS_END - c.start;
			if (s->track[c.track].kind == MS_AUDIO)
				limit = ms_clamp((int)ms_frame_tick(s, a->project.asset[c.source].frames) - c.offset, 1, limit);
			c.length = ms_clamp(c.length + delta, 1, limit);
		}
		else
		{
			c.start = ms_clamp(c.start + delta, 0, MS_END - c.length);
			if (track >= 0 && track < s->tracks && s->track[track].kind == s->track[c.track].kind)
				c.track = track;
		}
		s->clip[a->clip] = c;
		YMGUI_Obj_Invalidate(o);
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
	{
		if (a->drag == 1 && memcmp(a->gesture, s, sizeof(*s)))
		{
			if (event == GY_EVENT_ReleasedOff)
				*s = *a->gesture;
			else
			{
				ms_history_push(a->history, a->gesture);
				a->track = s->clip[a->clip].track;
				ms_changed(a);
			}
		}
		a->drag = 0;
		ms_ui_refresh(a);
	}
	else if (event == GY_EVENT_DoubleClicked && a->clip >= 0)
	{
		MsClip* c = &s->clip[a->clip];
		if (s->track[c->track].kind != MS_AUDIO)
			ms_select_pattern(a, c->source);
	}
}
void ms_library_draw(GYOBJ o, GYSURFACE surface, const GYrect* r)
{
	MsApp* a = o->user_data;
	ms_fill(surface, r->x, r->y, r->w, r->h, MS_PANEL);
	ms_text(surface, r->x + 10, r->y + 10, a->library_tab ? "音频素材库" : "节奏型与旋律片段", MS_ACCENT);
	int count = a->library_tab ? a->project.assets : a->project.song.patterns;
	GYsurface clip = *surface;
	clip.clip.x = r->x;
	clip.clip.w = r->w;
	surface = &clip;
	for (int i = 0; i < count && i < 16; ++i)
	{
		int y = r->y + 38 + i * 17;
		if (i == (a->library_tab ? a->asset : a->pattern))
			ms_fill(surface, r->x + 4, y - 1, r->w - 8, 17, 0x42544f);
		ms_text(surface, r->x + 10, y, a->library_tab ? a->project.asset[i].name : a->project.song.pattern[i].name, MS_TEXT);
	}
	if (!count)
		ms_text(surface, r->x + 10, r->y + 56, "点击“导入音频”添加", MS_DIM);
	ms_text(surface, r->x + 10, r->y + r->h - 18, "拖入同类轨道即可编排", MS_DIM);
}
static void update_drop(MsApp* a, int x, int y)
{
	MsSong* s = &a->project.song;
	int source = a->library_tab ? a->asset : a->pattern;
	int count = a->library_tab ? a->project.assets : s->patterns;
	int kind = source >= 0 && source < count ? (a->library_tab ? MS_AUDIO : s->pattern[source].kind) : -1;
	a->drop_length = kind < 0 ? 0 : kind == MS_AUDIO ? (int)ms_frame_tick(s, a->project.asset[source].frames)
													 : s->pattern[source].steps * MS_STEP;
	a->drop_track = x >= MS_TIMEX && x < MS_W - 12 && y >= MS_TIMEY && y < MS_TIMEY + s->tracks * MS_ROWH ? (y - MS_TIMEY) / MS_ROWH : -1;
	a->drop_tick = tick_at(a, x);
	a->drop_valid = a->drop_track >= 0 && kind == s->track[a->drop_track].kind && a->drop_length > 0 &&
					s->clips < MS_CLIPS && a->drop_tick <= MS_END - a->drop_length;
	if (a->drag_ghost)
	{
		YMGUI_Obj_Invalidate(a->drag_ghost);
		a->drag_ghost->area.x = ms_clamp(x + 18, 0, MS_W - 230);
		a->drag_ghost->area.y = ms_clamp(y + 18, 0, MS_H - 48);
		YMGUI_Obj_SetHidden(a->drag_ghost, !a->drag_active);
		YMGUI_Obj_Invalidate(a->drag_ghost);
	}
	YMGUI_Obj_Invalidate(a->timeline);
}
void ms_drag_ghost_draw(GYOBJ o, GYSURFACE surface, const GYrect* r)
{
	MsApp* a = o->user_data;
	unsigned color = a->drop_valid ? MS_ACCENT : 0xe88980;
	ms_fill(surface, r->x, r->y, r->w, r->h, 0x30383d);
	ms_border(surface, r->x, r->y, r->w, r->h, color);
	const char* name = a->library_tab ? a->project.asset[a->asset].name : a->project.song.pattern[a->pattern].name;
	ms_text(surface, r->x + 8, r->y + 6, name, MS_TEXT);
	char hint[96];
	if (a->drop_valid)
		snprintf(hint, sizeof(hint), "松开放入第 %d 轨 · 第 %d 拍", a->drop_track + 1, a->drop_tick / MS_PPQ + 1);
	else
		snprintf(hint, sizeof(hint), "%s", a->drop_track < 0 ? "请拖到对应类型的轨道" : "无法放置：类型或长度不符");
	ms_text(surface, r->x + 8, r->y + 27, hint, color);
}
void ms_library_event(GYOBJ o, GYEvent event)
{
	MsApp* a = o->user_data;
	if (a->worker)
		return;
	int x = o->ctx->point_x, y = o->ctx->point_y;
	if (event == GY_EVENT_Pressed)
	{
		int i = (y - o->area.y - 38) / 17;
		if (y < o->area.y + 38)
			return;
		int count = a->library_tab ? a->project.assets : a->project.song.patterns;
		if (i < 0 || i >= count || i >= 16)
			return;
		a->drag = 2;
		a->drag_active = 0;
		a->drag_x = x;
		a->drag_y = y;
		if (a->library_tab)
		{
			a->asset = i;
			select_source_track(a, MS_AUDIO, i);
			a->tab = 2;
		}
		else
			ms_select_pattern(a, i);
		ms_ui_refresh(a);
		ms_status(a, a->clip >= 0 ? "已切换到片段引用的轨道；共享片段优先当前引用轨道" : "片段尚未编排，使用当前同类轨道的音色");
		update_drop(a, x, y);
	}
	else if (event == GY_EVENT_Pressing && a->drag == 2)
	{
		if (abs(x - a->drag_x) + abs(y - a->drag_y) >= 6)
			a->drag_active = 1;
		update_drop(a, x, y);
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
	{
		int dropping = a->drag == 2 && a->drag_active && o->ctx->pressed_obj == o;
		if (dropping)
			update_drop(a, x, y);
		if (dropping && a->drop_valid)
			ms_add_source(a, a->drop_track, a->drop_tick);
		else if (a->drag_active)
			ms_status(a, "已取消放置，工程未改变");
		a->drag = a->drag_active = a->drop_valid = 0;
		a->drop_track = -1;
		if (a->drag_ghost)
			YMGUI_Obj_SetHidden(a->drag_ghost, 1);
		YMGUI_Obj_Invalidate(a->timeline);
	}
}
