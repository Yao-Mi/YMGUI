#include "demo_anim_ui.h"
#include "YMGUI_Event.h"
#include "YMGUI_TextInput.h"

static DemoUI s_ui;
static GYOBJ s_fields[3];
static GYOBJ s_highlight;
static GYOBJ s_status;
static GYOBJ s_last_focus;

static void followFocus(void)
{
	GYOBJ focused = s_ui.ctx->focus_obj;
	if (focused == s_last_focus)
		return;
	s_last_focus = focused;
	for (uint8 i = 0; i < 3; ++i)
		if (focused == s_fields[i])
		{
			static const char* statuses[3] = {"Focus: Name", "Focus: Email", "Focus: Project"};
			YMGUI_Anim_MoveTo(s_highlight, 156, 86 + i * 51, 350, GY_ANIM_EASE_OUT);
			YMGUI_Label_SetText(s_status, statuses[i]);
			return;
		}
	YMGUI_Label_SetText(s_status, "Click a field or press Tab");
}

int main(int argc, char** argv)
{
	static const char* names[3] = {"Name", "Email", "Project"};
	static const char* values[3] = {"Alex", "alex@example.com", "YMGUI"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "11  /  FOCUS FOLLOW", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "Tab or click: the focus ring follows", UI_MUTED);
	s_highlight = ui_box(s_ui.ctx->root, 156, 86, 292, 38, UI_CYAN);
	for (uint8 i = 0; i < 3; ++i)
	{
		GYcoord y = 90 + i * 51;
		ui_text(s_ui.ctx->root, 38, y + 6, 112, 22, names[i], UI_WHITE);
		s_fields[i] = YMGUI_Creat_TextInput_Creat(s_ui.ctx->root, 160, y, 284, 30, 64);
		YMGUI_TextInput_SetText(s_fields[i], values[i]);
	}
	s_status = ui_text(s_ui.ctx->root, 38, 246, 390, 20, "Focus: Name", UI_MUTED);
	YMGUI_SetFocus(s_ui.ctx, s_fields[1]);
	followFocus();
	int max_frames = argc > 1 ? atoi(argv[1]) : -1;
	int frame = 0;
	while (SDL_LCD_PumpEvents())
	{
		followFocus();
		YMGUI_Refresh(s_ui.ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}
	ui_free(&s_ui);
	return 0;
}
