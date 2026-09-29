#include "demo_anim_ui.h"

static DemoUI s_ui;
static GYOBJ s_panels[3];
static GYOBJ s_headers[3];
static int s_open = -1;

static void selectSection(GYOBJ button)
{
	int selected = -1;
	for (int i = 0; i < 3; ++i)
		if (button == s_headers[i])
			selected = i;
	if (selected < 0)
		return;
	s_open = s_open == selected ? -1 : selected;
	for (int i = 0; i < 3; ++i)
	{
		GYcoord y = 70 + i * 49 + (s_open >= 0 && i > s_open ? 42 : 0);
		YMGUI_Anim_MoveTo(s_panels[i], 28, y, 350, GY_ANIM_SMOOTH);
		YMGUI_Anim_ResizeTo(s_panels[i], 424, i == s_open ? 86 : 44,
								 350, GY_ANIM_SMOOTH);
		YMGUI_Anim_BgColorTo(s_panels[i],
							 i == s_open ? GY_ARGB(0xFF, 0x32, 0x3F, 0x66) : UI_PANEL,
							 350, GY_ANIM_SMOOTH);
	}
}

int main(int argc, char** argv)
{
	static const char* names[3] = {"Account", "Notifications", "Appearance"};
	static const char* details[3] = {
		"Profile, privacy and connected devices",
		"Email digests and alert preferences",
		"Theme, contrast and motion settings"
	};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "07  /  ACCORDION SECTIONS", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "Resize one panel; shift the following rows", UI_MUTED);
	for (int i = 0; i < 3; ++i)
	{
		s_panels[i] = ui_box(s_ui.ctx->root, 28, 70 + i * 49, 424, 44, UI_PANEL);
		s_panels[i]->state |= GY_STATE_ClipChildren;
		s_headers[i] = ui_button(s_panels[i], 6, 5, 412, 34, names[i], selectSection);
		ui_text(s_panels[i], 17, 54, 390, 22, details[i], UI_WHITE);
	}
	ui_text(s_ui.ctx->root, 28, 249, 420, 20, "Click the open section again to collapse it", UI_MUTED);
	selectSection(s_headers[0]);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
