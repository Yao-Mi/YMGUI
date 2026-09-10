/* Copyright (c) 2026. YMGUI video editing workspace. */
#include "app/studio.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Bar.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Font.h"
#include "YMGUI_DrawFill.h"
#include <stdio.h>
#include <string.h>
GYOBJ studio_panel(GYOBJ p, int x, int y, int w, int h, unsigned color)
{
	GYOBJ o = YMGUI_Creat_Obj_Creat(p, x, y, w, h);
	YMGUI_Obj_SetBgColor(o, ST_RGB(color));
	o->state |= GY_STATE_ClipChildren;
	return o;
}
GYOBJ studio_label(GYOBJ p, int x, int y, int w, int h, const char* text, unsigned color)
{
	GYOBJ o = YMGUI_Creat_Label_Creat(p, x, y, w, h);
	YMGUI_Label_SetText(o, text);
	YMGUI_Label_SetTextColor(o, ST_RGB(color));
	return o;
}
static void clicked(GYOBJ obj)
{
	Studio* s = studio_of(obj);
	for (int i = 0; i < s->action_count; i++)
		if (s->action_buttons[i] == obj)
		{
			studio_action(s, s->action_ids[i]);
			return;
		}
}
GYOBJ studio_button(GYOBJ p, int x, int y, int w, int h, const char* text, StAction action)
{
	Studio* s = studio_of(p);
	GYOBJ o = YMGUI_Creat_Button_Creat(p, x, y, w, h);
	YMGUI_Button_SetText(o, text);
	YMGUI_Button_SetColors(o, ST_RGB(0x35383e), ST_RGB(0x526272));
	YMGUI_Button_SetClicked(o, clicked);
	if (s->action_count < 64)
	{
		s->action_buttons[s->action_count] = o;
		s->action_ids[s->action_count++] = action;
	}
	return o;
}
static void info_line(GYSURFACE surf, int x, int y, const char* name, const char* value)
{
	YMGUI_Draw_Text(surf, &YMGUI_Font_Default, x, y, name, ST_RGB(0x9da3ad));
	YMGUI_Draw_Text(surf, &YMGUI_Font_Default, x, y + 22, value, ST_RGB(0xe2e5e9));
}
static void properties_draw(GYOBJ o, GYSURFACE surf, const GYrect* a)
{
	Studio* s = studio_of(o);
	GYsurface clipped = *surf;
	clipped.clip.x = a->x;
	clipped.clip.w = a->w; /* Draw primitives also clip to their band. */
	surf = &clipped;
	YMGUI_Draw_Fill(surf, a, ST_RGB(0x292c31), GY_OPA_COVER);
	int track = -1;
	const StClip* c = st_clip(&s->project, s->selected_clip, &track);
	int m = c ? c->media : s->selected_media;
	if (m < 0 || m >= s->project.media_count)
	{
		info_line(surf, a->x + 14, a->y + 16, "未选择素材", "在素材库或时间线上选择片段");
		return;
	}
	StMedia* media = &s->project.media[m];
	char buffer[96];
	info_line(surf, a->x + 14, a->y + 16, c ? "所选片段" : "源素材", st_basename(media->path));
	snprintf(buffer, sizeof(buffer), "%d x %d", media->width, media->height);
	info_line(surf, a->x + 14, a->y + 78, "视频尺寸", buffer);
	studio_timecode(buffer, sizeof(buffer), c ? c->length : media->frames);
	info_line(surf, a->x + 14, a->y + 138, "时长", buffer);
	if (c)
	{
		char in[32], out[32];
		studio_timecode(in, sizeof(in), c->in);
		studio_timecode(out, sizeof(out), c->in + c->length - 1);
		snprintf(buffer, sizeof(buffer), "%s - %s", in, out);
		info_line(surf, a->x + 14, a->y + 198, "源入点 / 出点", buffer);
		snprintf(buffer, sizeof(buffer), "V%d   #%d", track + 1, c->id);
		info_line(surf, a->x + 14, a->y + 258, "轨道 / 片段", buffer);
	}
	else
		info_line(surf, a->x + 14, a->y + 198, "项目帧率", "25 fps");
}
static void monitor_seek_event(GYOBJ object, GYEvent event)
{
	Studio* s = studio_of(object);
	if (s->seek_event)
		s->seek_event(object, event);
	if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
		studio_request(s, 1);
}
void studio_ui_build(Studio* s)
{
	GYOBJ root = s->ctx->root;
	YMGUI_Obj_SetBgColor(root, ST_RGB(0x202226));
	GYOBJ menu = studio_panel(root, 0, 0, ST_W, 34, 0x24262b);
	studio_label(menu, 14, 8, 150, 20, "video_stidio", 0xe8ecf0);
	studio_button(menu, 174, 3, 100, 28, "打开项目", ACT_OPEN);
	studio_button(menu, 280, 3, 88, 28, "保存项目", ACT_SAVE);
	s->title = studio_label(menu, 390, 8, ST_W - 570, 22, "未命名项目", 0xbac0c8);
	studio_label(menu, ST_W - 152, 8, 142, 20, "YMGUI / 25 fps", 0x949da8);
	GYOBJ tools = studio_panel(root, 0, 35, ST_W, 42, 0x2d3035);
	studio_button(tools, 10, 6, 100, 30, "打开视频", ACT_IMPORT);
	s->undo = studio_button(tools, 120, 6, 80, 30, "撤销", ACT_UNDO);
	s->redo = studio_button(tools, 208, 6, 80, 30, "重做", ACT_REDO);
	studio_button(tools, 310, 6, 118, 30, "追加到轨道", ACT_APPEND);
	studio_button(tools, 440, 6, 100, 30, "分割  S", ACT_SPLIT);
	studio_button(tools, 550, 6, 108, 30, "移除片段", ACT_LIFT);
	studio_button(tools, 668, 6, 108, 30, "波纹删除", ACT_RIPPLE);
	s->export_button = studio_button(tools, 794, 6, 110, 30, "导出 MP4", ACT_EXPORT);
	s->export_cancel = studio_button(tools, 912, 6, 72, 30, "取消", ACT_EXPORT_CANCEL);
	s->export_bar = YMGUI_Creat_Bar_Creat(tools, 998, 24, ST_W - 1142, 7);
	YMGUI_Bar_SetRange(s->export_bar, 0, 1000);
	YMGUI_Bar_SetColors(s->export_bar, ST_RGB(0x1c2026), ST_RGB(0x72bf9c));
	s->export_label = studio_label(tools, ST_W - 146, 13, 138, 20, "720p · 无音频", 0xaeb6bf);

	GYOBJ left = studio_panel(root, 0, 80, ST_LEFT - 3, ST_WORK_H, 0x292c31);
	studio_label(left, 12, 8, 210, 22, "播放列表 / 素材库", 0xe1e5ea);
	studio_label(left, 12, 36, 236, 20, "拖放素材到时间线 · 双击预览", 0x939da7);
	s->bin = studio_panel(left, 4, 62, ST_LEFT - 11, ST_BIN_ROWS * 65, 0x25282d);
	s->bin->draw_cb = studio_bin_draw;
	s->bin->event_cb = studio_bin_event;
	studio_button(left, 12, ST_WORK_H - 45, 108, 30, "添加素材", ACT_IMPORT);
	studio_button(left, ST_LEFT - 136, ST_WORK_H - 45, 118, 30, "追加到轨道", ACT_APPEND);

	GYOBJ center = studio_panel(root, ST_LEFT, 80, ST_RIGHT - ST_LEFT - 3, ST_WORK_H, 0x1e2024);
	studio_label(center, 12, 8, 160, 22, "播放器", 0xdce0e6);
	s->source_tab = studio_button(center, ST_RIGHT - ST_LEFT - 215, 3, 92, 28, "源素材", ACT_SOURCE);
	s->project_tab = studio_button(center, ST_RIGHT - ST_LEFT - 115, 3, 92, 28, "项目", ACT_PROJECT);
	s->preview = YMGUI_Creat_Image_Creat(center, 12, 38, ST_RIGHT - ST_LEFT - 28, ST_WORK_H - 90);
	YMGUI_Obj_SetBgColor(s->preview, ST_RGB(0x050505));
	YMGUI_Image_SetScaleMode(s->preview, GY_IMG_FIT);
	YMGUI_Image_SetSrc(s->preview, &s->preview_image);
	s->seek = YMGUI_Creat_Slider_Creat(center, 18, ST_WORK_H - 48, ST_RIGHT - ST_LEFT - 40, 14);
	YMGUI_Slider_SetRange(s->seek, 0, 0);
	s->seek_event = s->seek->event_cb;
	s->seek->event_cb = monitor_seek_event;
	s->timecode = studio_label(center, 20, ST_WORK_H - 24, 340, 22, "00:00:00:00 / 00:00:00:00", 0xe8edf3);
	studio_button(center, ST_RIGHT - ST_LEFT - 304, ST_WORK_H - 30, 44, 28, "|<", ACT_HOME);
	studio_button(center, ST_RIGHT - ST_LEFT - 253, ST_WORK_H - 30, 44, 28, "<", ACT_PREV);
	s->play = studio_button(center, ST_RIGHT - ST_LEFT - 202, ST_WORK_H - 30, 74, 28, "播放", ACT_PLAY);
	studio_button(center, ST_RIGHT - ST_LEFT - 121, ST_WORK_H - 30, 44, 28, ">", ACT_NEXT);
	studio_button(center, ST_RIGHT - ST_LEFT - 70, ST_WORK_H - 30, 44, 28, ">|", ACT_END);

	GYOBJ right = studio_panel(root, ST_RIGHT, 80, ST_W - ST_RIGHT, ST_WORK_H, 0x292c31);
	studio_label(right, 12, 8, 180, 22, "属性", 0xe1e5ea);
	s->properties = studio_panel(right, 0, 34, ST_W - ST_RIGHT, ST_WORK_H - 112, 0x292c31);
	s->properties->draw_cb = properties_draw;
	studio_button(right, 12, ST_WORK_H - 72, 108, 30, "设为入点 I", ACT_IN);
	studio_button(right, ST_W - ST_RIGHT - 120, ST_WORK_H - 72, 108, 30, "设为出点 O", ACT_OUT);
	studio_label(right, 12, ST_WORK_H - 28, 226, 20, "拖动片段两端可精确修剪", 0x939da7);

	GYOBJ tltools = studio_panel(root, 0, ST_TL_Y - 39, ST_W, 36, 0x303339);
	studio_label(tltools, 12, 9, 84, 22, "时间线", 0xe8edf3);
	studio_button(tltools, 98, 4, 96, 28, "增加视频轨", ACT_TRACK);
	studio_label(tltools, 216, 9, 622, 20, "拖动移动 / 跨轨 · 两端修剪 · Alt 暂停吸附", 0xaeb6bf);
	studio_button(tltools, ST_W - 264, 4, 44, 28, "-", ACT_ZOOM_OUT);
	studio_button(tltools, ST_W - 212, 4, 44, 28, "+", ACT_ZOOM_IN);
	studio_button(tltools, ST_W - 160, 4, 144, 28, "适配整个项目", ACT_FIT);
	s->timeline = studio_panel(root, 0, ST_TL_Y, ST_W, ST_TL_H, 0x24272b);
	s->timeline->draw_cb = studio_timeline_draw;
	s->timeline->event_cb = studio_timeline_event;
	s->head = YMGUI_Creat_Obj_Creat(s->timeline, ST_HEADER - 5, 0, 11, ST_TL_H);
	s->head->event_cb = studio_timeline_event;
	s->scrollbar = YMGUI_Creat_Slider_Creat(root, ST_HEADER, ST_H - 40, ST_W - ST_HEADER - 12, 14);
	YMGUI_Slider_SetRange(s->scrollbar, 0, ST_FPS * 60);
	studio_label(root, 10, ST_H - 40, 142, 18, "V 轨道 · 滚轮滚动", 0x959da8);
	GYOBJ status = studio_label(root, 10, ST_H - 19, ST_W - 20, 18, "", 0x99a6b4);
	YMGUI_Label_Bind(status, &s->status);
}
void studio_ui_refresh(Studio* s)
{
	char title[1100];
	snprintf(title, sizeof(title), "%s%s", s->project_path[0] ? st_basename(s->project_path) : "未命名项目", s->dirty ? " *" : "");
	YMGUI_Label_SetText(s->title, title);
	YMGUI_Button_SetColors(s->source_tab, ST_RGB(YMGUI_State_GetInt(&s->mode) ? 0x225e79 : 0x35383e), ST_RGB(0x527388));
	YMGUI_Button_SetColors(s->project_tab, ST_RGB(!YMGUI_State_GetInt(&s->mode) ? 0x225e79 : 0x35383e), ST_RGB(0x527388));
	YMGUI_Button_SetColors(s->undo, ST_RGB(s->history.cursor ? 0x3c4650 : 0x282b30), ST_RGB(0x526272));
	YMGUI_Button_SetColors(s->redo, ST_RGB(s->history.cursor + 1 < s->history.count ? 0x3c4650 : 0x282b30), ST_RGB(0x526272));
	int duration = studio_duration(s);
	YMGUI_Slider_SetRange(s->seek, 0, duration > 0 ? duration - 1 : 0);
	int end = st_duration(&s->project) + ST_FPS * 10;
	YMGUI_Slider_SetRange(s->scrollbar, 0, end);
	int maximum = s->project.track_count - ST_VISIBLE_TRACKS;
	if (maximum < 0)
		maximum = 0;
	if (s->track_scroll > maximum)
		s->track_scroll = maximum;
	YMGUI_Obj_Invalidate(s->timeline);
	YMGUI_Obj_Invalidate(s->bin);
	YMGUI_Obj_Invalidate(s->properties);
	s->internal_change = 2;
	YMGUI_State_Touch(&s->position);
	YMGUI_State_Touch(&s->source_position);
	s->internal_change = 0;
}
#ifndef GB2312_BIN_PATH
#define GB2312_BIN_PATH "gb2312_glyphs.bin"
#endif
extern const uint16 YMGUI_GB2312_cps[], YMGUI_GB2312_glyph_count;
static FILE* font_file;
static uint32 read_glyph(const GYfont* f, uint32 offset, uint32 length, uint8* buffer)
{
	(void)f;
	if (!font_file || fseek(font_file, offset, SEEK_SET))
		return 0;
	return fread(buffer, 1, length, font_file);
}
static GYfont chinese = {NULL, YMGUI_GB2312_cps, 0, 0, 0, 16, 16, 8, 4, NULL, read_glyph};
void studio_font_open(void)
{
	font_file = fopen(GB2312_BIN_PATH, "rb");
	if (font_file)
	{
		chinese.glyph_count = YMGUI_GB2312_glyph_count;
		YMGUI_Font_SetFallback(&chinese);
	}
}
void studio_font_close(void)
{
	YMGUI_Font_SetFallback(NULL);
	if (font_file)
		fclose(font_file);
	font_file = NULL;
}
