#include "phone_ui.h"
#include "phone_host.h"

static void demo_notice(GYOBJ obj);
static void demo_exit(GYOBJ obj);
static int app_command(const char* name, const char* argument);
static void camera_mock_draw(GYOBJ obj, GYSURFACE surface, const GYrect* a);


static void demo_notice(GYOBJ obj)
{
	(void)obj;
	PhoneHost_Notice("相机", "相机演示已打开。\n未调用摄像头，不拍照。\n需要时再接入真实功能。");
}
static void demo_exit(GYOBJ obj)
{
	(void)obj;
	PhoneHost_Close();
}
static int app_command(const char* name, const char* argument)
{
	(void)argument;
	if (strcmp(name, "notice"))
		return 0;
	demo_notice(NULL);
	return 1;
}

static void camera_mock_draw(GYOBJ obj, GYSURFACE surface, const GYrect* a)
{
	PhoneUI_gallery_draw(obj, surface, a);
	for (int i = 1; i < 3; ++i)
	{
		YMGUI_Draw_Line(surface, a->x + a->w * i / 3, a->y + 8, a->x + a->w * i / 3, a->y + a->h - 8, WHITE);
		YMGUI_Draw_Line(surface, a->x + 8, a->y + a->h * i / 3, a->x + a->w - 8, a->y + a->h * i / 3, WHITE);
	}
}
static void app_create(GYOBJ view)
{

	PhoneUI_app_header(view, "相机", "界面演示 · 真实功能按需接入");
	GYOBJ preview = PhoneUI_panel(view, 18, 65, 284, 212, RGB(91, 150, 166));
	preview->draw_cb = camera_mock_draw;
	PhoneUI_label_ex(view, 24, 73, 272, "模拟取景 · 非摄像头画面", WHITE, 2);
	GYOBJ shutter = PhoneUI_button(view, 131, 287, 58, 58, "", demo_notice, PhoneApps_Get(CAMERA)->color);
	PhoneUI_button_icon(shutter, ICON_CAMERA);
	PhoneUI_left_label(view, 25, 304, 90, "照片模式", MUTED, 2);
	PhoneUI_button(view, 24, 354, 272, 39, "关闭应用并返回桌面", demo_exit, RGB(128, 137, 159));
}
const PhoneApp PhoneApp_camera = {
	.key = "camera", .title = "相机", .color = RGB(97, 111, 139), .icon = ICON_CAMERA, .create = app_create, .command = app_command};
