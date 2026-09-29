#include "demo_anim_ui.h"
#include "YMGUI_Checkbox.h"
#include <stdio.h>

static DemoUI s_ui;
static GYOBJ s_rows[3];
static GYOBJ s_checks[3];
static GYOBJ s_bar;
static GYOBJ s_status;

static void updateSelection(GYOBJ checkbox, uint8 checked)
{
	(void)checkbox;
	(void)checked;
	uint8 count = 0;
	char text[32];
	for (uint8 i = 0; i < 3; ++i)
	{
		uint8 selected = YMGUI_Checkbox_GetChecked(s_checks[i]);
		count += selected;
		YMGUI_Anim_BgColorTo(s_rows[i], selected ? GY_ARGB(0xFF, 0x35, 0x48, 0x70) : UI_PANEL,
							  290, GY_ANIM_SMOOTH);
	}
	YMGUI_Anim_MoveTo(s_bar, 28, count ? 224 : 272, 350, GY_ANIM_EASE_OUT);
	snprintf(text, sizeof(text), "%u item%s selected", (unsigned)count, count == 1 ? "" : "s");
	YMGUI_Label_SetText(s_status, text);
}

static void clearSelection(GYOBJ button)
{
	(void)button;
	for (uint8 i = 0; i < 3; ++i)
		YMGUI_Checkbox_SetChecked(s_checks[i], 0);
	updateSelection(NULL, 0);
}

static void archiveSelection(GYOBJ button)
{
	clearSelection(button);
	YMGUI_Label_SetText(s_status, "Items archived");
}

int main(int argc, char** argv)
{
	static const char* names[3] = {"Design review", "Release notes", "Team update"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "13  /  BULK ACTION BAR", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "Selection reveals contextual actions", UI_MUTED);
	s_status = ui_text(s_ui.ctx->root, 28, 68, 410, 20, "0 items selected", UI_CYAN);
	for (uint8 i = 0; i < 3; ++i)
	{
		s_rows[i] = ui_box(s_ui.ctx->root, 28, 94 + i * 43, 424, 38, UI_PANEL);
		s_checks[i] = YMGUI_Creat_Checkbox_Creat(s_rows[i], 12, 4, 390, 30);
		YMGUI_Checkbox_SetText(s_checks[i], names[i]);
		YMGUI_Checkbox_SetChanged(s_checks[i], updateSelection);
	}
	s_bar = ui_box(s_ui.ctx->root, 28, 272, 424, 44, GY_ARGB(0xFF, 0x31, 0x40, 0x65));
	ui_text(s_bar, 14, 12, 150, 22, "Selection actions", UI_WHITE);
	ui_button(s_bar, 232, 5, 84, 33, "Clear", clearSelection);
	ui_button(s_bar, 326, 5, 88, 33, "Archive", archiveSelection);
	YMGUI_Checkbox_SetChecked(s_checks[0], 1);
	YMGUI_Checkbox_SetChecked(s_checks[1], 1);
	updateSelection(NULL, 0);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
