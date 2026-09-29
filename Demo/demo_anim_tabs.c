#include "demo_anim_ui.h"

static DemoUI s_ui;
static GYOBJ s_tabs[3];
static GYOBJ s_pages[3];
static GYOBJ s_indicator;

static void selectTab(GYOBJ button)
{
	uint8 selected = 0;
	for (uint8 i = 0; i < 3; ++i)
		if (button == s_tabs[i])
			selected = i;
	YMGUI_Anim_MoveTo(s_indicator, 32 + selected * 140, 101, 370, GY_ANIM_EASE_OUT);
	for (uint8 i = 0; i < 3; ++i)
		YMGUI_Anim_MoveTo(s_pages[i], (GYcoord)((int)i - selected) * 424,
						 0, 420, GY_ANIM_SMOOTH);
}

int main(int argc, char** argv)
{
	static const char* names[3] = {"Overview", "Activity", "Settings"};
	static const char* headlines[3] = {"Your workspace", "Recent activity", "Display settings"};
	static const char* details[3] = {"08 active projects", "12 updates today", "Motion enabled"};
	static const char* footers[3] = {"A page is a real YMGUI subtree", "Swipe-like page transition", "Indicator and page move together"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 18, 428, 24, "06  /  ANIMATED TABS", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 45, 420, 20, "Select a tab to slide between UI pages", UI_MUTED);
	for (uint8 i = 0; i < 3; ++i)
		s_tabs[i] = ui_button(s_ui.ctx->root, 28 + i * 140, 68, 132, 30,
							 names[i], selectTab);
	s_indicator = ui_box(s_ui.ctx->root, 32, 101, 124, 4, UI_CYAN);
	GYOBJ viewport = ui_box(s_ui.ctx->root, 28, 115, 424, 130, UI_PANEL);
	viewport->state |= GY_STATE_ClipChildren;
	for (uint8 i = 0; i < 3; ++i)
	{
		s_pages[i] = ui_box(viewport, i * 424, 0, 424, 130,
								  i == 1 ? GY_ARGB(0xFF, 0x28, 0x3C, 0x59) : UI_PANEL);
		ui_box(s_pages[i], 18, 17, 6, 95, i == 2 ? UI_PURPLE : UI_CYAN);
		ui_text(s_pages[i], 38, 18, 360, 24, headlines[i], UI_WHITE);
		ui_text(s_pages[i], 38, 54, 360, 22, details[i], UI_CYAN);
		ui_text(s_pages[i], 38, 89, 360, 22, footers[i], UI_MUTED);
	}
	selectTab(s_tabs[1]);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
