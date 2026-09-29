#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ contact_search, contact_rows[4], contact_selected;
	int contact_index;
} AppState;
static AppState state;

static void contact_filter(GYOBJ obj, const char* text);
static void contact_pick(GYOBJ obj);
static void contact_call(GYOBJ obj);
static void contact_message(GYOBJ obj);


static const char* contact_names[] = {"小米", "阿明", "小雨", "工作室"};
static const char* contact_numbers[] = {"13800000001", "13800000002", "13800000003", "01000000000"};

static void contact_filter(GYOBJ obj, const char* text)
{
	(void)obj;
	for (int i = 0; i < 4; ++i)
		YMGUI_Obj_SetHidden(state.contact_rows[i], text[0] && !strstr(contact_names[i], text) && !strstr(contact_numbers[i], text));
}

static void contact_pick(GYOBJ obj)
{
	for (int i = 0; i < 4; ++i)
		if (obj == state.contact_rows[i])
			state.contact_index = i;
	char text[96];
	snprintf(text, sizeof(text), "%s  %s", contact_names[state.contact_index], contact_numbers[state.contact_index]);
	PhoneUI_text_set(state.contact_selected, text);
}

static void contact_call(GYOBJ obj)
{
	(void)obj;
	PhoneIME_Hide();
	PhoneHost_Open(PHONE, contact_numbers[state.contact_index]);
}

static void contact_message(GYOBJ obj)
{
	(void)obj;
	PhoneIME_Hide();
	PhoneHost_Open(MESSAGES, contact_numbers[state.contact_index]);
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "通讯录", "搜索姓名或号码 · 示例联系人");
	state.contact_search = PhoneUI_app_input(view, 24, 65, 272, "", 63);
	for (int i = 0; i < 4; ++i)
	{
		char text[64];
		snprintf(text, sizeof(text), "%s  %s", contact_names[i], contact_numbers[i]);
		state.contact_rows[i] = PhoneUI_button(view, 24, 110 + i * 46, 272, 39, text, contact_pick, RGB(114, 142, 169));
	}
	YMGUI_TextInput_SetChanged(state.contact_search, contact_filter);
	state.contact_selected = PhoneUI_label_ex(view, 24, 303, 272, "小米  13800000001", MUTED, 2);
	PhoneUI_button(view, 24, 351, 130, 43, "电话", contact_call, PhoneApps_Get(PHONE)->color);
	PhoneUI_button(view, 166, 351, 130, 43, "短信", contact_message, PhoneApps_Get(MESSAGES)->color);
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "contact_search"))
		return (intptr_t)state.contact_search;
	if (!strcmp(name, "contact_rows"))
		return index >= 0 && index < 4 ? (intptr_t)state.contact_rows[index] : 0;
	if (!strcmp(name, "contact_selected"))
		return (intptr_t)state.contact_selected;
	if (!strcmp(name, "contact_index"))
		return (intptr_t)state.contact_index;
	return 0;
}

const PhoneApp PhoneApp_contacts = {
	.key = "contacts", .title = "通讯录", .color = RGB(68, 132, 198), .icon = ICON_CONTACTS, .create = app_create, .inspect = app_inspect, .destroy = app_destroy};
