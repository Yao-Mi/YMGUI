#include "demo_anim_ui.h"

static DemoUI s_ui;
static GYOBJ s_drawer;
static GYOBJ s_title;
static GYOBJ s_overview;
static uint8 s_open;

static void toggleDrawer(GYOBJ button)
{
	(void)button;
	s_open = !s_open;
	YMGUI_Anim_MoveTo(s_drawer, s_open ? 0 : -218, 0, 430, GY_ANIM_EASE_OUT);
}

static void selectItem(GYOBJ button)
{
	if (button == s_overview)
		YMGUI_Label_SetText(s_title, "Overview / your workspace");
	else
		YMGUI_Label_SetText(s_title, "Activity / recent updates");
	toggleDrawer(button);
}

int main(int argc, char** argv)
{
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 425, 24, "01  /  NAVIGATION DRAWER", UI_CYAN);
	ui_button(s_ui.ctx->root, 28, 54, 88, 34, "Menu", toggleDrawer);
	s_title = ui_text(s_ui.ctx->root, 137, 63, 300, 25, "Overview / your workspace", UI_WHITE);
	GYOBJ content = ui_box(s_ui.ctx->root, 28, 108, 422, 138, UI_PANEL);
	ui_text(content, 20, 15, 340, 22, "Workspace", UI_WHITE);
	ui_text(content, 20, 47, 360, 22, "Projects active       08", UI_MUTED);
	ui_text(content, 20, 76, 360, 22, "Latest activity       Today", UI_MUTED);
	ui_text(content, 20, 108, 360, 20, "Tap Menu to slide the navigation in", UI_CYAN);

	s_drawer = ui_box(s_ui.ctx->root, -218, 0, 218, UI_H, GY_ARGB(0xFF, 0x2A, 0x37, 0x5B));
	ui_text(s_drawer, 20, 22, 178, 25, "YMGUI / MENU", UI_CYAN);
	ui_text(s_drawer, 20, 58, 178, 18, "Animated container", UI_MUTED);
	s_overview = ui_button(s_drawer, 18, 96, 182, 34, "Overview", selectItem);
	ui_button(s_drawer, 18, 143, 182, 34, "Activity", selectItem);
	ui_button(s_drawer, 18, 211, 182, 34, "Close", toggleDrawer);
	s_open = 0;
	toggleDrawer(NULL);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
