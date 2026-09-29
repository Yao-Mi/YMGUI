#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ weather_city, weather_temp, weather_condition;
	int city_index;
} AppState;
static AppState state;

static void weather_next(GYOBJ obj);


static void weather_next(GYOBJ obj)
{
	(void)obj;
	static const char* cities[] = {"杭州", "上海", "成都"};
	static const char* temps[] = {"22 C", "25 C", "19 C"};
	static const char* conditions[] = {"多云", "晴间多云", "阴天微风"};
	state.city_index = (state.city_index + 1) % 3;
	PhoneUI_text_set(state.weather_city, cities[state.city_index]);
	PhoneUI_text_set(state.weather_temp, temps[state.city_index]);
	PhoneUI_text_set(state.weather_condition, conditions[state.city_index]);
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "天气", "离线示例天气");
	state.weather_city = PhoneUI_label_ex(view, 18, 72, 284, "杭州", INK, 3);
	state.weather_temp = PhoneUI_label_ex(view, 18, 123, 284, "22 C", ACCENT, 1);
	state.weather_condition = PhoneUI_label(view, 18, 195, 284, "多云", MUTED);
	static const char* forecast[] = {"今天       18 / 24 C", "星期三     17 / 23 C", "星期四     19 / 26 C"};
	for (int i = 0; i < 3; ++i)
	{
		GYOBJ row = PhoneUI_panel(view, 18, 235 + i * 40, 284, 36, WHITE);
		PhoneUI_left_label(row, 14, 5, 256, forecast[i], INK, 0);
	}
	PhoneUI_button(view, 18, 369, 284, 37, "切换城市", weather_next, PhoneApps_Get(WEATHER)->color);
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "weather_city"))
		return (intptr_t)state.weather_city;
	if (!strcmp(name, "weather_temp"))
		return (intptr_t)state.weather_temp;
	if (!strcmp(name, "weather_condition"))
		return (intptr_t)state.weather_condition;
	if (!strcmp(name, "city_index"))
		return (intptr_t)state.city_index;
	return 0;
}

const PhoneApp PhoneApp_weather = {
	.key = "weather", .title = "天气", .color = RGB(69, 161, 218), .icon = ICON_WEATHER, .create = app_create, .inspect = app_inspect, .destroy = app_destroy};
