#include "studio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void ms_checkpoint(MsApp* a)
{
	ms_history_push(a->history, &a->project.song);
}
void ms_changed(MsApp* a)
{
	a->dirty = 1;
	ms_transport_seek(&a->transport, ms_transport_position(&a->transport));
	ms_ui_refresh(a);
}
void ms_action(MsApp* a, MsAction ac)
{
	if (a->drag == 2 || a->drag == 6 || a->drag == 7)
		return;
	if (a->worker)
	{
		ms_status(a, "正在处理音频，请稍候");
		return;
	}
	MsSong* s = &a->project.song;
	MsClip* c = a->clip >= 0 && a->clip < s->clips ? &s->clip[a->clip] : NULL;
	MsPattern* p = a->pattern >= 0 && a->pattern < s->patterns ? &s->pattern[a->pattern] : NULL;
	int tick = (int)ms_frame_tick(s, ms_transport_position(&a->transport));
	if ((ac == A_FADE_IN || ac == A_FADE_OUT) && !c)
	{
		ms_status(a, "先将素材加入轨道并选择片段，再设置淡入淡出");
		return;
	}
	if (ac == A_HELP)
	{
		YMGUI_MsgBox_ClearButtons(a->msgbox);
		YMGUI_MsgBox_SetTitle(a->msgbox, "音乐工坊 · 操作帮助");
		YMGUI_MsgBox_SetText(a->msgbox, "空格播放，回车回到开头。\n拖动素材加入时间线，拖动片段两端裁剪。\n鼓机点击鼓点，下方调力度；鼓名上滚轮调音量。\n钢琴点击写音符，右端改长度，右键删除。\n开启“框选已有”，拖动空白区域选择，整体移动/删除。");
		YMGUI_MsgBox_AddButton(a->msgbox, "知道了", NULL);
		YMGUI_MsgBox_Show(a->msgbox);
		return;
	}
	if (ac == A_NEW)
	{
		if (a->dirty)
		{
			ms_confirm(a, A_NEW, "当前作品尚未保存。继续新建将丢弃修改。");
			return;
		}
		a->transport.playing = 0;
		ms_transport_seek(&a->transport, 0);
		ms_project_free(&a->project);
		ms_project_init(&a->project, 0);
		memset(a->history, 0, sizeof(*a->history));
		a->path[0] = 0;
		a->clip = -1;
		a->pattern = 0;
		ms_notes_clear_selection(a);
		a->track = 0;
		a->asset = -1;
		a->clipboard_valid = 0;
		a->scroll = 0;
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_OPEN && a->dirty)
	{
		ms_confirm(a, A_OPEN, "打开其他作品将替换当前未保存的修改，是否继续？");
		return;
	}
	if (ac == A_SAVE && a->path[0])
	{
		ms_save(a, a->path);
		return;
	}
	if (ac == A_OPEN || ac == A_SAVE || ac == A_SAVE_AS || ac == A_IMPORT || ac == A_EXPORT || ac == A_EXPORT_LOOP)
	{
		ms_dialog(a, ac);
		return;
	}
	if (ac == A_PREVIEW)
	{
		if (a->library_tab && !c)
		{
			ms_status(a, "请先选择已加入轨道的音频片段");
			return;
		}
		if (!p && !c)
		{
			ms_status(a, "请先选择一个节奏、旋律或音频片段");
			return;
		}
		if (!a->transport.device)
		{
			ms_status(a, "声卡打开失败，无法试听；仍可编辑和导出");
			return;
		}
		int stop = a->transport.pattern_mode && a->transport.playing;
		if (!c && p && (a->track < 0 || a->track >= s->tracks || s->track[a->track].kind != p->kind))
			ms_select_pattern(a, a->pattern);
		if (a->track < 0 || (!c && (!p || s->track[a->track].kind != p->kind)))
		{
			ms_status(a, "请先添加与片段类型匹配的鼓轨或乐器轨");
			return;
		}
		a->transport.pattern_mode = 1;
		a->transport.playing = !stop;
		ms_transport_seek(&a->transport, 0);
		ms_ui_refresh(a);
		ms_status(a, stop ? "试听已停止；顶部可切换整曲播放" : "正在单独试听当前片段，保留淡化、增益和裁剪设置");
		return;
	}
	if (ac == A_INSTRUMENT)
	{
		if (!a->transport.device)
			ms_status(a, "声卡打开失败，无法试听");
		else if (a->transport.playing)
			ms_status(a, "请先停止播放，再试听单个音色");
		else
		{
			int pitch = p && a->note >= 0 && a->note < p->count ? p->notes[a->note].pitch : ms_clamp(a->octave * 12, 0, 127);
			if (a->track < 0 || a->track >= s->tracks || s->track[a->track].kind != MS_SYNTH)
				ms_select_pattern(a, a->pattern);
			ms_audition(&a->transport, &a->project, &a->sounds, -1, pitch);
		}
		return;
	}
	if (ac == A_PLAY)
	{
		if (!a->transport.device)
		{
			ms_status(a, "声卡打开失败；可编辑并导出，检查桌面音频设备后重新启动");
			return;
		}
		int64_t pos = ms_transport_position(&a->transport);
		ms_transport_seek(&a->transport, pos);
		a->transport.playing ^= 1;
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_STOP)
	{
		a->transport.playing = 0;
		ms_transport_seek(&a->transport, 0);
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_LOOP)
	{
		a->transport.loop ^= 1;
		ms_transport_seek(&a->transport, ms_transport_position(&a->transport));
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_METRO)
	{
		a->transport.metronome ^= 1;
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_MODE)
	{
		a->transport.pattern_mode ^= 1;
		ms_transport_seek(&a->transport, 0);
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_UNDO || ac == A_REDO)
	{
		a->clipboard_valid = 0;
		int ok = ac == A_UNDO ? ms_history_undo(a->history, s) : ms_history_redo(a->history, s);
		a->clip = -1;
		ms_notes_clear_selection(a);
		if (ok)
			ms_changed(a);
		else
			ms_ui_refresh(a);
		return;
	}
	if (ac == A_DRUM || ac == A_PIANO || ac == A_MIXER)
	{
		a->tab = ac - A_DRUM;
		if (a->tab < 2 && (!p || p->kind != a->tab || a->track < 0 || s->track[a->track].kind != a->tab))
			for (int i = 0; i < s->patterns; ++i)
				if (s->pattern[i].kind == a->tab)
				{
					ms_select_pattern(a, i);
					break;
				}
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_LIBRARY_PAT || ac == A_LIBRARY_AUDIO)
	{
		a->library_tab = ac == A_LIBRARY_AUDIO;
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_APPEND)
	{
		ms_add_source(a, a->track, (tick / MS_STEP) * MS_STEP);
		return;
	}
	if (ac == A_ZOOM_IN || ac == A_ZOOM_OUT)
	{
		ms_timeline_zoom_at(a, MS_TIMEX, ac == A_ZOOM_IN ? 1 : -1);
		return;
	}
	if (ac == A_SNAP)
	{
		a->snap ^= 1;
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_OCT_DOWN || ac == A_OCT_UP)
	{
		a->octave = ms_clamp(a->octave + (ac == A_OCT_UP ? 1 : -1), 0, 9);
		ms_ui_refresh(a);
		return;
	}
	if (ac == A_NOTE_SELECT)
	{
		a->select_notes ^= 1;
		ms_notes_clear_selection(a);
		ms_ui_refresh(a);
		ms_status(a, a->select_notes ? "框选已开启：拖动空白区域选择已有音符，不会新增；拖动所选音符可整体移动" : "框选已关闭：点击空白网格添加音符");
		return;
	}
	if ((ac == A_NOTE_DELETE || (a->select_notes && (ac == A_NOTE_SHORT || ac == A_NOTE_LONG || ac == A_QUANTIZE))) &&
		(!p || p->kind != MS_SYNTH || !ms_notes_selection_count(a)))
		return;
	if (ac == A_SAMPLE)
	{
		a->import_drum = 1;
		ms_dialog(a, A_SAMPLE);
		return;
	}
	ms_checkpoint(a);
	switch (ac)
	{
	case A_BEATS:
		s->beats = s->beats == 4 ? 3 : s->beats == 3 ? 6
													 : 4;
		break;
	case A_ADD_DRUM:
	case A_ADD_SYNTH:
	case A_ADD_AUDIO:
	{
		int t = ms_track_add(s, ac - A_ADD_DRUM);
		if (t >= 0)
			a->track = t;
		else
			ms_status(a, "最多支持 8 条轨道");
	}
	break;
	case A_DEL_TRACK:
		a->clipboard_valid = 0;
		ms_track_delete(s, a->track);
		a->clip = -1;
		break;
	case A_UP:
	case A_DOWN:
	{
		a->clipboard_valid = 0;
		int to = a->track + (ac == A_UP ? -1 : 1);
		if (to >= 0 && to < s->tracks)
		{
			ms_track_move(s, a->track, to);
			a->track = to;
		}
	}
	break;
	case A_REPEAT:
		if (c)
		{
			MsClip copy = *c;
			int n = ms_clip_add(s, c->track, c->source, c->start + c->length, c->length);
			if (n >= 0)
			{
				copy.start += copy.length;
				s->clip[n] = copy;
				a->clip = n;
			}
		}
		break;
	case A_SPLIT:
		if (c)
		{
			int i = ms_clip_split(s, a->clip, tick);
			if (i >= 0)
				a->clip = i;
			else
				ms_status(a, "请将播放指针放在所选片段内部再分割");
		}
		break;
	case A_DELETE:
		if (c)
		{
			ms_clip_delete(s, a->clip);
			a->clip = -1;
		}
		break;
	case A_LOOP_START:
		if (tick < s->loop_end)
			s->loop_start = ms_clamp(tick, 0, MS_END - 1);
		break;
	case A_LOOP_END:
		if (tick > s->loop_start)
			s->loop_end = ms_clamp(tick, 1, MS_END);
		break;
	case A_PAT_NEW:
	case A_PAT_COPY:
	{
		int kind = a->tab == 1 ? MS_SYNTH : MS_DRUM;
		int n = ms_pattern_add(s, kind, ac == A_PAT_COPY ? a->pattern : -1);
		if (n >= 0)
		{
			a->library_tab = 0;
			ms_select_pattern(a, n);
		}
		else
			ms_status(a, "最多支持 16 个节奏型与旋律片段");
	}
	break;
	case A_PAT_UNIQUE:
		if (c && s->track[c->track].kind != MS_AUDIO)
		{
			int n = ms_pattern_add(s, s->track[c->track].kind, c->source);
			if (n >= 0)
			{
				c->source = n;
				ms_select_pattern(a, n);
			}
		}
		break;
	case A_NOTE_DELETE:
		if (p && p->kind == MS_SYNTH)
		{
			int count = 0, previous = p->count;
			for (int i = 0; i < previous; ++i)
				if (!ms_note_selected(a, i))
					p->notes[count++] = p->notes[i];
			memset(&p->notes[count], 0, (size_t)(previous - count) * sizeof(MsNote));
			p->count = count;
			ms_notes_clear_selection(a);
		}
		break;
	case A_PAT_LENGTH:
		ms_notes_clear_selection(a);
		if (p)
		{
			p->steps = p->steps == 16 ? 32 : 16;
			for (int i = p->count - 1; i >= 0; --i)
				if (p->notes[i].tick >= p->steps * MS_STEP)
				{
					memmove(&p->notes[i], &p->notes[i + 1], (size_t)(p->count - i - 1) * sizeof(MsNote));
					memset(&p->notes[--p->count], 0, sizeof(MsNote));
				}
				else if (p->notes[i].tick + p->notes[i].length > p->steps * MS_STEP)
					p->notes[i].length = p->steps * MS_STEP - p->notes[i].tick;
			/* 32 缩为 16 步时，明确移除后半部事件。 */
			if (p->steps == 16)
				for (int d = 0; d < MS_DRUMS; ++d)
					memset(&p->drum[d][16], 0, 16);
		}
		break;
	case A_PAT_CLEAR:
		if (p)
		{
			memset(p->drum, 0, sizeof(p->drum));
			memset(p->notes, 0, sizeof(p->notes));
			p->count = 0;
			ms_notes_clear_selection(a);
		}
		break;
	case A_SAMPLE_RESET:
		s->drum_asset[a->drum] = -1;
		break;
	case A_DRUM_MUTE:
		s->drum_mute[a->drum] ^= 1;
		break;
	case A_DRUM_SOLO:
		s->drum_solo[a->drum] ^= 1;
		break;
	case A_NOTE_SHORT:
	case A_NOTE_LONG:
	{
		int length = p && a->note >= 0 && a->note < p->count ? p->notes[a->note].length : a->note_length;
		int limit = p ? p->steps * MS_STEP : MS_STEP * 32;
		int delta = ms_piano_step(a) * (ac == A_NOTE_LONG ? 1 : -1);
		a->note_length = ms_clamp(length + delta, 1, limit);
		if (p)
			for (int i = 0; i < p->count; ++i)
				if (ms_note_selected(a, i))
					p->notes[i].length = ms_clamp(p->notes[i].length + delta, 1, p->steps * MS_STEP - p->notes[i].tick);
	}
	break;
	case A_FADE_IN:
		if (c)
			c->fade_in = c->fade_in > 0 ? 0 : .15f;
		break;
	case A_FADE_OUT:
		if (c)
			c->fade_out = c->fade_out > 0 ? 0 : .3f;
		break;
	case A_QUANTIZE:
		if (p)
			for (int i = 0; i < p->count; ++i)
			{
				if (a->select_notes && !ms_note_selected(a, i))
					continue;
				MsNote* n = &p->notes[i];
				int quantum = ms_piano_step(a);
				n->tick = ms_clamp((int)lround((double)n->tick / quantum) * quantum, 0, p->steps * MS_STEP - n->length);
				n->length = ms_clamp(n->length, 1, p->steps * MS_STEP - n->tick);
			}
		break;
	default:
		break;
	}
	ms_changed(a);
}
