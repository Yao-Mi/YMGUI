#include "demo_anim_ui.h"
#include "YMGUI_TextInput.h"
#include <string.h>

static DemoUI s_ui;
static GYOBJ s_field;
static GYOBJ s_input;
static GYOBJ s_status;

static void validate(GYOBJ button)
{
	(void)button;
	const char* value = YMGUI_TextInput_GetText(s_input);
	const char* at = value != NULL ? strchr(value, '@') : NULL;
	uint8 valid = at != NULL && at > value && strchr(at + 1, '.') != NULL;
	if (valid)
	{
		YMGUI_Anim_BgColorTo(s_field, UI_CYAN, 350, GY_ANIM_SMOOTH);
		YMGUI_Label_SetText(s_status, "Looks good / ready to submit");
	}
	else
	{
		YMGUI_Anim_BgColorTo(s_field, GY_ARGB(0xFF, 0xE8, 0x67, 0x78),
							  350, GY_ANIM_SMOOTH);
		YMGUI_Anim_MoveTo(s_field, 54, 102, 320, GY_ANIM_THERE_AND_BACK);
		YMGUI_Label_SetText(s_status, "Add an @ and a domain to continue");
	}
}

static void fixSample(GYOBJ button)
{
	(void)button;
	YMGUI_TextInput_SetText(s_input, "hello@example.com");
	validate(NULL);
}

int main(int argc, char** argv)
{
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "12  /  VALIDATION FEEDBACK", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "The whole input subtree reacts to invalid data", UI_MUTED);
	ui_text(s_ui.ctx->root, 40, 78, 380, 21, "Email address", UI_WHITE);
	s_field = ui_box(s_ui.ctx->root, 40, 102, 400, 44, UI_PURPLE);
	s_input = YMGUI_Creat_TextInput_Creat(s_field, 4, 4, 392, 36, 64);
	YMGUI_TextInput_SetText(s_input, "hello.example.com");
	s_status = ui_text(s_ui.ctx->root, 40, 166, 400, 22,
					   "Add an @ and a domain to continue", UI_MUTED);
	ui_button(s_ui.ctx->root, 40, 217, 185, 36, "Validate", validate);
	ui_button(s_ui.ctx->root, 255, 217, 185, 36, "Fix sample", fixSample);
	validate(NULL);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
