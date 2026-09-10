#include "studio.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_FileDialog.h"
#include "YMGUI_MsgBox.h"
#include "SDL_LCD.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int close_requested(void* user)
{
	Studio* s = user;
	StExportStatus export_status;
	st_export_status(s->exporter, &export_status);
	if (export_status.state == ST_EXPORT_RUNNING)
	{
		studio_confirm(s, -2, s->dirty ? "退出将取消导出，并丢弃未保存的修改。" : "导出仍在进行，退出将取消导出。");
		return 0;
	}
	if (s->dirty)
	{
		studio_confirm(s, -2, "项目尚未保存。退出将丢弃未保存的修改。");
		return 0;
	}
	return 1;
}
static uint8 key_filter(uint32 key, uint8 pressed, void* user)
{
	Studio* s = user;
	if (key == GY_KEY_ALT)
	{
		s->alt = pressed;
		return 0;
	}
	if (key == GY_KEY_SHIFT)
	{
		s->shift = pressed;
		return 0;
	}
	if (!pressed || YMGUI_FileDialog_IsShown(s->dialog) || YMGUI_MsgBox_IsShown(s->msgbox) || s->ctx->point_pressed)
		return 0;
	if (s->ctx->focus_obj && (s->ctx->focus_obj->state & GY_STATE_Editing))
		return 0;
	switch (key)
	{
	case GY_KEY_UNDO:
		studio_action(s, s->shift ? ACT_REDO : ACT_UNDO);
		return 1;
	case GY_KEY_DEL:
		studio_action(s, s->shift ? ACT_RIPPLE : ACT_LIFT);
		return 1;
	case ' ':
		studio_action(s, ACT_PLAY);
		return 1;
	case 's':
	case 'S':
		studio_action(s, ACT_SPLIT);
		return 1;
	case 'i':
	case 'I':
		studio_action(s, ACT_IN);
		return 1;
	case 'o':
	case 'O':
		studio_action(s, ACT_OUT);
		return 1;
	case GY_KEY_LEFT:
		studio_action(s, ACT_PREV);
		return 1;
	case GY_KEY_RIGHT:
		studio_action(s, ACT_NEXT);
		return 1;
	case GY_KEY_HOME:
		studio_action(s, ACT_HOME);
		return 1;
	case GY_KEY_END:
		studio_action(s, ACT_END);
		return 1;
	case '+':
	case '=':
		studio_action(s, ACT_ZOOM_IN);
		return 1;
	case '-':
		studio_action(s, ACT_ZOOM_OUT);
		return 1;
	default:
		return 0;
	}
}
Studio* studio_create(void)
{
	Studio* s = calloc(1, sizeof(*s));
	if (!s)
		return NULL;
	st_project_init(&s->project);
	s->selected_media = -1;
	s->selected_clip = -1;
	s->ghost_track = -1;
	YMGUI_State_InitInt(&s->position, 0);
	YMGUI_State_InitInt(&s->source_position, 0);
	YMGUI_State_InitBool(&s->playing, 0);
	YMGUI_State_InitInt(&s->mode, 0);
	YMGUI_State_InitInt(&s->zoom, 70);
	YMGUI_State_InitInt(&s->scroll, 0);
	YMGUI_State_InitInt(&s->revision, 0);
	YMGUI_State_InitStr(&s->status, s->status_text);
	YMGUI_State_InitBool(&s->export_running, 0);
	YMGUI_State_InitInt(&s->export_progress, 0);
	YMGUI_State_InitStr(&s->export_text, s->export_buffer);
	s->display.hor_res = ST_W;
	s->display.ver_res = ST_H;
	s->display.buf_px_cnt = ST_W * 64;
	s->display.buf1 = GY_malloc1(ST_W * 64 * sizeof(GYpx));
	if (!s->display.buf1)
		goto fail;
	if (SDL_LCD_Init(&s->display, 1))
		goto fail;
	s->ctx = YMGUI_Creat_Ctx_Creat(&s->display, ST_W, ST_H);
	if (!s->ctx)
		goto fail;
	s->ctx->root->user_data = s;
	YMGUI_Inject_SetCtx(s->ctx);
	studio_font_open();
	if (!st_history_init(&s->history, &s->project))
		goto fail;
	s->engine = st_engine_create();
	if (!s->engine)
		goto fail;
	s->exporter = st_export_create();
	if (!s->exporter)
		goto fail;
	s->preview_image = (GYimg){.data = s->pixels, .w = ST_PREVIEW_W, .h = ST_PREVIEW_H};
	studio_ui_build(s);
	studio_dialog_init(s);
	studio_bind(s);
	studio_export_bind(s);
	studio_export_poll(s);
	studio_ui_refresh(s);
	YMGUI_Inject_SetKeyFilter(key_filter, s);
	SDL_LCD_SetCloseRequestCb(close_requested, s);
	SDL_Window* window = SDL_GetWindowFromID(1);
	if (window)
		SDL_SetWindowTitle(window, "video_stidio");
	studio_status(s, "打开视频或拖入文件，然后拖到时间线开始剪辑");
	return s;
fail:
	studio_destroy(s);
	return NULL;
}
void studio_destroy(Studio* s)
{
	if (!s)
		return;
	st_export_destroy(s->exporter);
	st_engine_destroy(s->engine);
	studio_unbind(s);
	YMGUI_Inject_SetKeyFilter(NULL, NULL);
	YMGUI_Inject_SetCtx(NULL);
	SDL_LCD_SetCloseRequestCb(NULL, NULL);
	if (s->ctx)
		YMGUI_Free_CtxFree(s->ctx);
	st_history_free(&s->history);
	studio_font_close();
	SDL_LCD_Destroy();
	GY_free1(s->display.buf1);
	free(s);
}
void studio_request(Studio* s, int final)
{
	const char* path = "";
	int frame = studio_position(s);
	if (YMGUI_State_GetInt(&s->mode))
	{
		if (s->selected_media >= 0 && s->selected_media < s->project.media_count)
			path = s->project.media[s->selected_media].path;
	}
	else
	{
		const StClip* c = st_visible(&s->project, frame);
		if (c)
		{
			path = s->project.media[c->media].path;
			frame = frame - c->start + c->in;
		}
	}
	s->wanted_serial = st_engine_request(s->engine, path, frame);
	if (final)
		s->min_serial = s->wanted_serial;
	s->request_tick = SDL_GetTicks();
	s->request_dirty = 0;
}
void studio_import(Studio* s, const char* path)
{
	char absolute[4096];
	if (!realpath(path, absolute) || strlen(absolute) >= 1024)
	{
		studio_status(s, "无法打开路径，或路径超过 1023 字节");
		return;
	}
	if (s->project.media_count + s->pending_imports >= ST_MEDIA)
	{
		studio_status(s, "素材库已满（64 项）");
		return;
	}
	if (st_engine_import(s->engine, absolute))
	{
		s->pending_imports++;
		studio_status(s, "正在读取素材…");
	}
	else
		studio_status(s, "无法加入导入队列");
}
static void imported(Studio* s, StImportResult* r)
{
	if (s->pending_imports > 0)
		s->pending_imports--;
	if (!r->ok)
	{
		char text[256];
		snprintf(text, sizeof(text), "素材读取失败：%s", r->error);
		studio_status(s, text);
		return;
	}
	int m = 0;
	while (m < s->project.media_count && strcmp(s->project.media[m].path, r->media.path))
		m++;
	if (m == s->project.media_count)
	{
		if (m == ST_MEDIA)
			return;
		s->project.media[m] = r->media;
		s->project.media_count++;
		st_history_push(&s->history, &s->project);
		s->dirty = 1;
	}
	int k = 0;
	while (k < s->thumb_count && strcmp(s->thumbs[k].path, r->media.path))
		k++;
	if (k == s->thumb_count && s->thumb_count < ST_MEDIA)
		s->thumb_count++;
	if (k < ST_MEDIA)
	{
		StThumb* t = &s->thumbs[k];
		strcpy(t->path, r->media.path);
		memcpy(t->pixels, r->thumb, sizeof(t->pixels));
		t->image = (GYimg){.data = t->pixels, .w = 160, .h = 90};
	}
	s->selected_media = m;
	studio_changed(s);
	studio_status(s, "素材已就绪 · 拖到轨道，或点击“追加到轨道”");
	if (!st_duration(&s->project))
		studio_mode(s, 1);
}
void studio_tick(Studio* s)
{
	for (int m = 0; m < s->project.media_count; m++)
	{
		int state = st_engine_proxy_state(s->engine, s->project.media[m].path);
		if (state != s->proxy_states[m])
		{
			s->proxy_states[m] = state;
			YMGUI_Obj_Invalidate(s->bin);
			if (state == 2)
			{
				studio_status(s, "预览代理已就绪 · 原素材保持不变");
				studio_request(s, 1);
			}
			else if (state == 1)
				studio_status(s, "正在准备快速预览 · 可继续剪辑");
			else if (state == -1)
				studio_status(s, "代理生成失败，继续使用原素材预览");
		}
	}
	StImportResult r;
	while (st_engine_poll_import(s->engine, &r))
		imported(s, &r);
	if (YMGUI_State_GetBool(&s->playing))
	{
		int frame = s->anchor_frame + (int)((uint64_t)(SDL_GetTicks() - s->anchor_tick) * ST_FPS / 1000), end = studio_duration(s);
		if (frame >= end)
		{
			frame = end > 0 ? end - 1 : 0;
			YMGUI_State_SetBool(&s->playing, 0);
		}
		s->internal_change = 1;
		YMGUI_State_SetInt(YMGUI_State_GetInt(&s->mode) ? &s->source_position : &s->position, frame);
		s->internal_change = 0;
	}
	if (s->request_dirty && SDL_GetTicks() - s->request_tick >= 30)
		studio_request(s, 0);
	StFrameResult frame;
	if (st_engine_poll_frame(s->engine, s->incoming, &frame) && frame.serial >= s->min_serial)
	{
		memcpy(s->pixels, s->incoming, sizeof(s->pixels));
		s->shown_serial = frame.serial;
		s->last_decode_ms = frame.elapsed_ms;
		s->preview_ok = frame.ok;
		YMGUI_Obj_Invalidate(s->preview);
		if (!frame.ok)
		{
			char message[200];
			snprintf(message, sizeof(message), "预览失败：%s", frame.error);
			studio_status(s, message);
		}
	}
	studio_thumbnail_tick(s);
	studio_export_poll(s);
	YMGUI_Refresh(s->ctx);
}
static int desktop_event(void* user, SDL_Event* event)
{
	Studio* s = user;
	if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
	{
		s->drag_kind = 0;
		s->ghost_track = -1;
		s->drag_moved = 0;
		s->scrubbing = 0;
		s->shift = 0;
		s->alt = 0;
		YMGUI_Obj_Invalidate(s->timeline);
	}
	if (event->type != SDL_KEYDOWN)
		return 1;
	if (event->key.keysym.sym == SDLK_ESCAPE)
	{
		s->drag_kind = 0;
		s->ghost_track = -1;
		s->drag_moved = 0;
		s->scrubbing = 0;
		YMGUI_Inject_PointerCancel();
		YMGUI_Obj_Invalidate(s->timeline);
		if (YMGUI_MsgBox_IsShown(s->msgbox))
			YMGUI_MsgBox_Close(s->msgbox);
		else if (YMGUI_FileDialog_IsShown(s->dialog))
			YMGUI_FileDialog_Close(s->dialog);
		return 0;
	}
	if ((event->key.keysym.mod & KMOD_CTRL) && !event->key.repeat && !YMGUI_MsgBox_IsShown(s->msgbox) && !YMGUI_FileDialog_IsShown(s->dialog))
	{
		if (event->key.keysym.sym == SDLK_s)
		{
			studio_action(s, ACT_SAVE);
			return 0;
		}
		if (event->key.keysym.sym == SDLK_o)
		{
			studio_action(s, ACT_OPEN);
			return 0;
		}
	}
	return 1;
}
int studio_pump(Studio* s)
{
	SDL_PumpEvents();
	SDL_FilterEvents(desktop_event, s);
	SDL_Event e;
	while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_DROPFILE, SDL_DROPFILE) > 0)
	{
		studio_import(s, e.drop.file);
		SDL_free(e.drop.file);
	}
	return !s->quit && SDL_LCD_PumpEvents();
}
