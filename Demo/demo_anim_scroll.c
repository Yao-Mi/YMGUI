#include "demo_anim_ui.h"
#include <stdio.h>

static DemoUI s_ui;
static GYOBJ s_viewport;
static GYOBJ s_status;
static GYOBJ s_open_buttons[6];
static uint8 s_page;

static void openRow(GYOBJ button)
{
	char text[40];
	for (uint8 i = 0; i < 6; ++i)
		if (button == s_open_buttons[i])
		{
			snprintf(text, sizeof(text), "Opened item %u", (unsigned)(i + 1));
			YMGUI_Label_SetText(s_status, text);
			break;
		}
}

static void goTop(GYOBJ button)
{
	(void)button;
	s_page = 0;
	YMGUI_Anim_ScrollTo(s_viewport, 0, 0, 530, GY_ANIM_SMOOTH);
	YMGUI_Label_SetText(s_status, "Items 01-03");
}

static void goNext(GYOBJ button)
{
	(void)button;
	s_page = s_page == 2 ? 0 : s_page + 1;
	YMGUI_Anim_ScrollTo(s_viewport, 0, s_page == 1 ? 48 : s_page == 2 ? 144 : 0,
							 530, GY_ANIM_SMOOTH);
	YMGUI_Label_SetText(s_status, s_page == 0 ? "Items 01-03" :
									  s_page == 1 ? "Items 02-04" : "Items 04-06");
}

int main(int argc, char** argv)
{
	static const char* names[6] = {
		"Design review", "Release notes", "Team update",
		"System status", "Asset sync", "Weekly report"
	};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "09  /  SCROLLING VIEWPORT", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "The clipped object tree scrolls as one", UI_MUTED);
	s_viewport = ui_box(s_ui.ctx->root, 28, 72, 424, 150, UI_PANEL);
	s_viewport->state |= GY_STATE_ClipChildren;
	for (uint8 i = 0; i < 6; ++i)
	{
		GYOBJ row = ui_box(s_viewport, 8, 6 + i * 48, 408, 42,
							 i & 1 ? GY_ARGB(0xFF, 0x2B, 0x3A, 0x57) :
							 GY_ARGB(0xFF, 0x25, 0x33, 0x4F));
		ui_box(row, 0, 0, 4, 42, i & 1 ? UI_PURPLE : UI_CYAN);
		ui_text(row, 16, 11, 270, 21, names[i], UI_WHITE);
		s_open_buttons[i] = ui_button(row, 313, 6, 86, 30, "Open", openRow);
	}
	s_status = ui_text(s_ui.ctx->root, 28, 239, 175, 22, "Items 01-03", UI_MUTED);
	ui_button(s_ui.ctx->root, 252, 233, 92, 32, "Top", goTop);
	ui_button(s_ui.ctx->root, 354, 233, 98, 32, "Next", goNext);
	goNext(NULL);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
