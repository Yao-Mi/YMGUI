#include "studio.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Invalidate.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
void studio_timecode(char* dst, size_t size, int f)
{
	if (f < 0)
		f = 0;
	snprintf(dst, size, "%02d:%02d:%02d:%02d", f / (ST_FPS * 3600), f / (ST_FPS * 60) % 60, f / ST_FPS % 60, f % ST_FPS);
}
int studio_position(Studio* s)
{
	return YMGUI_State_GetInt(YMGUI_State_GetInt(&s->mode) ? &s->source_position : &s->position);
}
int studio_duration(Studio* s)
{
	return YMGUI_State_GetInt(&s->mode) ? (s->selected_media >= 0 && s->selected_media < s->project.media_count ? s->project.media[s->selected_media].frames : 0) : st_duration(&s->project);
}
static void time_apply(GYOBJ obj, const GYval* v)
{
	(void)v;
	Studio* s = studio_of(obj);
	char a[32], b[32], text[80];
	studio_timecode(a, sizeof(a), studio_position(s));
	studio_timecode(b, sizeof(b), studio_duration(s));
	snprintf(text, sizeof(text), "%s / %s", a, b);
	YMGUI_Label_SetText(obj, text);
}
static void play_apply(GYOBJ obj, const GYval* v)
{
	YMGUI_Button_SetText(obj, v->u.i ? "暂停" : "播放");
}
static void position_changed(GYSUBJECT subject, const GYval* v, void* user)
{
	(void)subject;
	(void)v;
	Studio* s = user;
	if (s->internal_change == 2)
	{
		time_apply(s->timecode, NULL);
		return;
	}
	if (!s->internal_change)
		YMGUI_State_SetBool(&s->playing, 0);
	s->request_dirty = 1;
	time_apply(s->timecode, NULL);
}
static void geometry_changed(GYSUBJECT subject, const GYval* v, void* user)
{
	(void)subject;
	(void)v;
	Studio* s = user;
	YMGUI_Obj_Invalidate(s->timeline);
	int internal = s->internal_change;
	s->internal_change = 2;
	YMGUI_State_Touch(&s->position);
	s->internal_change = internal;
}
static void revision_changed(GYSUBJECT subject, const GYval* v, void* user)
{
	(void)subject;
	(void)v;
	studio_ui_refresh(user);
}
static void play_changed(GYSUBJECT subject, const GYval* v, void* user)
{
	(void)subject;
	Studio* s = user;
	if (v->u.i)
	{
		s->anchor_frame = studio_position(s);
		s->anchor_tick = SDL_GetTicks();
	}
}
void studio_status(Studio* s, const char* message)
{
	snprintf(s->status_text, sizeof(s->status_text), "%s", message);
	YMGUI_State_Touch(&s->status);
}
void studio_bind(Studio* s)
{
	YMGUI_Bind_Attach(s->head, &s->position, studio_head_apply);
	YMGUI_Bind_Attach(s->play, &s->playing, play_apply);
	YMGUI_Slider_Bind(s->seek, &s->position);
	YMGUI_Slider_Bind(s->scrollbar, &s->scroll);
	s->observers[0] = YMGUI_State_AddObserver(&s->position, position_changed, s);
	s->observers[1] = YMGUI_State_AddObserver(&s->source_position, position_changed, s);
	s->observers[2] = YMGUI_State_AddObserver(&s->zoom, geometry_changed, s);
	s->observers[3] = YMGUI_State_AddObserver(&s->scroll, geometry_changed, s);
	s->observers[4] = YMGUI_State_AddObserver(&s->revision, revision_changed, s);
	s->observers[5] = YMGUI_State_AddObserver(&s->playing, play_changed, s);
	time_apply(s->timecode, NULL);
}
void studio_unbind(Studio* s)
{
	for (int i = 0; i < 7; i++)
		YMGUI_State_RemoveObserver(s->observers[i]);
}
void studio_mode(Studio* s, int source)
{
	if (source && s->selected_media < 0)
		return;
	YMGUI_State_SetBool(&s->playing, 0);
	YMGUI_State_SetInt(&s->mode, source);
	int end = studio_duration(s), pos = studio_position(s);
	if (pos >= end)
		YMGUI_State_SetInt(source ? &s->source_position : &s->position, end > 0 ? end - 1 : 0);
	YMGUI_Bind_Unlink(s->seek);
	YMGUI_Slider_SetRange(s->seek, 0, studio_duration(s) > 0 ? studio_duration(s) - 1 : 0);
	YMGUI_Slider_Bind(s->seek, source ? &s->source_position : &s->position);
	studio_ui_refresh(s);
	studio_request(s, 1);
}
void studio_seek(Studio* s, int frame, int final)
{
	int end = studio_duration(s);
	if (frame < 0)
		frame = 0;
	if (frame >= end)
		frame = end > 0 ? end - 1 : 0;
	YMGUI_State_SetBool(&s->playing, 0);
	YMGUI_State_SetInt(YMGUI_State_GetInt(&s->mode) ? &s->source_position : &s->position, frame);
	if (final)
		studio_request(s, 1);
	else
		s->request_dirty = 1;
}
