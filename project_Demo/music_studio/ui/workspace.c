#include "app/studio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

int ms_clamp(int v, int lo, int hi)
{
	return v < lo ? lo : v > hi ? hi
								: v;
}
void ms_fill(GYSURFACE s, int x, int y, int w, int h, unsigned color)
{
	if (w <= 0 || h <= 0)
		return;
	/* 放大/滚动后的时间坐标可能超出 16 位，先裁剪再转为 GYcoord。 */
	int right = x + w, bottom = y + h;
	if (x < s->clip.x)
		x = s->clip.x;
	if (y < s->clip.y)
		y = s->clip.y;
	if (right > s->clip.x + s->clip.w)
		right = s->clip.x + s->clip.w;
	if (bottom > s->clip.y + s->clip.h)
		bottom = s->clip.y + s->clip.h;
	if (right <= x || bottom <= y)
		return;
	GYrect r = {(GYcoord)x, (GYcoord)y, (GYcoord)(right - x), (GYcoord)(bottom - y)};
	YMGUI_Draw_Fill(s, &r, MS_RGB(color), GY_OPA_COVER);
}
void ms_text(GYSURFACE s, int x, int y, const char* text, unsigned color)
{
	YMGUI_Draw_Text(s, &YMGUI_Font_Default, x, y, text, MS_RGB(color));
}
void ms_border(GYSURFACE s, int x, int y, int w, int h, unsigned c)
{
	ms_fill(s, x, y, w, 1, c);
	ms_fill(s, x, y + h - 1, w, 1, c);
	ms_fill(s, x, y, 1, h, c);
	ms_fill(s, x + w - 1, y, 1, h, c);
}
GYOBJ ms_panel(MsApp* a, int x, int y, int w, int h, GYobj_draw_cb draw, GYobj_event_cb event)
{
	GYOBJ o = YMGUI_Creat_Obj_Creat(a->ctx->root, x, y, w, h);
	if (!o)
		return NULL;
	o->bg_color = MS_RGB(MS_PANEL);
	o->state |= GY_STATE_ClipChildren | GY_STATE_Focusable;
	o->user_data = a;
	if (draw)
		o->draw_cb = draw;
	o->event_cb = event;
	return o;
}
GYOBJ ms_label(MsApp* a, int x, int y, int w, const char* text, unsigned color)
{
	GYOBJ o = YMGUI_Creat_Label_Creat(a->ctx->root, x, y, w, 22);
	YMGUI_Label_SetText(o, text);
	YMGUI_Label_SetTextColor(o, MS_RGB(color));
	return o;
}
static MsApp* current;
static void button_click(GYOBJ o)
{
	for (int i = 0; i < current->button_count; ++i)
		if (current->buttons[i] == o)
		{
			ms_action(current, current->actions[i]);
			return;
		}
}
GYOBJ ms_button(MsApp* a, int x, int y, int w, const char* text, MsAction action)
{
	GYOBJ o = YMGUI_Creat_Button_Creat(a->ctx->root, x, y, w, 28);
	YMGUI_Button_SetText(o, text);
	YMGUI_Button_SetColors(o, MS_RGB(0x393f46), MS_RGB(0x53655f));
	YMGUI_Button_SetClicked(o, button_click);
	if (a->button_count < 96)
	{
		a->buttons[a->button_count] = o;
		a->actions[a->button_count++] = action;
	}
	return o;
}
static void select_instrument(GYOBJ o, uint16 selected)
{
	(void)o;
	MsApp* a = current;
	if (a->syncing)
		return;
	if (a->worker || a->track < 0 || a->track >= a->project.song.tracks || a->project.song.track[a->track].kind != MS_SYNTH)
	{
		ms_ui_refresh(a);
		return;
	}
	if (selected >= MS_INSTRUMENTS || selected == a->project.song.track[a->track].instrument)
		return;
	ms_checkpoint(a);
	a->project.song.track[a->track].instrument = selected;
	ms_changed(a);
	ms_status(a, "已更换当前乐器轨音色；点击琴键或试听片段即可听效果");
}
static void select_piano_grid(GYOBJ o, uint16 selected)
{
	(void)o;
	MsApp* a = current;
	if (a->syncing)
		return;
	if (a->worker || a->drag || selected > 6)
	{
		ms_ui_refresh(a);
		return;
	}
	a->note_grid = selected;
	a->note_length = ms_piano_grid_ticks(selected);
	ms_ui_refresh(a);
	ms_status(a, "已设置新音符音长与编辑步长；关闭吸附可细调，已有音符保持原长度");
}
int ms_instrument_popup_open(MsApp* a)
{
	return YMGUI_Dropdown_IsOpen(a->instrument) || YMGUI_Dropdown_IsOpen(a->piano_grid);
}
static void submit(GYOBJ o, const char* text)
{
	MsApp* a = current;
	if (a->syncing || a->worker)
		return;
	if (o == a->fade_in || o == a->fade_out)
	{
		if (a->clip < 0 || a->clip >= a->project.song.clips)
		{
			ms_status(a, "先将素材加入轨道并选择片段，再设置淡入淡出");
			ms_ui_refresh(a);
			return;
		}
		char* end;
		double seconds = strtod(text, &end);
		if (end == text || *end || !isfinite(seconds) || seconds < 0 || seconds > 10)
		{
			ms_status(a, "淡入淡出范围为 0 到 10 秒；填 0 关闭，回车确认");
			ms_ui_refresh(a);
			return;
		}
		MsClip* clip = &a->project.song.clip[a->clip];
		float* duration = o == a->fade_in ? &clip->fade_in : &clip->fade_out;
		if (*duration != (float)seconds)
		{
			ms_checkpoint(a);
			*duration = (float)seconds;
			ms_changed(a);
		}
		ms_status(a, "已设置片段淡化；整曲播放、片段试听和导出均生效");
		return;
	}
	ms_checkpoint(a);
	if (o == a->bpm_input)
	{
		char* end;
		long bpm = strtol(text, &end, 10);
		if (*end || bpm < 30 || bpm > 300)
		{
			ms_status(a, "速度范围为每分钟 30 至 300 拍");
			ms_ui_refresh(a);
			return;
		}
		double tick = ms_frame_tick(&a->project.song, ms_transport_position(&a->transport));
		a->project.song.bpm = (int)bpm;
		ms_transport_seek(&a->transport, (int64_t)ms_tick_frame(&a->project.song, tick));
	}
	else if (o == a->track_name && a->track >= 0 && a->track < a->project.song.tracks)
		snprintf(a->project.song.track[a->track].name, MS_NAME, "%s", text);
	else if (o == a->pattern_name && a->pattern >= 0 && a->pattern < a->project.song.patterns)
		snprintf(a->project.song.pattern[a->pattern].name, MS_NAME, "%s", text);
	ms_changed(a);
}
static void slider(GYOBJ o, int32 value)
{
	MsApp* a = current;
	if (a->syncing || a->worker)
		return;
	MsSong* s = &a->project.song;
	if (o == a->velocity)
	{
		a->note_velocity = value;
		if (a->pattern >= 0 && a->pattern < s->patterns)
		{
			MsPattern* p = &s->pattern[a->pattern];
			if (a->tab == 0 && a->step >= 0 && p->kind == MS_DRUM)
				p->drum[a->drum][a->step] = value;
			if (a->tab == 1 && p->kind == MS_SYNTH)
				for (int i = 0; i < p->count; ++i)
					if (ms_note_selected(a, i))
						p->notes[i].velocity = value;
		}
	}
	else if (o == a->drum_pan_slider)
		s->drum_pan[a->drum] = value / 100.f;
	else if (o == a->master)
		s->master = value / 100.f;
	else if (o == a->clip_gain && a->clip >= 0 && a->clip < s->clips)
		s->clip[a->clip].gain = value / 100.f;
	else if (a->track >= 0 && a->track < s->tracks)
	{
		if (o == a->volume)
			s->track[a->track].volume = value / 100.f;
		if (o == a->pan)
			s->track[a->track].pan = value / 100.f;
	}
	a->dirty = 1;
	ms_transport_seek(&a->transport, ms_transport_position(&a->transport));
	YMGUI_Obj_Invalidate(a->editor);
	YMGUI_Obj_Invalidate(a->timeline);
}
static GYobj_event_cb slider_original;
static void slider_event(GYOBJ o, GYEvent event)
{
	MsApp* a = current;
	if (a->worker)
		return;
	if (event == GY_EVENT_Pressed)
		ms_checkpoint(a);
	if (slider_original)
		slider_original(o, event);
	if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
		ms_ui_refresh(a);
}
static GYOBJ make_slider(MsApp* a, int x, int y, int w, int lo, int hi)
{
	GYOBJ o = YMGUI_Creat_Slider_Creat(a->ctx->root, x, y, w, 22);
	YMGUI_Slider_SetRange(o, lo, hi);
	YMGUI_Slider_SetChanged(o, slider);
	slider_original = o->event_cb;
	o->event_cb = slider_event;
	return o;
}
static void detail_draw(GYOBJ o, GYSURFACE surf, const GYrect* r)
{
	MsApp* a = o->user_data;
	MsSong* s = &a->project.song;
	char b[96];
	ms_fill(surf, r->x, r->y, r->w, r->h, MS_PANEL);
	ms_text(surf, r->x + 12, r->y + 10, "轨道属性", MS_ACCENT);
	ms_text(surf, r->x + 12, r->y + 46, "轨道名称（回车确认）", MS_DIM);
	float vol = a->track >= 0 && a->track < s->tracks ? s->track[a->track].volume : 0;
	float pan = a->track >= 0 && a->track < s->tracks ? s->track[a->track].pan : 0;
	snprintf(b, sizeof(b), "音量  %d%%", (int)(vol * 100));
	ms_text(surf, r->x + 12, r->y + 114, b, MS_TEXT);
	snprintf(b, sizeof(b), "声像  %s %d", pan < 0 ? "左" : pan > 0 ? "右"
																   : "中",
			 (int)fabsf(pan * 100));
	ms_text(surf, r->x + 12, r->y + 174, b, MS_TEXT);
	ms_text(surf, r->x + 12, r->y + 232, "片段增益 / 淡化", MS_DIM);
	if (a->clip >= 0)
	{
		ms_text(surf, r->x + 12, r->y + 290, "淡入(秒)", MS_DIM);
		ms_text(surf, r->x + 102, r->y + 290, "淡出(秒)", MS_DIM);
	}
	else
		ms_text(surf, r->x + 12, r->y + 266, "先选中编排片段", MS_DIM);
	ms_text(surf, r->x + 12, r->y + 356, "主输出", MS_ACCENT);
	snprintf(b, sizeof(b), "%d%%", (int)(s->master * 100));
	ms_text(surf, r->x + 130, r->y + 356, b, MS_TEXT);
}
void ms_ui_build(MsApp* a)
{
	current = a;
	YMGUI_Obj_SetBgColor(a->ctx->root, MS_RGB(MS_BG));
	ms_panel(a, 0, 0, MS_W, 40, NULL, NULL);
	ms_label(a, 16, 10, 150, "音乐工坊", MS_ACCENT);
	MsAction menu[] = {A_NEW, A_OPEN, A_SAVE, A_SAVE_AS, A_IMPORT, A_EXPORT, A_EXPORT_LOOP, A_UNDO, A_REDO, A_HELP};
	const char* names[] = {"新建", "打开", "保存", "另存为", "导入音频", "导出整曲", "导出循环", "撤销", "重做", "帮助"};
	int x = 172;
	for (int i = 0; i < 10; ++i)
	{
		int w = i >= 4 && i <= 6 ? 92 : 68;
		ms_button(a, x, 6, w, names[i], menu[i]);
		x += w + 6;
	}
	a->title = ms_label(a, 1070, 10, MS_W - 1082, "", MS_DIM);
	ms_panel(a, 0, 42, MS_W, 60, NULL, NULL);
	ms_button(a, 14, 57, 82, "播放", A_PLAY);
	ms_button(a, 104, 57, 70, "停止", A_STOP);
	ms_button(a, 182, 57, 78, "循环", A_LOOP);
	ms_button(a, 268, 57, 86, "节拍器", A_METRO);
	ms_button(a, 366, 57, 112, "整曲播放", A_MODE);
	ms_label(a, 498, 64, 44, "速度", MS_DIM);
	a->bpm_input = YMGUI_Creat_TextInput_Creat(a->ctx->root, 544, 57, 64, 28, 3);
	YMGUI_TextInput_SetSubmitted(a->bpm_input, submit);
	ms_button(a, 620, 57, 100, "拍号 4/4", A_BEATS);
	a->clock_label = ms_label(a, 754, 64, 390, "", MS_ACCENT);
	ms_label(a, 1160, 64, 268, "48 千赫兹 · 立体声", MS_DIM);
	ms_button(a, 12, 114, 97, "节奏与旋律", A_LIBRARY_PAT);
	ms_button(a, 117, 114, 97, "音频素材", A_LIBRARY_AUDIO);
	a->library = ms_panel(a, 12, 150, 202, 322, ms_library_draw, ms_library_event);
	MsAction tools[] = {A_ADD_DRUM, A_ADD_SYNTH, A_ADD_AUDIO, A_DEL_TRACK, A_UP, A_DOWN, A_REPEAT, A_SPLIT, A_DELETE, A_ZOOM_IN, A_ZOOM_OUT, A_SNAP};
	const char* labels[] = {"加鼓轨", "加乐器", "加音轨", "删轨道", "上移", "下移", "重复片段", "分割", "删片段", "放大", "缩小", "吸附"};
	x = 232;
	for (int i = 0; i < 12; ++i)
	{
		int w = i == 6 ? 94 : i == 4 || i == 5 || i == 7 || i >= 9 ? 64
																   : 80;
		ms_button(a, x, 114, w, labels[i], tools[i]);
		x += w + 6;
	}
	a->timeline = ms_panel(a, 232, 150, MS_W - 244, 322, ms_timeline_draw, ms_timeline_event);
	ms_button(a, 232, 478, 88, "循环起点", A_LOOP_START);
	ms_button(a, 326, 478, 88, "循环终点", A_LOOP_END);
	ms_label(a, 430, 484, 990, "拖动片段编排 · 拖动两端裁剪 · 点击标尺定位 · 滚轮横移 · 双击片段编辑", MS_DIM);
	a->detail = ms_panel(a, 12, 486, 202, MS_H - 526, detail_draw, NULL);
	a->track_name = YMGUI_Creat_TextInput_Creat(a->ctx->root, 24, 560, 178, 28, MS_NAME - 1);
	YMGUI_TextInput_SetSubmitted(a->track_name, submit);
	a->volume = make_slider(a, 24, 628, 178, 0, 150);
	a->pan = make_slider(a, 24, 686, 178, -100, 100);
	a->clip_gain = make_slider(a, 24, 750, 178, 0, 200);
	a->fade_in = YMGUI_Creat_TextInput_Creat(a->ctx->root, 24, 800, 82, 26, 6);
	a->fade_out = YMGUI_Creat_TextInput_Creat(a->ctx->root, 114, 800, 82, 26, 6);
	YMGUI_TextInput_SetSubmitted(a->fade_in, submit);
	YMGUI_TextInput_SetSubmitted(a->fade_out, submit);
	a->master = make_slider(a, 24, 874, 178, 0, 150);
	MsAction tabs[] = {A_DRUM, A_PIANO, A_MIXER};
	const char* tabnames[] = {"鼓机", "钢琴卷帘", "混音器"};
	for (int i = 0; i < 3; ++i)
		ms_button(a, 232 + i * 108, 516, 100, tabnames[i], tabs[i]);
	ms_button(a, 568, 516, 86, "新建片段", A_PAT_NEW);
	ms_button(a, 660, 516, 86, "复制片段", A_PAT_COPY);
	ms_button(a, 752, 516, 86, "独立副本", A_PAT_UNIQUE);
	ms_button(a, 844, 516, 86, "16 / 32步", A_PAT_LENGTH);
	ms_button(a, 936, 516, 72, "清空", A_PAT_CLEAR);
	ms_button(a, 1014, 516, 94, "加入编排", A_APPEND);
	a->pattern_name = YMGUI_Creat_TextInput_Creat(a->ctx->root, 1120, 516, MS_W - 1132, 28, MS_NAME - 1);
	YMGUI_TextInput_SetSubmitted(a->pattern_name, submit);
	ms_button(a, 232, 558, 88, "更换鼓声", A_SAMPLE);
	ms_button(a, 326, 558, 88, "内置鼓声", A_SAMPLE_RESET);
	ms_button(a, 420, 558, 64, "静音", A_DRUM_MUTE);
	ms_button(a, 490, 558, 64, "独奏", A_DRUM_SOLO);
	a->instrument_label = ms_label(a, 232, 565, 46, "音色", MS_DIM);
	a->instrument = YMGUI_Creat_Dropdown_Creat(a->ctx->root, 284, 558, 270, 28);
	for (int i = 0; i < MS_INSTRUMENTS; ++i)
		YMGUI_Dropdown_AddOption(a->instrument, ms_instrument_names[i]);
	YMGUI_Dropdown_SetSelectedCb(a->instrument, select_instrument);
	a->sound_mode = ms_label(a, 1444, 565, 224, "全部使用采样音源", MS_DIM);
	ms_button(a, 564, 558, 88, "原声试听", A_INSTRUMENT);
	ms_button(a, 658, 558, 64, "音长减", A_NOTE_SHORT);
	ms_button(a, 728, 558, 64, "音长加", A_NOTE_LONG);
	ms_button(a, 802, 558, 80, "低八度", A_OCT_DOWN);
	ms_button(a, 888, 558, 80, "高八度", A_OCT_UP);
	a->drum_pan_label = ms_label(a, 566, 565, 130, "鼓声声像", MS_DIM);
	a->drum_pan_slider = make_slider(a, 704, 562, 240, -100, 100);
	a->velocity_label = ms_label(a, 988, 565, 46, "力度", MS_DIM);
	a->velocity = make_slider(a, 1036, 562, 196, 1, 127);
	ms_button(a, 1250, 558, 80, "量化", A_QUANTIZE);
	ms_button(a, 1336, 558, 92, "试听片段", A_PREVIEW);
	a->editor = ms_panel(a, 232, 598, MS_W - 244, MS_H - 638, ms_editor_draw, ms_editor_event);
	ms_button(a, 1396, 600, 132, "框选已有", A_NOTE_SELECT);
	ms_button(a, 1536, 600, 120, "删除所选", A_NOTE_DELETE);
	a->piano_grid = YMGUI_Creat_Dropdown_Creat(a->ctx->root, 1444, 558, 224, 28);
	static const char* grids[] = {"步长 1/16", "步长 1/32", "步长 1/64", "1/8 三连音", "1/16 三连音", "1/32 三连音", "自由（最小音长）"};
	for (int i = 0; i < 7; ++i)
		YMGUI_Dropdown_AddOption(a->piano_grid, grids[i]);
	YMGUI_Dropdown_SetSelectedCb(a->piano_grid, select_piano_grid);
	ms_panel(a, 0, MS_H - 28, MS_W, 28, NULL, NULL);
	a->status = ms_label(a, 14, MS_H - 22, MS_W - 28, "", MS_DIM);
	a->dialog = YMGUI_Creat_FileDialog_Creat(a->ctx, 860, 580, MS_PATH - 1, 255, 512);
	a->msgbox = YMGUI_Creat_MsgBox_Creat(a->ctx);
	a->drag_ghost = YMGUI_Creat_Obj_Creat(YMGUI_Ctx_GetTopLayer(a->ctx), 0, 0, 230, 48);
	if (a->drag_ghost)
	{
		a->drag_ghost->user_data = a;
		a->drag_ghost->draw_cb = ms_drag_ghost_draw;
		a->drag_ghost->state |= GY_STATE_ClipChildren;
		YMGUI_Obj_SetHidden(a->drag_ghost, 1);
	}
	ms_ui_refresh(a);
}
void ms_status(MsApp* a, const char* text)
{
	if (a->status)
		YMGUI_Label_SetText(a->status, text);
}
void ms_ui_refresh(MsApp* a)
{
	if (!a->ctx)
		return;
	MsSong* s = &a->project.song;
	a->track = ms_clamp(a->track, 0, s->tracks - 1);
	if (!s->tracks)
		a->track = -1;
	if (a->pattern >= s->patterns)
		a->pattern = s->patterns - 1;
	if (a->clip >= s->clips || (a->clip >= 0 && s->clip[a->clip].track != a->track))
		a->clip = -1;
	if (a->tab != 1 || a->selection_pattern != a->pattern)
		ms_notes_clear_selection(a);
	ms_editor_view_clamp(a);
	a->syncing = 1;
	if (a->transport.track != a->track || a->transport.pattern != a->pattern || a->transport.clip != a->clip)
	{
		if (a->transport.pattern_mode)
			ms_transport_seek(&a->transport, 0);
		a->transport.track = a->track;
		a->transport.pattern = a->pattern;
		a->transport.clip = a->clip;
	}
	int instrument_hidden = a->tab != 1 || a->track < 0 || s->track[a->track].kind != MS_SYNTH;
	if (instrument_hidden || a->worker)
		YMGUI_Dropdown_Close(a->instrument);
	YMGUI_Obj_SetHidden(a->instrument, instrument_hidden);
	YMGUI_Obj_SetHidden(a->instrument_label, instrument_hidden);
	YMGUI_Obj_SetHidden(a->sound_mode, a->tab != 0);
	if (a->tab != 1 || a->worker)
		YMGUI_Dropdown_Close(a->piano_grid);
	YMGUI_Obj_SetHidden(a->piano_grid, a->tab != 1);
	YMGUI_Dropdown_SetSelected(a->piano_grid, a->note_grid);
	if (!instrument_hidden)
		YMGUI_Dropdown_SetSelected(a->instrument, s->track[a->track].instrument);
	char b[128];
	YMGUI_Obj_SetHidden(a->clip_gain, a->clip < 0);
	YMGUI_Obj_SetHidden(a->fade_in, a->clip < 0);
	YMGUI_Obj_SetHidden(a->fade_out, a->clip < 0);
	snprintf(b, sizeof(b), "%.2f", a->clip >= 0 ? s->clip[a->clip].fade_in : 0);
	YMGUI_TextInput_SetText(a->fade_in, b);
	snprintf(b, sizeof(b), "%.2f", a->clip >= 0 ? s->clip[a->clip].fade_out : 0);
	YMGUI_TextInput_SetText(a->fade_out, b);
	snprintf(b, sizeof(b), "%s%s", s->name, a->dirty ? " *" : "");
	YMGUI_Label_SetText(a->title, b);
	snprintf(b, sizeof(b), "%d", s->bpm);
	YMGUI_TextInput_SetText(a->bpm_input, b);
	YMGUI_TextInput_SetText(a->track_name, a->track >= 0 ? s->track[a->track].name : "");
	YMGUI_TextInput_SetText(a->pattern_name, a->pattern >= 0 ? s->pattern[a->pattern].name : "");
	YMGUI_Slider_SetValue(a->volume, a->track >= 0 ? (int)(s->track[a->track].volume * 100) : 0);
	YMGUI_Slider_SetValue(a->pan, a->track >= 0 ? (int)(s->track[a->track].pan * 100) : 0);
	YMGUI_Slider_SetValue(a->master, (int)(s->master * 100));
	YMGUI_Slider_SetValue(a->clip_gain, a->clip >= 0 ? (int)(s->clip[a->clip].gain * 100) : 100);
	YMGUI_Slider_SetValue(a->velocity, a->note_velocity);
	YMGUI_Slider_SetValue(a->drum_pan_slider, (int)(s->drum_pan[a->drum] * 100));
	snprintf(b, sizeof(b), "鼓声声像 %d", (int)(s->drum_pan[a->drum] * 100));
	YMGUI_Label_SetText(a->drum_pan_label, b);
	YMGUI_Obj_SetHidden(a->drum_pan_label, a->tab != 0);
	YMGUI_Obj_SetHidden(a->drum_pan_slider, a->tab != 0);
	YMGUI_Obj_SetHidden(a->velocity, a->tab == 2);
	YMGUI_Obj_SetHidden(a->velocity_label, a->tab == 2);
	YMGUI_Obj_SetHidden(a->pattern_name, a->tab == 2);
	for (int i = 0; i < a->button_count; ++i)
	{
		MsAction ac = a->actions[i];
		int hidden = ((ac == A_SAMPLE || ac == A_SAMPLE_RESET || ac == A_DRUM_MUTE || ac == A_DRUM_SOLO) && a->tab != 0) ||
					 ((ac == A_INSTRUMENT || ac == A_NOTE_SHORT || ac == A_NOTE_LONG || ac == A_OCT_DOWN || ac == A_OCT_UP || ac == A_QUANTIZE || ac == A_NOTE_SELECT || ac == A_NOTE_DELETE) && a->tab != 1) ||
					 ((ac >= A_PAT_NEW && ac <= A_PAT_CLEAR) && a->tab == 2);
		YMGUI_Obj_SetHidden(a->buttons[i], hidden);
		int on = (ac == A_NOTE_SELECT && a->select_notes) || (ac == A_PREVIEW && a->transport.playing && a->transport.pattern_mode) || (ac == A_PLAY && a->transport.playing) || (ac == A_LOOP && a->transport.loop) || (ac == A_METRO && a->transport.metronome) ||
				 (ac == A_DRUM && a->tab == 0) || (ac == A_PIANO && a->tab == 1) || (ac == A_MIXER && a->tab == 2) || (ac == A_SNAP && a->snap) ||
				 (ac == A_LIBRARY_PAT && !a->library_tab) || (ac == A_LIBRARY_AUDIO && a->library_tab) || (ac == A_DRUM_MUTE && s->drum_mute[a->drum]) || (ac == A_DRUM_SOLO && s->drum_solo[a->drum]);
		YMGUI_Button_SetColors(a->buttons[i], MS_RGB(on ? 0x42695e : 0x393f46), MS_RGB(0x58776c));
		if (ac == A_NOTE_SELECT)
			YMGUI_Button_SetText(a->buttons[i], a->select_notes ? "框选已有：开" : "框选已有：关");
		if (ac == A_PLAY)
			YMGUI_Button_SetText(a->buttons[i], a->transport.playing ? "暂停" : "播放");
		if (ac == A_PREVIEW)
			YMGUI_Button_SetText(a->buttons[i], a->transport.playing && a->transport.pattern_mode ? "停止试听" : "试听片段");
		if (ac == A_MODE)
			YMGUI_Button_SetText(a->buttons[i], a->transport.pattern_mode ? "片段循环" : "整曲播放");
		if (ac == A_BEATS)
		{
			snprintf(b, sizeof(b), "拍号 %d/4", s->beats);
			YMGUI_Button_SetText(a->buttons[i], b);
		}
	}
	a->syncing = 0;
	YMGUI_Obj_Invalidate(a->ctx->root);
}
