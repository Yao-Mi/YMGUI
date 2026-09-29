#include "phone_ui.h"
#include "phone_host.h"

static void demo_notice(GYOBJ obj);
static void demo_exit(GYOBJ obj);
static int app_command(const char* name, const char* argument);


static void demo_notice(GYOBJ obj)
{
	(void)obj;
	PhoneHost_Notice("浏览器", "浏览器演示已打开。\n未连接网络，不加载网页。\n需要时再接入真实功能。");
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


static void app_create(GYOBJ view)
{

	PhoneUI_app_header(view, "浏览器", "界面演示 · 真实功能按需接入");
	PhoneUI_app_input(view, 24, 66, 272, "yaomi://start", 127);
	GYOBJ web = PhoneUI_panel(view, 18, 112, 284, 163, WHITE);
	PhoneUI_left_label(web, 17, 10, 250, "发现一点新鲜", INK, 3);
	PhoneUI_left_label(web, 17, 55, 250, "欢迎使用 Yaomi 浏览器", MUTED, 0);
	GYOBJ card = PhoneUI_panel(web, 14, 98, 256, 47, RGB(232, 237, 252));
	PhoneUI_left_label(card, 12, 11, 236, "示例网页 · 不连接互联网", ACCENT, 0);
	PhoneUI_button(view, 24, 302, 272, 43, "打开演示弹窗", demo_notice, PhoneApps_Get(BROWSER)->color);
	PhoneUI_button(view, 24, 354, 272, 39, "关闭应用并返回桌面", demo_exit, RGB(128, 137, 159));
}
const PhoneApp PhoneApp_browser = {
	.key = "browser", .title = "浏览器", .color = RGB(80, 122, 220), .icon = ICON_BROWSER, .create = app_create, .command = app_command};
