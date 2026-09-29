#include "phone_ui.h"
#include "phone_host.h"
#include "phone_shade.h"
#include "phone_quick.h"
#include "phone_quick_builtin.h"
enum
{
	WIFI,
	DATA,
	FLIGHT,
	BLUETOOTH,
	HOTSPOT,
	SOUND,
	CAPTURE,
	QUIET
};
/* SOUND 表示静音已开启；默认正常响铃。 */
static int options[8] = {1, 1, 0, 0, 0, 0, 0, 0};
static const char* keys[] = {"wifi", "data", "flight", "bluetooth", "hotspot", "sound", "capture", "quiet"};
int PhoneQuickBuiltin_State(const char* key)
{
	for (int i = 0; i < 8; ++i)
		if (!strcmp(key, keys[i]))
			return options[i];
	return 0;
}
void PhoneQuickBuiltin_SetWifi(int on)
{
	options[WIFI] = !!on;
}
static void toggle(int id)
{
	if (id == WIFI)
	{
		PhoneHost_SetWifi(!options[WIFI]);
		return;
	}
	options[id] = !options[id];
	if (id == FLIGHT && options[id])
		options[DATA] = options[HOTSPOT] = 0;
	if ((id == DATA || id == HOTSPOT) && options[id])
		options[FLIGHT] = 0;
}
static void draw(int id, GYSURFACE s, int x, int y, GYcolor bg)
{
	if (id == WIFI || id == HOTSPOT)
	{
		YMGUI_Draw_ArcThick(s, x, y + 6, 10, 12, 220, 320, WHITE);
		YMGUI_Draw_ArcThick(s, x, y + 6, 5, 7, 220, 320, WHITE);
		YMGUI_Draw_CircleFill(s, x, y + 6, 2, WHITE);
		if (id == HOTSPOT)
			YMGUI_Draw_CircleFill(s, x, y - 9, 2, WHITE);
	}
	else if (id == CAPTURE)
		PhoneUI_icon_draw(s, ICON_CAMERA, x, y, bg);
	else if (id == SOUND)
	{
		GYrect speaker = {x - 10, y - 4, 6, 9};
		YMGUI_Draw_Fill(s, &speaker, WHITE, 255);
		YMGUI_Draw_Line(s, x - 4, y - 4, x + 1, y - 9, WHITE);
		YMGUI_Draw_Line(s, x + 1, y - 9, x + 1, y + 9, WHITE);
		YMGUI_Draw_Line(s, x + 1, y + 9, x - 4, y + 4, WHITE);
		if (options[id])
		{
			YMGUI_Draw_Line(s, x + 6, y - 4, x + 13, y + 4, WHITE);
			YMGUI_Draw_Line(s, x + 6, y + 4, x + 13, y - 4, WHITE);
		}
		else
			YMGUI_Draw_ArcThick(s, x + 1, y, 8, 10, 310, 410, WHITE);
	}
	else if (id == QUIET)
		PhoneUI_icon_draw(s, ICON_CLOCK, x, y, bg);
	else if (id == FLIGHT)
	{
		YMGUI_Draw_Line(s, x, y - 12, x, y + 11, WHITE);
		YMGUI_Draw_Line(s, x - 11, y + 3, x, y - 3, WHITE);
		YMGUI_Draw_Line(s, x, y - 3, x + 11, y + 3, WHITE);
		YMGUI_Draw_Line(s, x - 5, y + 11, x + 5, y + 11, WHITE);
	}
	else if (id == DATA)
	{
		YMGUI_Draw_Line(s, x - 5, y - 10, x - 5, y + 10, WHITE);
		YMGUI_Draw_Line(s, x - 10, y - 5, x - 5, y - 10, WHITE);
		YMGUI_Draw_Line(s, x + 5, y - 10, x + 5, y + 10, WHITE);
		YMGUI_Draw_Line(s, x + 5, y + 10, x + 10, y + 5, WHITE);
	}
	else
	{
		YMGUI_Draw_Line(s, x, y - 12, x, y + 12, WHITE);
		YMGUI_Draw_Line(s, x, y - 12, x + 8, y - 5, WHITE);
		YMGUI_Draw_Line(s, x + 8, y - 5, x - 7, y + 7, WHITE);
		YMGUI_Draw_Line(s, x - 7, y - 7, x + 8, y + 5, WHITE);
		YMGUI_Draw_Line(s, x + 8, y + 5, x, y + 12, WHITE);
	}
}
/* 每项是独立描述符：联动设备状态属于内置实现，不进入面板分发分支。 */
#define FEATURE(name, title_, id)                                  \
	static int name##_active(void)                                 \
	{                                                              \
		return options[id];                                        \
	}                                                              \
	static void name##_activate(void)                              \
	{                                                              \
		toggle(id);                                                \
	}                                                              \
	static void name##_draw(GYSURFACE s, int x, int y, GYcolor bg) \
	{                                                              \
		draw(id, s, x, y, bg);                                     \
	}                                                              \
	const PhoneQuick PhoneQuick_##name = {#name, title_, name##_active, name##_activate, name##_draw};
FEATURE(wifi, "Wi-Fi", WIFI)
FEATURE(data, "数据", DATA)
FEATURE(flight, "飞行", FLIGHT)
FEATURE(bluetooth, "蓝牙", BLUETOOTH)
FEATURE(hotspot, "热点", HOTSPOT)
FEATURE(sound, "静音", SOUND)
FEATURE(quiet, "勿扰", QUIET)
static void capture_draw(GYSURFACE s, int x, int y, GYcolor bg)
{
	draw(CAPTURE, s, x, y, bg);
}
const PhoneQuick PhoneQuick_capture = {"capture", "截屏", NULL, PhoneShade_Capture, capture_draw};
