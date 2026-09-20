#include "app/studio.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
static void click(int x, int y)
{
	YMGUI_Inject_Pointer(x, y, 1);
	YMGUI_Inject_Pointer(x, y, 0);
}
#define CHECK(x)                                                                \
	do                                                                          \
	{                                                                           \
		if (!(x))                                                               \
		{                                                                       \
			fprintf(stderr, "UI 检查失败：%s:%d %s\n", __FILE__, __LINE__, #x); \
			return 0;                                                           \
		}                                                                       \
	} while (0)
static int test_library_feedback(MsApp* a)
{
	MsSong* original = malloc(sizeof(*original));
	CHECK(original);
	*original = a->project.song;
	MsSong* s = &a->project.song;
	int track = ms_track_add(s, MS_SYNTH);
	int pattern = ms_pattern_add(s, MS_SYNTH, 1);
	CHECK(track >= 0 && pattern >= 0);
	s->track[track].instrument = 8;
	s->track[track].volume = .5f;
	s->track[track].pan = .25f;
	snprintf(s->track[track].name, MS_NAME, "测试圆号");
	int clip = ms_clip_add(s, track, pattern, 0, 4 * MS_PPQ);
	CHECK(clip >= 0);
	a->track = 1;
	a->library_tab = 0;
	ms_ui_refresh(a);
	/* 同为乐器轨也必须跳转到真正引用片段的轨道，并同步属性和音色。 */
	int row = 188 + pattern * 17 + 5;
	click(45, row);
	CHECK(a->track == track && a->pattern == pattern && a->clip == clip);
	CHECK(a->transport.track == track && YMGUI_Dropdown_GetSelected(a->instrument) == 8);
	CHECK(!strcmp(YMGUI_TextInput_GetText(a->track_name), "测试圆号"));
	CHECK(YMGUI_Slider_GetValue(a->volume) == 50 && YMGUI_Slider_GetValue(a->pan) == 25);
	CHECK(a->drag_ghost->state & GY_STATE_Hidden);
	/* 秒数输入可提交、非法值不产生编辑，未选片段时不会误改上一段。 */
	click(45, 810);
	YMGUI_TextInput_SetText(a->fade_in, "0.75");
	YMGUI_Inject_Key(GY_KEY_ENTER, 1);
	YMGUI_Inject_Key(GY_KEY_ENTER, 0);
	CHECK(s->clip[clip].fade_in == .75f);
	click(140, 810);
	YMGUI_TextInput_SetText(a->fade_out, "1.25");
	YMGUI_Inject_Key(GY_KEY_ENTER, 1);
	YMGUI_Inject_Key(GY_KEY_ENTER, 0);
	CHECK(s->clip[clip].fade_out == 1.25f);
	int history = a->history->undos;
	click(140, 810);
	YMGUI_TextInput_SetText(a->fade_out, "nan");
	YMGUI_Inject_Key(GY_KEY_ENTER, 1);
	YMGUI_Inject_Key(GY_KEY_ENTER, 0);
	CHECK(s->clip[clip].fade_out == 1.25f && a->history->undos == history);
	CHECK(a->transport.clip == clip);
	/* 同一旋律被两轨引用时，保持当前实际引用；时间线点击保持精确选中项。 */
	int shared = ms_clip_add(s, 1, pattern, 4 * MS_PPQ, 4 * MS_PPQ);
	CHECK(shared >= 0);
	a->track = 1;
	a->clip = shared;
	click(45, row);
	CHECK(a->track == 1 && a->clip == shared);
	ms_clip_delete(s, shared);
	int clips = s->clips;
	YMGUI_Inject_Pointer(45, row, 1);
	CHECK(!a->drag_active && a->drag_ghost->state & GY_STATE_Hidden);
	/* 乐器拖向鼓轨：红色拒绝状态，模型和历史不应改变。 */
	YMGUI_Inject_Pointer(MS_TIMEX + 120, MS_TIMEY + 14, 1);
	CHECK(a->drag_active && !a->drop_valid && a->drop_track == 0);
	CHECK(!(a->drag_ghost->state & GY_STATE_Hidden));
	YMGUI_Refresh(a->ctx);
	int old_x = a->drag_ghost->area.x;
	YMGUI_Inject_Pointer(MS_TIMEX + 220, MS_TIMEY + track * MS_ROWH + 14, 1);
	CHECK(a->drop_valid && a->drop_track == track && a->drop_tick % MS_STEP == 0);
	CHECK(a->drag_ghost->area.x != old_x && s->clips == clips);
	YMGUI_Refresh(a->ctx);
	YMGUI_Inject_PointerCancel();
	CHECK(a->drag_ghost->state & GY_STATE_Hidden);
	CHECK(!a->drag_active && a->drop_track == -1 && s->clips == clips);
	YMGUI_Inject_Pointer(45, row, 1);
	YMGUI_Inject_Pointer(MS_TIMEX + 120, MS_TIMEY + 14, 1);
	YMGUI_Inject_Pointer(MS_TIMEX + 120, MS_TIMEY + 14, 0);
	CHECK(s->clips == clips && a->drag_ghost->state & GY_STATE_Hidden);
	/* 新建但还没有编排的片段，保留当前同类轨道作为试听/放置目标。 */
	ms_clip_delete(s, clip);
	a->track = track;
	click(45, row);
	CHECK(a->track == track && a->clip == -1);
	*s = *original;
	free(original);
	memset(a->history, 0, sizeof(*a->history));
	a->dirty = 0;
	a->track = 0;
	a->pattern = 0;
	a->clip = -1;
	a->tab = 0;
	ms_ui_refresh(a);
	return 1;
}

static void choose_piano_grid(int index)
{
	click(1500, 570);
	click(1500, 586 + index * 22 + 10);
}
static int test_fine_notes(MsApp* a)
{
	MsSong* original = malloc(sizeof(*original));
	CHECK(original);
	*original = a->project.song;
	int old_length = a->note_length, old_velocity = a->note_velocity;
	a->track = 1;
	a->pattern = 1;
	a->tab = 1;
	a->note = -1;
	a->clip = -1;
	MsPattern* p = &a->project.song.pattern[1];
	p->steps = 16;
	p->count = 0;
	ms_ui_refresh(a);
	CHECK(YMGUI_Dropdown_GetOptionCount(a->piano_grid) == 7);
	int w = MS_GRID_WIDTH / 16;
	const int order[] = {1, 2, 3, 4, 5, 6, 0};
	const int ticks[] = {24, 12, 6, 32, 16, 8, 1};
	for (int i = 0; i < 7; ++i)
	{
		int choice = order[i], quantum = ticks[choice];
		int undos = a->history->undos;
		choose_piano_grid(choice);
		CHECK(a->note_grid == choice && a->note_length == quantum);
		CHECK(a->history->undos == undos && !ms_instrument_popup_open(a));
		int x = MS_GRID_X + 3 * w + w / 2;
		click(x, 640);
		CHECK(p->count == 1 && p->notes[0].length == quantum);
		CHECK(p->notes[0].tick == (x - MS_GRID_X) * MS_STEP / (w * quantum) * quantum);
		YMGUI_Refresh(a->ctx);
		ms_action(a, A_UNDO);
		CHECK(p->count == 0);
		ms_action(a, A_REDO);
		CHECK(p->count == 1 && p->notes[0].length == quantum);
		ms_action(a, A_UNDO);
	}
	/* 已有非网格短音符：选择、垂直移动不能拉长，拖动可撤销。 */
	p->notes[0] = (MsNote){5, 3, 71, 90};
	p->count = 1;
	int left = MS_GRID_X + 5 * w / MS_STEP;
	int right = left + 3 * w / MS_STEP;
	click(right - 1, 640);
	CHECK(p->notes[0].length == 3);
	YMGUI_Inject_Pointer(left + 1, 640, 1);
	YMGUI_Inject_Pointer(left + 1, 640 + MS_KEY_ROW, 1);
	YMGUI_Inject_Pointer(left + 1, 640 + MS_KEY_ROW, 0);
	CHECK(p->notes[0].tick == 5 && p->notes[0].length == 3 && p->notes[0].pitch == 70);
	ms_action(a, A_UNDO);
	CHECK(p->notes[0].pitch == 71);
	choose_piano_grid(2);
	CHECK(p->notes[0].length == 3);
	YMGUI_Inject_Pointer(right - 1, 640, 1);
	YMGUI_Inject_Pointer(right - 1 + 20, 640, 1);
	YMGUI_Inject_Pointer(right - 1 + 20, 640, 0);
	CHECK(p->notes[0].length == 9);
	ms_action(a, A_UNDO);
	/* 关闭吸附，仍选 1/64，但拖动和音长按钮按 1 tick 细调。 */
	ms_action(a, A_SNAP);
	CHECK(!a->snap && ms_piano_step(a) == 1);
	YMGUI_Inject_Pointer(left + 1, 640, 1);
	YMGUI_Inject_Pointer(left + 1 + 13, 640, 1);
	YMGUI_Inject_Pointer(left + 1 + 13, 640, 0);
	CHECK(p->notes[0].tick == 9 && p->notes[0].length == 3);
	ms_action(a, A_UNDO);
	click(left + 1, 640);
	ms_action(a, A_NOTE_SHORT);
	CHECK(p->notes[0].length == 2);
	ms_action(a, A_NOTE_SHORT);
	CHECK(p->notes[0].length == 1);
	ms_action(a, A_NOTE_SHORT);
	CHECK(p->notes[0].length == 1);
	ms_action(a, A_NOTE_LONG);
	CHECK(p->notes[0].length == 2);
	/* 最短音符在 32 步视图也能选中、右端拖长；取消完整恢复。 */
	p->steps = 32;
	p->notes[0] = (MsNote){0, 1, 71, 90};
	a->note = -1;
	YMGUI_Refresh(a->ctx);
	click(MS_GRID_X + 1, 640);
	CHECK(a->note == 0 && p->count == 1);
	YMGUI_Inject_Pointer(MS_GRID_X + 2, 640, 1);
	YMGUI_Inject_Pointer(MS_GRID_X + 12, 640, 1);
	CHECK(p->notes[0].length > 1);
	YMGUI_Inject_PointerCancel();
	CHECK(p->notes[0].length == 1 && !a->drag);
	/* 三连音量化与自由模式；不会裁短末尾已有音符。 */
	ms_action(a, A_SNAP);
	choose_piano_grid(4);
	p->notes[0] = (MsNote){19, 3, 71, 90};
	p->notes[1] = (MsNote){757, 11, 70, 90};
	p->count = 2;
	ms_action(a, A_QUANTIZE);
	CHECK(p->notes[0].tick == 16 && p->notes[0].length == 3);
	CHECK(p->notes[1].tick + p->notes[1].length <= 768 && p->notes[1].length == 11);
	CHECK(ms_song_valid(&a->project.song, a->project.assets));
	ms_action(a, A_UNDO);
	CHECK(p->notes[0].tick == 19);
	choose_piano_grid(6);
	ms_action(a, A_QUANTIZE);
	CHECK(p->notes[0].tick == 19 && p->notes[0].length == 3);
	click(1500, 570);
	CHECK(ms_instrument_popup_open(a));
	ms_action(a, A_DRUM);
	CHECK(!ms_instrument_popup_open(a) && a->piano_grid->state & GY_STATE_Hidden);
	*a->gesture = *original;
	a->project.song = *original;
	free(original);
	memset(a->history, 0, sizeof(*a->history));
	a->note_grid = 0;
	a->note_length = old_length;
	a->note_velocity = old_velocity;
	a->note = -1;
	a->track = a->pattern = a->tab = a->dirty = 0;
	a->snap = 1;
	ms_ui_refresh(a);
	return 1;
}

static void box_drag(int x1, int y1, int x2, int y2)
{
	YMGUI_Inject_Pointer(x1, y1, 1);
	YMGUI_Inject_Pointer(x2, y2, 1);
	YMGUI_Inject_Pointer(x2, y2, 0);
}
static int test_box_selection(MsApp* a)
{
	MsSong* original = malloc(sizeof(*original));
	CHECK(original);
	*original = a->project.song;
	int old_velocity = a->note_velocity, old_length = a->note_length;
	a->track = a->pattern = a->tab = 1;
	a->clip = -1;
	a->note_grid = 0;
	a->snap = 1;
	a->octave = 5;
	ms_notes_clear_selection(a);
	MsPattern* p = &a->project.song.pattern[1];
	memset(p->notes, 0, sizeof(p->notes));
	p->steps = 16;
	p->count = 4;
	p->notes[0] = (MsNote){24, 12, 71, 90};
	p->notes[1] = (MsNote){72, 1, 69, 80}; /* 最短音符也可框选。 */
	p->notes[2] = (MsNote){144, 24, 65, 70};
	p->notes[3] = (MsNote){30, 12, 59, 60}; /* 屏外八度不能误选。 */
	MsPattern initial = *p;
	memset(a->history, 0, sizeof(*a->history));
	a->dirty = 0;
	ms_ui_refresh(a);
	click(1450, 612);
	CHECK(a->select_notes && !a->dirty && a->history->undos == 0);
	click(410, 640);
	CHECK(p->count == 4 && !ms_notes_selection_count(a));
	box_drag(410, 632, 700, 723);
	CHECK(ms_notes_selection_count(a) == 2 && a->note_selected[0] && a->note_selected[1]);
	CHECK(!a->note_selected[2] && !a->note_selected[3]);
	CHECK(!a->dirty && !a->history->undos && !memcmp(p, &initial, sizeof(initial)));
	YMGUI_Refresh(a->ctx);
	/* 反向拖框、失焦取消与点按已有音符都不产生工程修改。 */
	box_drag(700, 723, 410, 632);
	CHECK(ms_notes_selection_count(a) == 2);
	YMGUI_Inject_Pointer(900, 900, 1);
	YMGUI_Inject_Pointer(950, 950, 1);
	CHECK(ms_notes_selection_count(a) == 0);
	YMGUI_Inject_PointerCancel();
	CHECK(!a->drag && ms_notes_selection_count(a) == 2 && !a->dirty);
	click(480, 640);
	CHECK(ms_notes_selection_count(a) == 2 && !a->dirty && !a->history->undos);
	ms_action(a, A_UNDO);
	CHECK(!ms_notes_selection_count(a) && !a->dirty && !a->history->undos);
	box_drag(410, 632, 700, 723);
	/* 整体移动一步、一半音，只记一次撤销。 */
	int w = MS_GRID_WIDTH / 16;
	box_drag(480, 640, 480 + w, 640 + MS_KEY_ROW);
	CHECK(p->notes[0].tick == 48 && p->notes[1].tick == 96);
	CHECK(p->notes[0].pitch == 70 && p->notes[1].pitch == 68);
	CHECK(p->notes[0].length == 12 && p->notes[1].length == 1);
	CHECK(!memcmp(&p->notes[2], &initial.notes[2], 2 * sizeof(MsNote)));
	CHECK(a->history->undos == 1);
	MsPattern moved = *p;
	YMGUI_Inject_Pointer(480 + w, 671, 1);
	YMGUI_Inject_Pointer(480 + 2 * w, 702, 1);
	YMGUI_Inject_PointerCancel();
	CHECK(!memcmp(p, &moved, sizeof(moved)) && a->history->undos == 1);
	/* 组内最早音符撞到左边界，其他音符保留间距。 */
	box_drag(480 + w, 671, 410, 671);
	CHECK(p->notes[0].tick == 0 && p->notes[1].tick == 48);
	ms_action(a, A_UNDO);
	CHECK(!memcmp(p, &moved, sizeof(moved)) && !ms_notes_selection_count(a));
	box_drag(540, 650, 800, 758);
	CHECK(ms_notes_selection_count(a) == 2);
	int right = MS_GRID_X + (48 + 12) * w / MS_STEP - 1;
	box_drag(right, 671, right + w, 671);
	CHECK(p->notes[0].length == 36 && p->notes[1].length == 25);
	ms_action(a, A_NOTE_SHORT);
	CHECK(p->notes[0].length == 12 && p->notes[1].length == 1);
	ms_action(a, A_NOTE_LONG);
	CHECK(p->notes[0].length == 36 && p->notes[1].length == 25);
	/* 量化和力度仅影响选区，工具栏获得焦点后 Delete 仍不删轨道片段。 */
	p->notes[0].tick = 49;
	p->notes[1].tick = 97;
	p->notes[2].tick = 145;
	ms_action(a, A_QUANTIZE);
	CHECK(p->notes[0].tick == 48 && p->notes[1].tick == 96 && p->notes[2].tick == 145);
	click(1150, 573);
	CHECK(p->notes[0].velocity == p->notes[1].velocity && p->notes[0].velocity != 90);
	CHECK(p->notes[2].velocity == 70 && p->notes[3].velocity == 60);
	int clips = a->project.song.clips;
	a->clip = 0;
	CHECK(clips > 0);
	YMGUI_Inject_Key(GY_KEY_DEL, 1);
	YMGUI_Inject_Key(GY_KEY_DEL, 0);
	CHECK(p->count == 2 && a->project.song.clips == clips);
	CHECK(p->notes[0].pitch == 65 && p->notes[1].pitch == 59);
	CHECK(!ms_notes_selection_count(a));
	ms_action(a, A_UNDO);
	CHECK(p->count == 4 && p->notes[0].length == 36);
	ms_action(a, A_REDO);
	CHECK(p->count == 2);
	ms_action(a, A_UNDO);
	box_drag(540, 650, 850, 758);
	CHECK(ms_notes_selection_count(a) == 2);
	click(1590, 612);
	CHECK(p->count == 2 && a->project.song.clips == clips);
	/* 空选区的删除/长度/量化不新增音符或历史，关闭后恢复放置。 */
	int undos = a->history->undos;
	click(1590, 612);
	ms_action(a, A_NOTE_LONG);
	ms_action(a, A_QUANTIZE);
	CHECK(p->count == 2 && a->history->undos == undos);
	box_drag(410, 632, 1100, 900);
	CHECK(ms_notes_selection_count(a) == 1);
	ms_select_pattern(a, 0);
	CHECK(!ms_notes_selection_count(a));
	ms_select_pattern(a, 1);
	CHECK(!ms_notes_selection_count(a) && a->select_notes);
	click(1450, 612);
	CHECK(!a->select_notes);
	click(410, 640);
	CHECK(p->count == 3);
	CHECK(ms_song_valid(&a->project.song, a->project.assets));
	a->project.song = *original;
	*a->gesture = *original;
	free(original);
	memset(a->history, 0, sizeof(*a->history));
	a->track = a->pattern = a->tab = a->dirty = 0;
	a->note_velocity = old_velocity;
	a->note_length = old_length;
	ms_notes_clear_selection(a);
	ms_ui_refresh(a);
	return 1;
}

static int test_view_zoom(MsApp* a)
{
	MsSong* original = malloc(sizeof(*original));
	CHECK(original);
	*original = a->project.song;
	int old_length = a->note_length, old_velocity = a->note_velocity;
	double old_zoom = a->zoom;
	a->track = a->pattern = a->tab = 1;
	a->clip = -1;
	a->editor_zoom = 1;
	a->editor_scroll = a->scroll = 0;
	a->select_notes = 0;
	a->note_grid = 6;
	a->snap = 1;
	a->octave = 5;
	ms_notes_clear_selection(a);
	MsPattern* p = &a->project.song.pattern[1];
	memset(p->notes, 0, sizeof(p->notes));
	p->steps = 16;
	p->count = 3;
	p->notes[0] = (MsNote){100, 1, 71, 90};
	p->notes[1] = (MsNote){110, 12, 70, 80};
	p->notes[2] = (MsNote){330, 12, 69, 70};
	*a->gesture = a->project.song;
	memset(a->history, 0, sizeof(*a->history));
	a->dirty = 0;
	ms_ui_refresh(a);
	/* 上下视图各自以鼠标为锚点，工程、选区和撤销历史不变。 */
	double anchor = (800 - MS_TIMEX) / a->zoom;
	YMGUI_Inject_Key(GY_KEY_CTRL, 1);
	CHECK(a->ctrl);
	YMGUI_Inject_Wheel(800, 260, 0, 1);
	CHECK(a->zoom > old_zoom && a->editor_zoom == 1);
	CHECK(fabs(a->scroll + (800 - MS_TIMEX) / a->zoom - anchor) <= .51);
	double timeline_zoom = a->zoom;
	int timeline_scroll = a->scroll;
	YMGUI_Inject_Wheel(725, 640, 0, 1);
	CHECK(a->editor_zoom == 1.5 && a->octave == 5);
	CHECK(fabs(a->editor_scroll + 325.0 * MS_STEP / 117 - 100) < .001);
	CHECK(a->zoom == timeline_zoom && a->scroll == timeline_scroll);
	YMGUI_Inject_Wheel(725, 640, 0, 20);
	CHECK(a->editor_zoom == 16);
	CHECK(!memcmp(a->gesture, &a->project.song, sizeof(MsSong)) && !a->dirty && !a->history->undos);
	YMGUI_Refresh(a->ctx);
	YMGUI_Inject_Key(GY_KEY_CTRL, 0);
	CHECK(!a->ctrl);
	/* 放大后最短音符能按可见右端拉长，实际增量仍按 tick 计算。 */
	int w = (int)((MS_GRID_WIDTH / 16) * a->editor_zoom);
	int left = MS_GRID_X + (int)floor((100 - a->editor_scroll) * w / MS_STEP);
	int right = left + w / MS_STEP - 1;
	click(left + 2, 640);
	CHECK(a->note == 0 && p->count == 3 && p->notes[0].length == 1);
	box_drag(right, 640, right + w / MS_STEP, 640);
	CHECK(p->notes[0].length == 2 && p->notes[0].tick == 100);
	ms_action(a, A_UNDO);
	CHECK(p->notes[0].length == 1);
	/* 缩放后的创建、框选和整组移动使用同一坐标映射。 */
	int new_tick = (int)floor(a->editor_scroll + 200.0 * MS_STEP / w);
	click(600, 800);
	CHECK(p->count == 4 && p->notes[3].tick == new_tick);
	ms_action(a, A_UNDO);
	ms_action(a, A_NOTE_SELECT);
	box_drag(410, 632, 1600, 720);
	CHECK(ms_notes_selection_count(a) == 2 && !a->note_selected[2]);
	box_drag(left + 2, 640, left + 2 + w / MS_STEP, 640);
	CHECK(p->notes[0].tick == 101 && p->notes[1].tick == 111 && p->notes[2].tick == 330);
	ms_action(a, A_UNDO);
	ms_action(a, A_NOTE_SELECT);
	/* Shift/横向滚轮平移，普通滚轮仍改变八度。 */
	double scroll = a->editor_scroll;
	YMGUI_Inject_Key(GY_KEY_SHIFT, 1);
	YMGUI_Inject_Wheel(900, 800, 0, -2);
	CHECK(a->editor_scroll > scroll && a->octave == 5);
	YMGUI_Inject_Key(GY_KEY_SHIFT, 0);
	scroll = a->editor_scroll;
	YMGUI_Inject_Wheel(900, 800, -1, 0);
	CHECK(a->editor_scroll < scroll);
	YMGUI_Inject_Wheel(900, 800, 0, 1);
	CHECK(a->octave == 6);
	a->octave = 5;
	/* 边界钳制以及忙碌/拖动时 Ctrl 抬起不会卡住。 */
	YMGUI_Inject_Key(GY_KEY_CTRL, 1);
	YMGUI_Inject_Wheel(725, 640, 0, -32);
	CHECK(a->editor_zoom == 1 && a->editor_scroll == 0);
	YMGUI_Inject_Wheel(800, 260, 0, 32);
	CHECK(a->zoom == 12);
	YMGUI_Inject_Wheel(800, 260, 0, -32);
	CHECK(a->zoom == .12 && a->scroll >= 0);
	a->drag = 6;
	YMGUI_Inject_Wheel(725, 640, 0, 3);
	CHECK(a->editor_zoom == 1);
	YMGUI_Inject_Key(GY_KEY_CTRL, 0);
	CHECK(!a->ctrl);
	a->drag = 0;
	/* 鼓机下方同样缩放，点击/力度命中随视口变换。 */
	ms_select_pattern(a, 0);
	MsPattern* drums = &a->project.song.pattern[0];
	MsPattern before = *drums;
	int undos = a->history->undos;
	YMGUI_Inject_Key(GY_KEY_CTRL, 1);
	YMGUI_Inject_Wheel(900, 650, 0, 2);
	YMGUI_Inject_Key(GY_KEY_CTRL, 0);
	CHECK(a->editor_zoom == 2.25 && !memcmp(drums, &before, sizeof(before)));
	CHECK(a->history->undos == undos);
	w = (int)((MS_GRID_WIDTH / drums->steps) * a->editor_zoom);
	int step = (int)floor(a->editor_scroll / MS_STEP + 500.0 / w);
	int previous = drums->drum[0][step];
	click(900, 645);
	CHECK(a->step == step && (drums->drum[0][step] != 0) == !previous);
	ms_action(a, A_UNDO);
	CHECK(!memcmp(drums, &before, sizeof(before)));
	YMGUI_Refresh(a->ctx);
	a->project.song = *original;
	*a->gesture = *original;
	free(original);
	memset(a->history, 0, sizeof(*a->history));
	a->track = a->pattern = a->tab = a->dirty = a->scroll = 0;
	a->editor_zoom = 1;
	a->editor_scroll = 0;
	a->zoom = old_zoom;
	a->note_grid = 0;
	a->note_length = old_length;
	a->note_velocity = old_velocity;
	ms_notes_clear_selection(a);
	ms_ui_refresh(a);
	return 1;
}

int ms_ui_selftest(MsApp* a)
{
	CHECK(ms_song_valid(&a->project.song, a->project.assets));
	CHECK(test_fine_notes(a));
	CHECK(test_box_selection(a));
	CHECK(test_view_zoom(a));
	CHECK(test_library_feedback(a));
	/* 最后一行响指可以试听、写入、撤销，且不会落入力度区。 */
	click(265, 636 + 8 * MS_DRUM_ROW + 8);
	CHECK(a->drum == 8);
	click(MS_GRID_X + 10, 636 + 8 * MS_DRUM_ROW + 8);
	CHECK(a->project.song.pattern[0].drum[8][0] > 0);
	ms_action(a, A_UNDO);
	CHECK(a->project.song.pattern[0].drum[8][0] == 0);
	click(265, 640);
	int before = a->project.song.pattern[0].drum[0][1];
	click(MS_GRID_X + MS_GRID_WIDTH / 16 + 10, 640);
	CHECK(a->project.song.pattern[0].drum[0][1] != before);
	ms_action(a, A_UNDO);
	CHECK(a->project.song.pattern[0].drum[0][1] == before);
	ms_action(a, A_REDO);
	CHECK(a->project.song.pattern[0].drum[0][1] != before);
	int old = a->project.song.clip[0].start;
	YMGUI_Inject_Pointer(MS_TIMEX + 30, MS_TIMEY + 14, 1);
	YMGUI_Inject_Pointer(MS_TIMEX + 94, MS_TIMEY + 14, 1);
	YMGUI_Inject_Pointer(MS_TIMEX + 94, MS_TIMEY + 14, 0);
	CHECK(a->project.song.clip[0].start > old);
	ms_action(a, A_UNDO);
	CHECK(a->project.song.clip[0].start == old);
	int clips = a->project.song.clips;
	a->library_tab = 0;
	YMGUI_Inject_Pointer(45, 192, 1);
	YMGUI_Inject_Pointer(1420, MS_TIMEY + 14, 1);
	YMGUI_Inject_Pointer(1420, MS_TIMEY + 14, 0);
	CHECK(a->project.song.clips == clips + 1);
	ms_action(a, A_UNDO);
	clips = a->project.song.clips;
	YMGUI_Inject_Pointer(45, 192, 1);
	YMGUI_Inject_Pointer(1420, MS_TIMEY + 14, 1);
	YMGUI_Inject_PointerCancel();
	CHECK(a->project.song.clips == clips);
	ms_action(a, A_PIANO);
	CHECK(a->tab == 1 && a->pattern == 1);
	int notes = a->project.song.pattern[1].count;
	click(MS_GRID_X + 70, 633);
	CHECK(a->project.song.pattern[1].count == notes + 1);
	ms_action(a, A_UNDO);
	CHECK(a->project.song.pattern[1].count == notes);
	/* 原有音色仍可选择和撤销；新工程默认采样钢琴。 */
	a->project.song.track[a->track].instrument = 0;
	ms_ui_refresh(a);
	CHECK(YMGUI_Dropdown_GetOptionCount(a->instrument) == MS_INSTRUMENTS);
	click(320, 570);
	CHECK(YMGUI_Dropdown_IsOpen(a->instrument));
	click(320, 586 + 11 * 22 + 10);
	CHECK(!YMGUI_Dropdown_IsOpen(a->instrument));
	CHECK(a->project.song.track[a->track].instrument == 11);
	ms_action(a, A_UNDO);
	CHECK(a->project.song.track[a->track].instrument == 0);
	ms_action(a, A_REDO);
	CHECK(YMGUI_Dropdown_GetSelected(a->instrument) == 11);
	click(320, 570);
	click(320, 586 + 12 * 22 + 10);
	CHECK(a->project.song.track[a->track].instrument == 12);
	ms_action(a, A_UNDO);
	CHECK(a->project.song.track[a->track].instrument == 11);
	int audition_track = ms_track_add(&a->project.song, MS_SYNTH);
	CHECK(audition_track >= 0);
	a->track = audition_track;
	a->project.song.track[audition_track].instrument = 12;
	a->project.song.track[audition_track].mute = 1;
	a->project.song.track[0].solo = 1;
	/* 连续长音隔离供音测试，避免取样电平刚好落在示例旋律的休止处。 */
	MsPattern preview_notes = a->project.song.pattern[a->pattern];
	a->project.song.pattern[a->pattern].count = 1;
	a->project.song.pattern[a->pattern].notes[0] = (MsNote){0, preview_notes.steps * MS_STEP, 67, 100};
	ms_ui_refresh(a);
	click(1370, 570);
	CHECK(a->transport.playing && a->transport.pattern_mode && a->transport.track == audition_track);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(a->transport.meters[audition_track] > 0);
	CHECK(a->transport.meters[0] == 0 && a->transport.meters[1] == 0);
	CHECK(a->project.song.track[audition_track].mute && a->project.song.track[0].solo);
	/* 合唱试听：主线程超过缓冲时长不 tick，重绘和停顿也不能中断供音。 */
	int64_t first_position = ms_transport_position(&a->transport);
	for (int stall = 0; stall < 6; ++stall)
	{
		YMGUI_Obj_Invalidate(a->timeline);
		YMGUI_Obj_Invalidate(a->editor);
		YMGUI_Refresh(a->ctx);
		SDL_Delay(100);
		CHECK(SDL_GetQueuedAudioSize(a->transport.device) > 0);
	}
	CHECK(ms_transport_position(&a->transport) - first_position > MS_RATE / 3);
	/* 渲染读取值快照，主线程改模型后通过 ms_changed 发布新版本。 */
	float master = a->project.song.master;
	a->project.song.master = 0;
	SDL_Delay(100);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(a->transport.meters[audition_track] > 0);
	ms_changed(a);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(a->transport.meters[MS_TRACKS] == 0);
	a->project.song.master = master;
	ms_changed(a);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(a->transport.meters[MS_TRACKS] > 0);
	click(1370, 570);
	CHECK(!a->transport.playing && SDL_GetQueuedAudioSize(a->transport.device) == 0);
	click(600, 570);
	CHECK(SDL_GetQueuedAudioSize(a->transport.device) > 0);
	ms_action(a, A_STOP);
	ms_action(a, A_MODE);
	CHECK(!a->transport.pattern_mode);
	a->project.song.pattern[a->pattern] = preview_notes;
	ms_track_delete(&a->project.song, audition_track);
	a->project.song.track[0].solo = 0;
	a->track = 1;
	ms_ui_refresh(a);
	ms_action(a, A_MIXER);
	float volume = a->project.song.track[0].volume;
	click(274, 720);
	CHECK(fabsf(a->project.song.track[0].volume - volume) > .1f);
	ms_action(a, A_UNDO);
	CHECK(a->project.song.track[0].volume == volume);
	ms_action(a, A_DRUM);
	ms_action(a, A_PLAY);
	CHECK(a->transport.playing);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(SDL_GetQueuedAudioSize(a->transport.device) > 0);
	ms_transport_seek(&a->transport, (int64_t)llround(ms_tick_frame(&a->project.song, a->project.song.loop_end)) - 256);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(SDL_GetQueuedAudioSize(a->transport.device) > 256 * 8);
	ms_action(a, A_STOP);
	CHECK(!a->transport.playing && a->transport.cursor == 0);
	/* 不循环的尾块自然消费结束，不靠 GUI tick 持续补队列。 */
	a->transport.loop = 0;
	int64_t end = (int64_t)llround(ms_tick_frame(&a->project.song, ms_song_end(&a->project.song)));
	ms_transport_seek(&a->transport, end - 256);
	a->transport.playing = 1;
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	SDL_Delay(100);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(!a->transport.playing && !a->transport.playback);
	CHECK(ms_transport_position(&a->transport) == end);
	CHECK(SDL_GetQueuedAudioSize(a->transport.device) == 0);
	a->transport.loop = 1;
	ms_action(a, A_STOP);
	CHECK(ms_song_valid(&a->project.song, a->project.assets));
	ms_action(a, A_PIANO);
	ms_action(a, A_PAT_LENGTH);
	CHECK(a->project.song.pattern[a->pattern].steps == 32);
	ms_note_toggle(&a->project.song.pattern[a->pattern], 20 * MS_STEP, 80, 100, MS_STEP);
	ms_action(a, A_PAT_LENGTH);
	CHECK(ms_song_valid(&a->project.song, a->project.assets));
	ms_action(a, A_DRUM);
	/* 导入音频轨也可独立试听淡化后的片段，不回退到上次的旋律。 */
	for (int i = 0; i < a->project.song.clips; ++i)
		if (a->project.song.track[a->project.song.clip[i].track].kind == MS_AUDIO)
		{
			a->clip = i;
			a->track = a->project.song.clip[i].track;
			break;
		}
	a->tab = 2;
	ms_ui_refresh(a);
	int audio_track = a->track;
	CHECK(a->project.song.track[audio_track].kind == MS_AUDIO);
	ms_action(a, A_PREVIEW);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(a->transport.playing && a->transport.clip == a->clip);
	CHECK(a->transport.meters[audio_track] > 0 && a->transport.meters[0] == 0);
	ms_action(a, A_STOP);
	ms_action(a, A_MODE);
	ms_action(a, A_DRUM);
	/* 文件任务从后台交还主线程，素材与工程在保存重开后仍可播放。 */
	char temp[] = "/tmp/ymgui-music-ui-XXXXXX";
	CHECK(mkdtemp(temp));
	char audio[MS_PATH], project[MS_PATH], error[256];
	snprintf(audio, sizeof(audio), "%s/中文导入.wav", temp);
	snprintf(project, sizeof(project), "%s/中文工程.ymmusic", temp);
	CHECK(ms_export(&a->project, &a->sounds, audio, 0, error, sizeof(error)));
	int assets = a->project.assets;
	ms_job_start(a, A_IMPORT, audio);
	uint32_t deadline = SDL_GetTicks() + 10000;
	while (a->worker && SDL_GetTicks() < deadline)
	{
		ms_job_poll(a);
		SDL_Delay(2);
	}
	CHECK(!a->worker && a->project.assets == assets + 1);
	a->track = 2;
	ms_action(a, A_APPEND);
	CHECK(a->project.song.clip[a->clip].source == assets);
	CHECK(ms_save(a, project));
	/* 播放导入 PCM 时打开工程，必须先等待供音线程再释放旧素材。 */
	ms_action(a, A_PREVIEW);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(a->transport.playback);
	CHECK(!ms_load(a, "/tmp/ymgui-music-missing-dir/missing.ymmusic"));
	CHECK(!a->transport.playing && !a->transport.playback);
	CHECK(a->project.assets == assets + 1);
	ms_action(a, A_PREVIEW);
	ms_transport_tick(&a->transport, &a->project, &a->sounds);
	CHECK(a->transport.playback);
	CHECK(ms_load(a, project));
	CHECK(!a->transport.playing && !a->transport.playback);
	CHECK(a->project.assets == assets + 1);
	unlink(audio);
	unlink(project);
	rmdir(temp);
	a->path[0] = 0;
	a->dirty = 0;
	ms_ui_refresh(a);
	puts("中文鼓机、时间线拖动、素材编排、钢琴输入、混音与播放交互通过");
	return 1;
}
