#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ theme_switch, wifi_switch, brightness_slider, device_name_input, wifi_status, settings_status;
} AppState;
static AppState state;

static void on_power(GYOBJ obj);
static void on_settings(GYOBJ btn, uint8 on);
static void brightness_changed(GYOBJ obj, int32 value);
static void wifi_changed(GYOBJ obj, uint8 on);

static void on_power(GYOBJ obj)
{
	(void)obj;
	PhoneHost_Power();
}
static void on_settings(GYOBJ btn, uint8 on)
{
	(void)btn;
	PhoneUI_text_set(state.settings_status, on ? "海蓝" : "暮色");
	PhoneHost_SetTheme(on);
}

static void brightness_changed(GYOBJ obj, int32 value)
{
	(void)obj;
	PhoneHost_SetBrightness(value);
}

static void wifi_changed(GYOBJ obj, uint8 on)
{
	(void)obj;
	PhoneHost_SetWifi(on);
}
static void app_show(const char* argument)
{
	(void)argument;
	YMGUI_Switch_SetOn(state.wifi_switch, PhoneHost_GetWifi());
	PhoneUI_text_set(state.wifi_status, PhoneHost_GetWifi() ? "Yaomi 工作室" : "未连接");
	YMGUI_Slider_SetValue(state.brightness_slider, PhoneHost_GetBrightness());
}
static int app_command(const char* name, const char* argument)
{
	if (strcmp(name, "sync"))
		return 0;
	app_show(argument);
	return 1;
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "设置", "把手机调成喜欢的样子");
	GYOBJ network = PhoneUI_panel(view, 18, 65, 284, 60, WHITE);
	PhoneUI_left_label(network, 14, 6, 180, "无线网络", INK, 0);
	state.wifi_status = PhoneUI_left_label(network, 14, 30, 180, "Yaomi 工作室", MUTED, 2);
	state.wifi_switch = YMGUI_Creat_Switch_Creat(network, 218, 16, 48, 27);
	YMGUI_Switch_SetOn(state.wifi_switch, 1);
	state.wifi_switch->draw_cb = PhoneUI_switch_draw;
	YMGUI_Switch_SetChanged(state.wifi_switch, wifi_changed);
	GYOBJ theme = PhoneUI_panel(view, 18, 135, 284, 60, WHITE);
	PhoneUI_left_label(theme, 14, 6, 180, "海蓝色壁纸", INK, 0);
	state.settings_status = PhoneUI_left_label(theme, 14, 30, 180, "暮色", MUTED, 2);
	state.theme_switch = YMGUI_Creat_Switch_Creat(theme, 218, 16, 48, 27);
	state.theme_switch->draw_cb = PhoneUI_switch_draw;
	YMGUI_Switch_SetChanged(state.theme_switch, on_settings);
	GYOBJ bright = PhoneUI_panel(view, 18, 205, 284, 79, WHITE);
	PhoneUI_left_label(bright, 14, 6, 244, "预览亮度", INK, 0);
	state.brightness_slider = YMGUI_Creat_Slider_Creat(bright, 14, 39, 256, 24);
	YMGUI_Slider_SetValue(state.brightness_slider, 100);
	state.brightness_slider->draw_cb = PhoneUI_slider_draw;
	YMGUI_Slider_SetChanged(state.brightness_slider, brightness_changed);
	state.device_name_input = YMGUI_Creat_TextInput_Creat(view, 24, 293, 272, 30, 95);
	state.device_name_input->draw_cb = PhoneUI_phone_input_draw;
	YMGUI_TextInput_SetText(state.device_name_input, "Yaomi Phone");
	PhoneUI_button(view, 18, 334, 284, 43, "关机...", on_power, RGB(198, 91, 114));
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "theme_switch"))
		return (intptr_t)state.theme_switch;
	if (!strcmp(name, "wifi_switch"))
		return (intptr_t)state.wifi_switch;
	if (!strcmp(name, "brightness_slider"))
		return (intptr_t)state.brightness_slider;
	if (!strcmp(name, "device_name_input"))
		return (intptr_t)state.device_name_input;
	if (!strcmp(name, "wifi_status"))
		return (intptr_t)state.wifi_status;
	if (!strcmp(name, "settings_status"))
		return (intptr_t)state.settings_status;
	return 0;
}

const PhoneApp PhoneApp_settings = {
	.key = "settings", .title = "设置", .color = RGB(102, 125, 158), .icon = ICON_SETTINGS, .create = app_create, .inspect = app_inspect, .show = app_show, .command = app_command, .destroy = app_destroy};
