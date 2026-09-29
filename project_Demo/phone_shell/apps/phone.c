#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ dial_input, dial_status, dial_button;
	int call_active, call_ms;
} AppState;
static AppState state;

static void app_show(const char* argument);
static void app_tick(uint32 elapsed);
static void app_close(void);
static void dial_digit(GYOBJ obj);
static void dial_delete(GYOBJ obj);
static void dial_call(GYOBJ obj);


static void app_show(const char* argument)
{
	if (argument)
		YMGUI_TextInput_SetText(state.dial_input, argument);
}
static void app_tick(uint32 elapsed)
{
	if (state.call_active)
	{
		state.call_ms = (state.call_ms + elapsed) % 3600000;
		char text[96];
		snprintf(text, sizeof(text), "演示通话 %02d:%02d · 未拨出", state.call_ms / 60000, state.call_ms / 1000 % 60);
		PhoneUI_text_set(state.dial_status, text);
	}
}
static void app_close(void)
{
	if (!state.call_active)
		return;
	state.call_active = 0;
	PhoneUI_button_set(state.dial_button, "模拟拨号");
	PhoneUI_text_set(state.dial_status, "演示已停止 · 未拨出电话");
}

static void dial_digit(GYOBJ obj)
{
	const char* key = ((PhoneButton*)obj->user_data)->value;
	char text[32];
	snprintf(text, sizeof(text), "%s", YMGUI_TextInput_GetText(state.dial_input));
	size_t n = strlen(text);
	if (n < 20)
	{
		text[n] = key[0];
		text[n + 1] = 0;
	}
	YMGUI_TextInput_SetText(state.dial_input, text);
}

static void dial_delete(GYOBJ obj)
{
	(void)obj;
	char text[32];
	snprintf(text, sizeof(text), "%s", YMGUI_TextInput_GetText(state.dial_input));
	size_t n = strlen(text);
	if (n)
	{
		--n;
		while (n && ((text[n] & 0xc0) == 0x80))
			--n;
		text[n] = 0;
	}
	YMGUI_TextInput_SetText(state.dial_input, text);
}

static void dial_call(GYOBJ obj)
{
	(void)obj;
	PhoneIME_Hide();
	const char* number = YMGUI_TextInput_GetText(state.dial_input);
	if (!state.call_active)
	{
		if (!number[0] || strspn(number, "0123456789+*#") != strlen(number))
		{
			PhoneUI_text_set(state.dial_status, "请输入有效号码（仅演示）");
			return;
		}
		state.call_ms = 0;
	}
	state.call_active = !state.call_active;
	PhoneUI_button_set(state.dial_button, state.call_active ? "挂断演示" : "模拟拨号");
	PhoneUI_text_set(state.dial_status, state.call_active ? "演示通话 00:00 · 未拨出" : "演示已结束 · 未拨出电话");
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "电话", "本地拨号演示 · 不会呼出电话");
	state.dial_input = PhoneUI_app_input(view, 24, 66, 226, "", 24);
	PhoneUI_button(view, 257, 66, 40, 34, "删", dial_delete, RGB(127, 137, 158));
	state.dial_status = PhoneUI_label_ex(view, 18, 106, 284, "输入号码后体验拨号界面", MUTED, 2);
	static const char* dial_keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
	for (int i = 0; i < 12; ++i)
		PhoneUI_button(view, 24 + i % 3 * 94, 137 + i / 3 * 50, 84, 42, dial_keys[i], dial_digit, RGB(108, 137, 155));
	state.dial_button = PhoneUI_button(view, 24, 350, 272, 43, "模拟拨号", dial_call, PhoneApps_Get(PHONE)->color);
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "dial_input"))
		return (intptr_t)state.dial_input;
	if (!strcmp(name, "dial_status"))
		return (intptr_t)state.dial_status;
	if (!strcmp(name, "dial_button"))
		return (intptr_t)state.dial_button;
	if (!strcmp(name, "call_active"))
		return (intptr_t)state.call_active;
	if (!strcmp(name, "call_ms"))
		return (intptr_t)state.call_ms;
	return 0;
}

const PhoneApp PhoneApp_phone = {
	.key = "phone", .title = "电话", .color = RGB(48, 173, 125), .icon = ICON_PHONE, .create = app_create, .inspect = app_inspect, .show = app_show, .tick = app_tick, .close = app_close, .destroy = app_destroy};
