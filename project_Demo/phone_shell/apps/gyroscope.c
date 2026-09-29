#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ gyro_control, gyro_values;
} AppState;
static AppState state;

static void gyro_changed(GYOBJ obj, int16 x, int16 y);
static void gyro_reset(GYOBJ obj);


static void gyro_changed(GYOBJ obj, int16 x, int16 y)
{
	(void)obj;
	char text[96];
	snprintf(text, sizeof(text), "X %+d  Y %+d  Z 0", x, y);
	PhoneUI_text_set(state.gyro_values, text);
}

static void gyro_reset(GYOBJ obj)
{
	(void)obj;
	YMGUI_Joystick_SetValue(state.gyro_control, 0, 0);
	gyro_changed(state.gyro_control, 0, 0);
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "陀螺仪", "拖动模拟姿态 · 无传感器数据");
	state.gyro_control = YMGUI_Creat_Joystick_Creat(view, 72, 87, 176, 176);
	YMGUI_Joystick_SetAutoCenter(state.gyro_control, 0);
	YMGUI_Joystick_SetChangedCb(state.gyro_control, gyro_changed);
	state.gyro_values = PhoneUI_label(view, 20, 286, 280, "X +0  Y +0  Z 0", INK);
	PhoneUI_button(view, 24, 351, 272, 43, "归零", gyro_reset, PhoneApps_Get(GYROSCOPE)->color);
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "gyro_control"))
		return (intptr_t)state.gyro_control;
	if (!strcmp(name, "gyro_values"))
		return (intptr_t)state.gyro_values;
	return 0;
}

const PhoneApp PhoneApp_gyroscope = {
	.key = "gyroscope", .title = "陀螺仪", .color = RGB(73, 165, 171), .icon = ICON_GYROSCOPE, .create = app_create, .inspect = app_inspect, .destroy = app_destroy};
