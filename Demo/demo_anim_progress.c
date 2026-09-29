#include "demo_anim_ui.h"
#include "YMGUI_Bar.h"
#include <stdio.h>

static DemoUI s_ui;
static GYOBJ s_bar;
static GYOBJ s_percent;
static GYOBJ s_status;
static GYOBJ s_badges[3];
static GYANIM s_value_anim;

static void progressChanged(int32 value, void* user_data)
{
	(void)user_data;
	char text[24];
	YMGUI_Bar_SetValue(s_bar, value);
	snprintf(text, sizeof(text), "%ld%%", (long)value);
	YMGUI_Label_SetText(s_percent, text);
	for (uint8 i = 0; i < 3; ++i)
	{
		GYcolor target = value >= (int32)((i + 1) * 33) ? UI_CYAN : UI_PANEL;
		if (s_badges[i]->bg_color != target)
			YMGUI_Obj_SetBgColor(s_badges[i], target);
	}
}

static void progressDone(GYANIM animation, uint8 cancelled, void* user_data)
{
	(void)animation;
	(void)user_data;
	if (!cancelled)
	{
		s_value_anim = 0;
		YMGUI_Label_SetText(s_status, "Complete / all three steps done");
	}
}

static void start(GYOBJ button)
{
	(void)button;
	if (s_value_anim)
		YMGUI_Anim_Cancel(s_ui.ctx, s_value_anim);
	progressChanged(0, NULL);
	YMGUI_Label_SetText(s_status, "Working / updating three steps");
	s_value_anim = YMGUI_Anim_ValueTo(s_ui.ctx, s_bar, 0, 100, 1800,
								  GY_ANIM_SMOOTH, progressChanged, NULL);
	YMGUI_Anim_SetDone(s_ui.ctx, s_value_anim, progressDone, NULL);
}

static void reset(GYOBJ button)
{
	(void)button;
	if (s_value_anim)
		YMGUI_Anim_Cancel(s_ui.ctx, s_value_anim);
	s_value_anim = 0;
	progressChanged(0, NULL);
	YMGUI_Label_SetText(s_status, "Ready / press Start to run");
}

int main(int argc, char** argv)
{
	static const char* steps[3] = {"Prepare", "Transfer", "Finish"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "04  /  PROGRESS FEEDBACK", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 50, 410, 22, "A numeric tween drives standard YMGUI widgets", UI_MUTED);
	GYOBJ panel = ui_box(s_ui.ctx->root, 28, 81, 424, 150, UI_PANEL);
	ui_text(panel, 18, 14, 200, 22, "Sync workspace", UI_WHITE);
	s_percent = ui_text(panel, 350, 14, 55, 22, "0%", UI_CYAN);
	s_bar = YMGUI_Creat_Bar_Creat(panel, 18, 49, 388, 20);
	YMGUI_Bar_SetRange(s_bar, 0, 100);
	YMGUI_Bar_SetColors(s_bar, GY_ARGB(0xFF, 0x12, 0x1B, 0x30), UI_CYAN);
	for (uint8 i = 0; i < 3; ++i)
	{
		GYcoord x = 18 + i * 132;
		s_badges[i] = ui_box(panel, x, 86, 120, 38, UI_PANEL);
		ui_text(s_badges[i], 8, 10, 104, 20, steps[i], UI_WHITE);
	}
	s_status = ui_text(s_ui.ctx->root, 28, 243, 290, 22, "Ready / press Start to run", UI_MUTED);
	ui_button(s_ui.ctx->root, 326, 237, 58, 30, "Start", start);
	ui_button(s_ui.ctx->root, 392, 237, 60, 30, "Reset", reset);
	start(NULL);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
