#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ message_to, message_editor, message_status;
} AppState;
static AppState state;

static void app_show(const char* argument);
static void message_send(GYOBJ obj);

static void app_show(const char* argument)
{
	if (argument)
		YMGUI_TextInput_SetText(state.message_to, argument);
}
static void message_send(GYOBJ obj)
{
	(void)obj;
	PhoneIME_Hide();
	if (!YMGUI_TextInput_GetText(state.message_to)[0] || !YMGUI_EditView_GetText(state.message_editor)[0])
	{
		PhoneUI_text_set(state.message_status, "请填写收件人和短信内容");
		return;
	}
	PhoneUI_text_set(state.message_status, "已模拟发送 · 未使用通信网络");
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "短信", "编辑与发送状态演示 · 不发送短信");
	PhoneUI_left_label(view, 24, 65, 272, "收件人", MUTED, 2);
	state.message_to = PhoneUI_app_input(view, 24, 88, 272, "13800000001", 95);
	state.message_editor = YMGUI_Creat_EditView_Creat(view, 24, 135, 272, 163, 512);
	YMGUI_EditView_SetText(state.message_editor, "今天一起去散步吗？");
	YMGUI_EditView_SetWrap(state.message_editor, 1);
	YMGUI_EditView_SetPadding(state.message_editor, 8, 8, 8, 8);
	YMGUI_EditView_SetBgColor(state.message_editor, WHITE);
	YMGUI_EditView_SetTextColor(state.message_editor, INK);
	state.message_status = PhoneUI_label_ex(view, 16, 310, 288, "草稿仅在本次运行保留", MUTED, 2);
	PhoneUI_button(view, 24, 351, 272, 43, "模拟发送", message_send, PhoneApps_Get(MESSAGES)->color);
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "message_to"))
		return (intptr_t)state.message_to;
	if (!strcmp(name, "message_editor"))
		return (intptr_t)state.message_editor;
	if (!strcmp(name, "message_status"))
		return (intptr_t)state.message_status;
	return 0;
}

const PhoneApp PhoneApp_messages = {
	.key = "messages", .title = "短信", .color = RGB(57, 153, 221), .icon = ICON_MESSAGES, .create = app_create, .inspect = app_inspect, .show = app_show, .destroy = app_destroy};
