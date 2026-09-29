#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ task_boxes[5], task_add_button, task_progress, task_summary;
	int task_count;
} AppState;
static AppState state;

static void check_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area);
static void check_changed(GYOBJ obj, uint8 checked);
static GYOBJ checkbox(GYOBJ parent, const char* value);
static void update_task_count(void);
static void on_add_task(GYOBJ btn);

static const char* task_names[5] = {"计划一次小旅行", "画一点新想法", "停下来休息片刻", "发现一张新歌单", "去看一场日落"};
static void check_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	int id = 0;
	for (int i = 0; i < 5; ++i)
		if (state.task_boxes[i] == obj)
			id = i;
	int checked = YMGUI_Checkbox_GetChecked(obj);
	GYcolor tint = checked ? PhoneApps_Get(TASKS)->color : RGB(206, 212, 223);
	int cx = area->x + 15, cy = area->y + area->h / 2;
	YMGUI_Draw_CircleFill(surface, cx, cy, 10, tint);
	if (checked)
	{
		YMGUI_Draw_Line(surface, cx - 5, cy, cx - 1, cy + 4, WHITE);
		YMGUI_Draw_Line(surface, cx - 1, cy + 4, cx + 6, cy - 5, WHITE);
	}
	else
		YMGUI_Draw_CircleFill(surface, cx, cy, 8, WHITE);
	PhoneUI_draw_text(surface, task_names[id], checked ? MUTED : INK, 0,
					  area->x + 34 + PhoneUI_text_width(task_names[id], 0) / 2, area->y + (area->h - 24) / 2);
	if (checked)
		YMGUI_Draw_Line(surface, area->x + 34, cy,
						area->x + 34 + PhoneUI_text_width(task_names[id], 0), cy, MUTED);
}

static void check_changed(GYOBJ obj, uint8 checked)
{
	(void)obj;
	(void)checked;
	update_task_count();
}

static GYOBJ checkbox(GYOBJ parent, const char* value)
{
	GYOBJ obj = YMGUI_Creat_Checkbox_Creat(parent, 9, 3, 264, 38);
	YMGUI_Checkbox_SetText(obj, value);
	YMGUI_Checkbox_SetChanged(obj, check_changed);
	obj->draw_cb = check_draw;
	return obj;
}

static void update_task_count(void)
{
	char text[32];
	int done = 0;
	for (int i = 0; i < state.task_count; ++i)
		done += YMGUI_Checkbox_GetChecked(state.task_boxes[i]);
	PhoneUI_button_set(state.task_add_button, state.task_count < 5 ? "+  添加任务" : "任务已全部添加");
	snprintf(text, sizeof(text), "已完成 %d / %d 项", done, state.task_count);
	PhoneUI_text_set(state.task_summary, text);
	PhoneUI_animate_bar(state.task_progress, done * 100 / state.task_count, 280);
}

static void on_add_task(GYOBJ btn)
{
	(void)btn;
	if (state.task_count >= 5)
		return;
	GYOBJ row = state.task_boxes[state.task_count]->parent;
	YMGUI_Obj_SetHidden(row, 0);
	int final_y = row->area.y;
	PhoneUI_set_pos(row, 34, final_y + 8);
	PhoneUI_move(row, 18, final_y, 220);
	++state.task_count;
	update_task_count();
}
static void app_create(GYOBJ view)
{
	state = (AppState){.task_count = 3};
	PhoneUI_app_header(view, "待办", "9月29日  星期二");
	GYOBJ progress = PhoneUI_panel(view, 18, 66, 284, 62, RGB(229, 242, 238));
	state.task_summary = PhoneUI_left_label(progress, 14, 7, 254, "已完成 0 / 3 项", RGB(44, 126, 106), 0);
	state.task_progress = YMGUI_Creat_Bar_Creat(progress, 15, 40, 254, 4);
	YMGUI_Bar_SetRange(state.task_progress, 0, 100);
	YMGUI_Bar_SetColors(state.task_progress, RGB(197, 224, 215), PhoneApps_Get(TASKS)->color);
	for (int i = 0; i < 5; ++i)
	{
		GYOBJ row = PhoneUI_panel(view, 18, 140 + i * 45, 284, 43, WHITE);
		state.task_boxes[i] = checkbox(row, task_names[i]);
		if (i >= state.task_count)
			YMGUI_Obj_SetHidden(row, 1);
	}
	state.task_add_button = PhoneUI_button(view, 18, 370, 284, 36, "+  添加任务", on_add_task, PhoneApps_Get(TASKS)->color);
	update_task_count();
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "task_boxes"))
		return index >= 0 && index < 5 ? (intptr_t)state.task_boxes[index] : 0;
	if (!strcmp(name, "task_add_button"))
		return (intptr_t)state.task_add_button;
	if (!strcmp(name, "task_progress"))
		return (intptr_t)state.task_progress;
	if (!strcmp(name, "task_summary"))
		return (intptr_t)state.task_summary;
	if (!strcmp(name, "task_count"))
		return (intptr_t)state.task_count;
	return 0;
}

const PhoneApp PhoneApp_tasks = {
	.key = "tasks", .title = "待办", .color = RGB(53, 172, 146), .icon = ICON_TASK, .create = app_create, .inspect = app_inspect, .destroy = app_destroy};
