#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ note_status, note_title_input, note_editor;
	int note_pinned;
} AppState;
static AppState state;

static int app_command(const char* name, const char* argument);
static void note_pin(GYOBJ obj);


static int app_command(const char* name, const char* argument)
{
	if (strcmp(name, "set-body") || !argument)
		return 0;
	YMGUI_EditView_SetText(state.note_editor, argument);
	return 1;
}

static void note_pin(GYOBJ obj)
{
	state.note_pinned = !state.note_pinned;
	PhoneUI_text_set(state.note_status, state.note_pinned ? "已置顶 / 个人" : "个人 / 今天");
	PhoneUI_button_set(obj, state.note_pinned ? "取消置顶" : "置顶笔记");
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "笔记", "记录生活里的小美好");
	GYOBJ note = PhoneUI_panel(view, 18, 62, 284, 263, RGB(255, 245, 212));
	state.note_status = PhoneUI_left_label(note, 18, 10, 248, "个人 / 今天", RGB(151, 121, 53), 2);
	state.note_title_input = YMGUI_Creat_TextInput_Creat(note, 14, 34, 256, 32, 127);
	state.note_title_input->draw_cb = PhoneUI_phone_input_draw;
	YMGUI_TextInput_SetText(state.note_title_input, "周末计划");
	state.note_editor = YMGUI_Creat_EditView_Creat(note, 14, 74, 256, 128, 4096);
	YMGUI_EditView_SetText(state.note_editor, "去湖边散步。\n一杯咖啡，一本书，慢慢来。");
	YMGUI_EditView_SetWrap(state.note_editor, 1);
	YMGUI_EditView_SetPadding(state.note_editor, 6, 5, 6, 5);
	YMGUI_EditView_SetTextColor(state.note_editor, INK);
	YMGUI_EditView_SetBgColor(state.note_editor, RGB(255, 245, 212));
	YMGUI_EditView_SetBorderColor(state.note_editor, RGB(225, 207, 158));
	PhoneUI_left_label(note, 18, 228, 248, "点击编辑 / 本次运行保留", MUTED, 2);
	PhoneUI_button(view, 18, 351, 284, 44, "置顶笔记", note_pin, RGB(196, 149, 52));
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "note_status"))
		return (intptr_t)state.note_status;
	if (!strcmp(name, "note_title_input"))
		return (intptr_t)state.note_title_input;
	if (!strcmp(name, "note_editor"))
		return (intptr_t)state.note_editor;
	if (!strcmp(name, "note_pinned"))
		return (intptr_t)state.note_pinned;
	return 0;
}

const PhoneApp PhoneApp_notes = {
	.key = "notes", .title = "笔记", .color = RGB(225, 173, 69), .icon = ICON_NOTES, .create = app_create, .inspect = app_inspect, .command = app_command, .destroy = app_destroy};
