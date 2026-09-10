#include "studio.h"

void studio_changed(Studio* s)
{
	if (s->selected_track >= s->project.track_count)
		s->selected_track = s->project.track_count - 1;
	if (s->selected_media >= s->project.media_count)
		s->selected_media = s->project.media_count - 1;
	if (!st_clip(&s->project, s->selected_clip, NULL))
		s->selected_clip = -1;
	YMGUI_State_SetBool(&s->playing, 0);
	s->internal_change = 1;
	int end = st_duration(&s->project), pos = YMGUI_State_GetInt(&s->position);
	if (pos >= end)
		YMGUI_State_SetInt(&s->position, end > 0 ? end - 1 : 0);
	s->internal_change = 0;
	YMGUI_State_SetInt(&s->revision, YMGUI_State_GetInt(&s->revision) + 1);
	studio_request(s, 1);
}
int studio_edit(Studio* s, StEdit edit)
{
	if (!st_project_edit(&s->project, edit))
	{
		studio_status(s, "操作未生效：片段重叠、轨道锁定或已到素材边界");
		return 0;
	}
	if (edit.kind == ST_ADD)
		s->selected_clip = s->project.next_id - 1;
	st_history_push(&s->history, &s->project);
	s->dirty = 1;
	studio_changed(s);
	studio_status(s, "剪辑已更新");
	return 1;
}
void studio_action(Studio* s, StAction a)
{
	int track = s->selected_track;
	const StClip* c = st_clip(&s->project, s->selected_clip, &track);
	StEdit e = {.track = track, .id = s->selected_clip};
	int pos = YMGUI_State_GetInt(&s->position);
	switch (a)
	{
	case ACT_IMPORT:
	case ACT_OPEN:
	case ACT_SAVE:
	case ACT_EXPORT:
		studio_dialog_show(s, a);
		break;
	case ACT_EXPORT_CANCEL:
		st_export_cancel(s->exporter);
		s->export_cancelling = 1;
		studio_export_poll(s);
		break;
	case ACT_UNDO:
	case ACT_REDO:
		if (st_history_step(&s->history, &s->project, a == ACT_UNDO ? -1 : 1))
		{
			s->dirty = 1;
			studio_changed(s);
			studio_status(s, a == ACT_UNDO ? "已撤销" : "已重做");
		}
		break;
	case ACT_PLAY:
		if (studio_duration(s) > 0)
		{
			if (studio_position(s) >= studio_duration(s) - 1)
				studio_seek(s, 0, 1);
			YMGUI_State_SetBool(&s->playing, !YMGUI_State_GetBool(&s->playing));
		}
		break;
	case ACT_PREV:
		studio_seek(s, studio_position(s) - 1, 1);
		break;
	case ACT_NEXT:
		studio_seek(s, studio_position(s) + 1, 1);
		break;
	case ACT_HOME:
		studio_seek(s, 0, 1);
		break;
	case ACT_END:
		studio_seek(s, studio_duration(s) - 1, 1);
		break;
	case ACT_SOURCE:
		studio_mode(s, 1);
		break;
	case ACT_PROJECT:
		studio_mode(s, 0);
		break;
	case ACT_APPEND:
		if (s->selected_media >= 0)
		{
			StTrack* t = &s->project.tracks[s->selected_track];
			int end = t->count ? t->clips[t->count - 1].start + t->clips[t->count - 1].length : 0;
			e = (StEdit){.kind = ST_ADD, .track = s->selected_track, .media = s->selected_media, .at = end, .length = s->project.media[s->selected_media].frames};
			if (studio_edit(s, e))
			{
				studio_mode(s, 0);
				studio_seek(s, end, 1);
			}
		}
		break;
	case ACT_SPLIT:
		if (!YMGUI_State_GetInt(&s->mode))
		{
			e.kind = ST_SPLIT;
			e.at = pos;
			studio_edit(s, e);
		}
		break;
	case ACT_LIFT:
	case ACT_RIPPLE:
		e.kind = a == ACT_LIFT ? ST_LIFT : ST_RIPPLE;
		studio_edit(s, e);
		break;
	case ACT_TRACK:
		e.kind = ST_TRACK_ADD;
		if (studio_edit(s, e))
		{
			s->selected_track = s->project.track_count - 1;
			s->track_scroll = 0;
			studio_ui_refresh(s);
		}
		break;
	case ACT_IN:
	case ACT_OUT:
		if (c && !YMGUI_State_GetInt(&s->mode) && pos >= c->start && pos < c->start + c->length)
		{
			e.kind = ST_TRIM;
			e.in = a == ACT_IN ? c->in + pos - c->start : c->in;
			e.length = a == ACT_IN ? c->length - (pos - c->start) : pos - c->start + 1;
			studio_edit(s, e);
		}
		break;
	case ACT_ZOOM_IN:
	case ACT_ZOOM_OUT:
	{
		int old_zoom = YMGUI_State_GetInt(&s->zoom);
		int old_scroll = YMGUI_State_GetInt(&s->scroll);
		int zoom = old_zoom;
		zoom = a == ACT_ZOOM_IN ? zoom * 5 / 4 : zoom * 4 / 5;
		if (zoom < 10)
			zoom = 10;
		if (zoom > 800)
			zoom = 800;
		s->internal_change = 1;
		YMGUI_State_SetInt(&s->zoom, zoom);
		int scroll = pos - (int)((int64_t)(pos - old_scroll) * old_zoom / zoom);
		if (scroll < 0)
			scroll = 0;
		YMGUI_State_SetInt(&s->scroll, scroll);
		s->internal_change = 0;
		studio_ui_refresh(s);
		break;
	}
	case ACT_FIT:
	{
		int duration = st_duration(&s->project);
		int zoom = duration ? (ST_W - ST_HEADER - 28) * ST_FPS / duration : 70;
		if (zoom < 1)
			zoom = 1;
		if (zoom > 800)
			zoom = 800;
		s->internal_change = 1;
		YMGUI_State_SetInt(&s->zoom, zoom);
		YMGUI_State_SetInt(&s->scroll, 0);
		s->internal_change = 0;
		studio_ui_refresh(s);
		break;
	}
	}
}
