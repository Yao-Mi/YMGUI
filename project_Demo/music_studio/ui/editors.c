#include "app/studio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 编辑偏好不进入工程；自由模式及关闭吸附均按模型最小 tick 调整。 */
int ms_piano_grid_ticks(int index)
{
	static const int ticks[] = {24, 12, 6, 32, 16, 8, 1};
	return index >= 0 && index < 7 ? ticks[index] : MS_STEP;
}
int ms_piano_step(const MsApp* a)
{
	return a->snap ? ms_piano_grid_ticks(a->note_grid) : 1;
}
void ms_notes_clear_selection(MsApp* a)
{
	memset(a->note_selected, 0, sizeof(a->note_selected));
	a->note = -1;
	a->selection_pattern = a->pattern;
}
int ms_note_selected(const MsApp* a, int index)
{
	return index >= 0 && index < MS_NOTES &&
		   (a->select_notes ? a->selection_pattern == a->pattern && a->note_selected[index] : index == a->note);
}
int ms_notes_selection_count(const MsApp* a)
{
	int count = 0;
	if (a->pattern >= 0 && a->pattern < a->project.song.patterns)
		for (int i = 0; i < a->project.song.pattern[a->pattern].count; ++i)
			count += ms_note_selected(a, i) != 0;
	return count;
}
static int note_width(const MsNote* note, int w)
{
	int width = note->length * w / MS_STEP;
	return width < 3 ? 3 : width;
}
static int grid_width(const MsApp* a, const MsPattern* p)
{
	return (int)((MS_GRID_WIDTH / p->steps) * fmax(1, a->editor_zoom));
}
static int grid_pixel(const MsApp* a, int tick, int w)
{
	return MS_GRID_X + (int)floor((tick - a->editor_scroll) * w / MS_STEP);
}
void ms_editor_view_clamp(MsApp* a)
{
	a->editor_zoom = fmax(1, fmin(16, a->editor_zoom));
	if (a->pattern < 0 || a->pattern >= a->project.song.patterns)
	{
		a->editor_scroll = 0;
		return;
	}
	const MsPattern* p = &a->project.song.pattern[a->pattern];
	double end = fmax(0, p->steps * MS_STEP - (double)MS_GRID_WIDTH * MS_STEP / grid_width(a, p));
	a->editor_scroll = fmax(0, fmin(end, a->editor_scroll));
}
static GYsurface grid_surface(GYSURFACE s)
{
	GYsurface grid = *s;
	int left = s->clip.x > MS_GRID_X ? s->clip.x : MS_GRID_X;
	int right = s->clip.x + s->clip.w;
	if (right > MS_GRID_X + MS_GRID_WIDTH)
		right = MS_GRID_X + MS_GRID_WIDTH;
	grid.clip.x = left;
	grid.clip.w = right > left ? right - left : 0;
	return grid;
}
static void editor_wheel_view(MsApp* a, MsPattern* p, int x, int dx, int dy)
{
	int w = grid_width(a, p);
	double anchor = ms_clamp(x - MS_GRID_X, 0, MS_GRID_WIDTH - 1);
	if (a->ctrl)
	{
		double tick = a->editor_scroll + anchor * MS_STEP / w;
		a->editor_zoom = fmax(1, fmin(16, a->editor_zoom * pow(1.5, ms_clamp(dy, -32, 32))));
		a->editor_scroll = tick - anchor * MS_STEP / grid_width(a, p);
	}
	else
		a->editor_scroll += ((double)dx - dy) * MS_GRID_WIDTH * MS_STEP / (w * 8.0);
	ms_editor_view_clamp(a);
	YMGUI_Obj_Invalidate(a->editor);
	char text[160];
	snprintf(text, sizeof(text), "编辑区缩放 %.0f%% · Ctrl＋滚轮缩放 · Shift＋滚轮左右移动 · 实际时长不变", a->editor_zoom * 100);
	ms_status(a, text);
}
static MsPattern* selected(MsApp* a)
{
	return a->pattern >= 0 && a->pattern < a->project.song.patterns ? &a->project.song.pattern[a->pattern] : NULL;
}
static void draw_drums(MsApp* a, GYSURFACE s, MsPattern* p)
{
	int w = grid_width(a, p);
	GYsurface grid = grid_surface(s);
	char b[64];
	ms_text(s, 244, 610, "鼓声 / 音量", MS_DIM);
	for (int j = 0; j < p->steps; ++j)
	{
		snprintf(b, sizeof(b), "%d", j + 1);
		ms_text(&grid, grid_pixel(a, j * MS_STEP, w) + 6, 610, b, j % 4 ? MS_DIM : MS_ACCENT);
	}
	int play = (int)(ms_frame_tick(&a->project.song, ms_transport_position(&a->transport)) / MS_STEP) % p->steps;
	for (int d = 0; d < MS_DRUMS; ++d)
	{
		int y = 636 + d * MS_DRUM_ROW;
		ms_fill(s, 240, y, 154, MS_DRUM_ROW - 2, a->drum == d ? 0x3f514d : 0x30363c);
		snprintf(b, sizeof(b), "%s %d%%", ms_drum_name(d), (int)(a->project.song.drum_volume[d] * 100));
		ms_text(s, 248, y + 5, b, MS_TEXT);
		if (a->project.song.drum_mute[d])
			ms_fill(s, 382, y + 5, 5, 5, 0xd5a35d);
		if (a->project.song.drum_solo[d])
			ms_fill(s, 382, y + 15, 5, 5, MS_ACCENT);
		for (int j = 0; j < p->steps; ++j)
		{
			int v = p->drum[d][j];
			unsigned c = v ? 0xbd9056 : (j / 4) % 2 ? 0x384049
													: 0x303740;
			ms_fill(&grid, grid_pixel(a, j * MS_STEP, w) + 2, y + 2, w - 4, MS_DRUM_ROW - 6, c);
			if (v)
				ms_fill(&grid, grid_pixel(a, j * MS_STEP, w) + 5, y + 15 - (v * 10 / 127), w - 10, v * 10 / 127 + 2, 0xefc383);
			if (a->drum == d && a->step == j)
				ms_border(&grid, grid_pixel(a, j * MS_STEP, w) + 1, y + 1, w - 2, MS_DRUM_ROW - 4, 0xf9e3bd);
			if (a->transport.playing && play == j)
				ms_fill(&grid, grid_pixel(a, j * MS_STEP, w) + 2, y + 2, w - 4, 2, MS_ACCENT);
		}
	}
	ms_text(s, 244, 972, "力度", MS_ACCENT);
	ms_text(s, 244, 990, "拖动下方柱条", MS_DIM);
	for (int j = 0; j < p->steps; ++j)
	{
		int v = p->drum[a->drum][j];
		int h = v * 37 / 127;
		ms_fill(&grid, grid_pixel(a, j * MS_STEP, w) + 2, 966, w - 4, 40, 0x22272c);
		ms_fill(&grid, grid_pixel(a, j * MS_STEP, w) + 4, 1005 - h, w - 8, h, v ? 0x72ad9b : 0x343c44);
	}
}
static void draw_piano(MsApp* a, GYSURFACE s, MsPattern* p)
{
	char b[128];
	int w = grid_width(a, p);
	int length = a->note >= 0 && a->note < p->count ? p->notes[a->note].length : a->note_length;
	snprintf(b, sizeof(b), "音长 %.4g 拍 · %s · Ctrl＋滚轮缩放 · 右键删除", length / (double)MS_PPQ,
			 ms_piano_step(a) == 1 ? "自由细调" : "按步长吸附");
	if (a->select_notes)
		snprintf(b, sizeof(b), "框选模式 · 已选 %d 个 · 拖动空白框选 · Ctrl＋滚轮缩放", ms_notes_selection_count(a));
	ms_text(s, 244, 608, b, a->select_notes ? MS_ACCENT : MS_DIM);
	for (int row = 0; row < 12; ++row)
	{
		int pitch = a->octave * 12 + 11 - row, y = 630 + row * MS_KEY_ROW;
		int pc = pitch % 12, black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
		ms_fill(s, 240, y, 154, MS_KEY_ROW - 1, black ? 0x292c32 : 0x9aabb4);
		static const char* names[] = {"哆", "升哆", "来", "升来", "咪", "发", "升发", "嗦", "升嗦", "啦", "升啦", "西"};
		snprintf(b, sizeof(b), "%s %d", names[pc], pitch / 12 - 1);
		ms_text(s, 248, y + 3, b, black ? MS_TEXT : 0x1e252c);
		ms_fill(s, MS_GRID_X, y, MS_GRID_WIDTH, MS_KEY_ROW - 1, black ? 0x262c33 : 0x30373e);
	}
	GYsurface grid = grid_surface(s);
	s = &grid;
	int spacing = ms_piano_step(a);
	if (spacing == 1)
		spacing = MS_STEP; /* 自由编辑仍保留可读的节拍参考线。 */
	for (int tick = 0; tick <= p->steps * MS_STEP; tick += spacing)
		ms_fill(s, grid_pixel(a, tick, w), 630, 1, 12 * MS_KEY_ROW, 0x3b444d);
	for (int tick = 0; tick <= p->steps * MS_STEP; tick += MS_PPQ)
		ms_fill(s, grid_pixel(a, tick, w), 630, 1, 12 * MS_KEY_ROW, 0x58636e);
	for (int i = 0; i < p->count; ++i)
	{
		MsNote* n = &p->notes[i];
		int row = a->octave * 12 + 11 - n->pitch;
		if (row < 0 || row >= 12)
			continue;
		int x = grid_pixel(a, n->tick, w), y = 630 + row * MS_KEY_ROW, width = note_width(n, w);
		ms_fill(s, x, y + 1, width, MS_KEY_ROW - 2, ms_note_selected(a, i) ? 0xcbdcc1 : 0x68a992);
		ms_fill(s, x + 2, y + MS_KEY_ROW - 5, (width - 4) * n->velocity / 127, 2, 0x39765f);
	}
	if (a->drag == 7)
	{
		int left = a->box_x < a->drag_x ? a->box_x : a->drag_x;
		int top = a->box_y < a->drag_y ? a->box_y : a->drag_y;
		int width = abs(a->box_x - a->drag_x) + 1, height = abs(a->box_y - a->drag_y) + 1;
		ms_border(s, left, top, width, height, MS_ACCENT);
	}
	if (a->transport.playing)
	{
		double t = ms_frame_tick(&a->project.song, ms_transport_position(&a->transport));
		int pos = (int)t % (p->steps * MS_STEP);
		ms_fill(s, grid_pixel(a, pos, w), 630, 2, 12 * MS_KEY_ROW, 0xeb887d);
	}
}
static void draw_mixer(MsApp* a, GYSURFACE s)
{
	MsSong* song = &a->project.song;
	char b[80];
	for (int t = 0; t < song->tracks; ++t)
	{
		int x = 242 + t * 176;
		MsTrack* tr = &song->track[t];
		ms_fill(s, x, 608, 166, 392, t == a->track ? 0x35413f : 0x30363d);
		ms_fill(s, x, 608, 166, 4, tr->color);
		GYsurface name = *s;
		name.clip.x = x + 5;
		name.clip.w = 156;
		ms_text(&name, x + 8, 623, tr->name, MS_TEXT);
		ms_fill(s, x + 10, 650, 48, 24, tr->mute ? 0x957049 : 0x424a52);
		ms_text(s, x + 18, 654, "静音", MS_TEXT);
		ms_fill(s, x + 72, 650, 48, 24, tr->solo ? 0x508469 : 0x424a52);
		ms_text(s, x + 80, 654, "独奏", MS_TEXT);
		ms_fill(s, x + 33, 693, 9, 146, 0x20252a);
		int fader = 839 - (int)(tr->volume / 1.5f * 146);
		ms_fill(s, x + 20, fader - 4, 35, 10, 0xc0cacd);
		ms_fill(s, x + 88, 693, 16, 146, 0x1e2429);
		float level = a->transport.meters[t];
		int h = ms_clamp((int)((20 * log10f(fmaxf(level, .001f)) + 60) / 60 * 146), 0, 146);
		ms_fill(s, x + 90, 839 - h, 12, h, level > .98f ? 0xe57b68 : MS_ACCENT);
		snprintf(b, sizeof(b), "%d%%", (int)(tr->volume * 100));
		ms_text(s, x + 10, 848, b, MS_TEXT);
		snprintf(b, sizeof(b), "声像 %d", (int)(tr->pan * 100));
		ms_text(s, x + 10, 871, b, MS_DIM);
		ms_fill(s, x + 84, 875, 40, 3, 0x65727d);
		ms_fill(s, x + 102 + (int)(tr->pan * 19), 871, 4, 10, MS_ACCENT);
		ms_text(s, x + 10, 920, "当前音源", MS_DIM);
		ms_text(&name, x + 10, 946, tr->kind == MS_SYNTH ? ms_instrument_names[tr->instrument] : tr->kind == MS_DRUM ? "九声部鼓组"
																													 : "音频素材",
				MS_TEXT);
	}
	if (!song->tracks)
		ms_text(s, 252, 638, "请先添加轨道", MS_DIM);
}
void ms_editor_draw(GYOBJ o, GYSURFACE s, const GYrect* r)
{
	MsApp* a = o->user_data;
	ms_fill(s, r->x, r->y, r->w, r->h, 0x272c32);
	MsPattern* p = selected(a);
	if (a->tab == 2)
		draw_mixer(a, s);
	else if (!p || p->kind != a->tab)
		ms_text(s, 252, 630, a->tab == 0 ? "请在左侧选择鼓机节奏型，或点击新建片段" : "请在左侧选择旋律片段，或点击新建片段", MS_DIM);
	else if (!a->tab)
		draw_drums(a, s, p);
	else
		draw_piano(a, s, p);
}
static int hit_note(MsApp* a, MsPattern* p, int x, int y)
{
	int w = grid_width(a, p), pitch = a->octave * 12 + 11 - (y - 630) / MS_KEY_ROW;
	for (int i = p->count - 1; i >= 0; --i)
	{
		MsNote* n = &p->notes[i];
		int left = grid_pixel(a, n->tick, w), right = left + note_width(n, w);
		if (n->pitch == pitch && x >= left && x < right)
			return i;
	}
	return -1;
}
static void mixer_update(MsApp* a, int x, int y)
{
	MsTrack* t = &a->project.song.track[a->track];
	int left = 242 + a->track * 176;
	if (a->drag_mode == 4)
		t->volume = ms_clamp(839 - y, 0, 146) * 1.5f / 146;
	if (a->drag_mode == 5)
		t->pan = ms_clamp(x - left - 104, -20, 20) / 20.f;
	a->dirty = 1;
	ms_transport_seek(&a->transport, ms_transport_position(&a->transport));
	YMGUI_Obj_Invalidate(a->editor);
}
static void box_select(MsApp* a, MsPattern* p, int x, int y)
{
	a->box_x = ms_clamp(x, MS_GRID_X, MS_GRID_X + MS_GRID_WIDTH - 1);
	a->box_y = ms_clamp(y, 630, 630 + 12 * MS_KEY_ROW - 1);
	int left = a->box_x < a->drag_x ? a->box_x : a->drag_x;
	int top = a->box_y < a->drag_y ? a->box_y : a->drag_y;
	int right = a->box_x > a->drag_x ? a->box_x : a->drag_x;
	int bottom = a->box_y > a->drag_y ? a->box_y : a->drag_y;
	int w = grid_width(a, p);
	ms_notes_clear_selection(a);
	for (int i = 0; i < p->count; ++i)
	{
		MsNote* n = &p->notes[i];
		int row = a->octave * 12 + 11 - n->pitch;
		int nx = grid_pixel(a, n->tick, w), ny = 630 + row * MS_KEY_ROW + 1;
		if (row >= 0 && row < 12 && left < nx + note_width(n, w) && right >= nx &&
			top < ny + MS_KEY_ROW - 2 && bottom >= ny)
		{
			a->note_selected[i] = 1;
			a->note = i;
		}
	}
}
static void drag_selected_notes(MsApp* a, MsPattern* p, int delta, int pitch_delta)
{
	const MsPattern* original = &a->gesture->pattern[a->pattern];
	int lo = -p->steps * MS_STEP, hi = p->steps * MS_STEP, pitch_lo = -127, pitch_hi = 127;
	/* 统一约束整个选区，撞到边界时保持相对时序、音高和长度差。 */
	for (int i = 0; i < p->count; ++i)
		if (ms_note_selected(a, i))
		{
			const MsNote* n = &original->notes[i];
			int minimum = a->drag_mode == 2 ? 1 - n->length : -n->tick;
			int maximum = p->steps * MS_STEP - n->tick - n->length;
			if (minimum > lo)
				lo = minimum;
			if (maximum < hi)
				hi = maximum;
			if (-n->pitch > pitch_lo)
				pitch_lo = -n->pitch;
			if (127 - n->pitch < pitch_hi)
				pitch_hi = 127 - n->pitch;
		}
	delta = ms_clamp(delta, lo, hi);
	pitch_delta = ms_clamp(pitch_delta, pitch_lo, pitch_hi);
	for (int i = 0; i < p->count; ++i)
		if (ms_note_selected(a, i))
		{
			MsNote n = original->notes[i];
			if (a->drag_mode == 2)
				n.length += delta;
			else
			{
				n.tick += delta;
				n.pitch += pitch_delta;
			}
			p->notes[i] = n;
		}
}
void ms_editor_event(GYOBJ o, GYEvent e)
{
	MsApp* a = o->user_data;
	if (a->worker)
		return;
	MsPattern* p = selected(a);
	int x = o->ctx->point_x, y = o->ctx->point_y;
	if (a->tab == 2)
	{
		int t = (x - 242) / 176;
		if (e == GY_EVENT_Pressed && t >= 0 && t < a->project.song.tracks)
		{
			a->track = t;
			int local = x - 242 - t * 176;
			*a->gesture = a->project.song;
			if (y >= 650 && y < 675)
			{
				ms_checkpoint(a);
				if (local < 65)
					a->project.song.track[t].mute ^= 1;
				else
					a->project.song.track[t].solo ^= 1;
				ms_changed(a);
			}
			else if (y >= 688 && y < 842 && local < 72)
			{
				a->drag = 4;
				a->drag_mode = 4;
				mixer_update(a, x, y);
			}
			else if (y >= 865)
			{
				a->drag = 4;
				a->drag_mode = 5;
				mixer_update(a, x, y);
			}
		}
		else if (e == GY_EVENT_Pressing && a->drag == 4)
			mixer_update(a, x, y);
		else if ((e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff) && a->drag == 4)
		{
			if (e == GY_EVENT_ReleasedOff)
				a->project.song = *a->gesture;
			else
				ms_history_push(a->history, a->gesture);
			a->drag = 0;
			ms_changed(a);
		}
		return;
	}
	if (!p || p->kind != a->tab)
		return;
	int w = grid_width(a, p), step = ms_clamp((int)floor(a->editor_scroll / MS_STEP + (double)(x - MS_GRID_X) / w), 0, p->steps - 1);
	if (e == GY_EVENT_Wheel && (a->ctrl || a->shift || o->ctx->wheel_x))
	{
		if (!a->drag)
			editor_wheel_view(a, p, x, o->ctx->wheel_x, o->ctx->wheel_y);
		return;
	}
	if (a->tab == 0)
	{
		int drum = ms_clamp((y - 636) / MS_DRUM_ROW, 0, MS_DRUMS - 1);
		if (e == GY_EVENT_Wheel)
		{
			ms_checkpoint(a);
			if (x < MS_GRID_X)
				a->project.song.drum_volume[drum] = ms_clamp((int)(a->project.song.drum_volume[drum] * 100) + o->ctx->wheel_y * 5, 0, 150) / 100.f;
			else if (p->drum[drum][step])
				p->drum[drum][step] = ms_clamp(p->drum[drum][step] + o->ctx->wheel_y * 5, 1, 127);
			ms_changed(a);
			return;
		}
		if (e == GY_EVENT_Pressed && y >= 636)
		{
			if (y < 960)
			{
				a->drum = drum;
				if (x >= MS_GRID_X)
				{
					ms_checkpoint(a);
					a->step = step;
					p->drum[drum][step] = p->drum[drum][step] ? 0 : a->note_velocity;
					ms_changed(a);
				}
				ms_audition(&a->transport, &a->project, &a->sounds, drum, 0);
				ms_ui_refresh(a);
				ms_status(a, "点击网格开关鼓点；滚轮调力度，鼓名上滚轮调音量；点击鼓名试听");
			}
			else if (x >= MS_GRID_X)
			{
				*a->gesture = a->project.song;
				a->drag = 5;
				a->step = step;
			}
		}
		if ((e == GY_EVENT_Pressed || e == GY_EVENT_Pressing) && a->drag == 5)
		{
			a->step = step;
			a->note_velocity = ms_clamp((1005 - y) * 127 / 37, 1, 127);
			p->drum[a->drum][step] = a->note_velocity;
			YMGUI_Obj_Invalidate(o);
		}
		if ((e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff) && a->drag == 5)
		{
			if (e == GY_EVENT_ReleasedOff)
				a->project.song = *a->gesture;
			else
				ms_history_push(a->history, a->gesture);
			a->drag = 0;
			ms_changed(a);
		}
		return;
	}
	if (e == GY_EVENT_Wheel && !a->drag)
	{
		a->octave = ms_clamp(a->octave + o->ctx->wheel_y, 0, 9);
		YMGUI_Obj_Invalidate(o);
		return;
	}
	if (e == GY_EVENT_ContextRequested && x >= MS_GRID_X && y >= 630)
	{
		int i = hit_note(a, p, x, y);
		if (i >= 0)
		{
			ms_checkpoint(a);
			MsNote n = p->notes[i];
			ms_note_toggle(p, n.tick, n.pitch, n.velocity, n.length);
			ms_notes_clear_selection(a);
			ms_changed(a);
		}
		return;
	}
	if (e == GY_EVENT_Pressed && y >= 630 && y < 630 + 12 * MS_KEY_ROW)
	{
		int pitch = ms_clamp(a->octave * 12 + 11 - (y - 630) / MS_KEY_ROW, 0, 127);
		if (x < MS_GRID_X)
		{
			ms_audition(&a->transport, &a->project, &a->sounds, -1, pitch);
			return;
		}
		int hit = hit_note(a, p, x, y);
		if (a->select_notes && hit < 0)
		{
			memcpy(a->selection_before, a->note_selected, sizeof(a->selection_before));
			a->drag_index = a->note;
			a->drag = 7;
			a->drag_x = ms_clamp(x, MS_GRID_X, MS_GRID_X + MS_GRID_WIDTH - 1);
			a->drag_y = y;
			box_select(a, p, x, y);
			YMGUI_Obj_Invalidate(o);
			return;
		}
		if (a->select_notes && !ms_note_selected(a, hit))
		{
			ms_notes_clear_selection(a);
			a->note_selected[hit] = 1;
		}
		a->note = hit;
		*a->gesture = a->project.song;
		if (a->note < 0)
		{
			int quantum = ms_piano_step(a);
			int tick = (int)floor((a->editor_scroll + (double)(x - MS_GRID_X) * MS_STEP / w) / quantum) * quantum;
			tick = ms_clamp(tick, 0, p->steps * MS_STEP - 1);
			a->note = ms_note_toggle(p, tick, pitch, a->note_velocity, a->note_length);
		}
		if (a->note >= 0)
		{
			a->drag = 6;
			a->drag_x = x;
			a->drag_y = y;
			a->drag_note = p->notes[a->note];
			int width = note_width(&a->drag_note, w);
			int right = grid_pixel(a, a->drag_note.tick, w) + width;
			int handle = ms_clamp(width / 3, 1, 6);
			a->drag_mode = x >= right - handle ? 2 : 0;
			a->note_velocity = p->notes[a->note].velocity;
			ms_audition(&a->transport, &a->project, &a->sounds, -1, pitch);
		}
		YMGUI_Obj_Invalidate(o);
	}
	else if (e == GY_EVENT_Pressing && a->drag == 7)
	{
		box_select(a, p, x, y);
		YMGUI_Obj_Invalidate(o);
	}
	else if ((e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff) && a->drag == 7)
	{
		if (o->ctx->pressed_obj != o)
		{
			memcpy(a->note_selected, a->selection_before, sizeof(a->note_selected));
			a->note = a->drag_index;
		}
		else
			box_select(a, p, x, y);
		a->drag = 0;
		if (a->note >= 0)
			a->note_velocity = p->notes[a->note].velocity;
		ms_ui_refresh(a);
	}
	else if (e == GY_EVENT_Pressing && a->drag == 6 && a->note >= 0)
	{
		MsNote n = a->drag_note;
		int quantum = ms_piano_step(a);
		int delta = (int)lround((double)(x - a->drag_x) * MS_STEP / (w * quantum)) * quantum;
		if (a->select_notes)
		{
			drag_selected_notes(a, p, delta, -(y - a->drag_y) / MS_KEY_ROW);
			YMGUI_Obj_Invalidate(o);
			return;
		}
		if (a->drag_mode == 2)
			n.length = ms_clamp(n.length + delta, 1, p->steps * MS_STEP - n.tick);
		else
		{
			n.tick = ms_clamp(n.tick + delta, 0, p->steps * MS_STEP - n.length);
			n.pitch = ms_clamp(n.pitch - (y - a->drag_y) / MS_KEY_ROW, 0, 127);
		}
		p->notes[a->note] = n;
		YMGUI_Obj_Invalidate(o);
	}
	else if ((e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff) && a->drag == 6)
	{
		int changed = e == GY_EVENT_Released && memcmp(a->gesture, &a->project.song, sizeof(MsSong));
		if (e == GY_EVENT_ReleasedOff)
			a->project.song = *a->gesture;
		else if (changed)
			ms_history_push(a->history, a->gesture);
		a->drag = 0;
		if (changed)
			ms_changed(a);
		else
			ms_ui_refresh(a);
	}
}
