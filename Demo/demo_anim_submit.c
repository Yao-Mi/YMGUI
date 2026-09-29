#include "demo_anim_ui.h"
#include "YMGUI_Bar.h"
#include <stdio.h>
#include <string.h>

static DemoUI s_ui;
static GYOBJ s_button;
static GYOBJ s_bar;
static GYOBJ s_status;
static GYANIM s_job;

static void updateProgress(int32 value, void* user)
{
	(void)user;
	char text[24];
	YMGUI_Bar_SetValue(s_bar, value);
	snprintf(text, sizeof(text), "Sending  %ld%%", (long)value);
	YMGUI_Label_SetText(s_status, text);
}

static void finish(GYANIM anim, uint8 cancelled, void* user)
{
	(void)anim;
	(void)user;
	if (cancelled)
		return;
	s_job = 0;
	YMGUI_Label_SetText(s_status, "Saved successfully");
	YMGUI_Button_SetText(s_button, "Done  /  retry");
	YMGUI_Anim_ResizeTo(s_button, 196, 42, 300, GY_ANIM_OVERSHOOT);
	YMGUI_Anim_BgColorTo(s_button, GY_ARGB(0xFF, 0x21, 0xA9, 0x8A), 300, GY_ANIM_SMOOTH);
}

static void submit(GYOBJ button)
{
	(void)button;
	if (s_job)
		YMGUI_Anim_Cancel(s_ui.ctx, s_job);
	YMGUI_Bar_SetValue(s_bar, 0);
	YMGUI_Label_SetText(s_status, "Sending  0%");
	YMGUI_Button_SetText(s_button, "Working...");
	YMGUI_Anim_ResizeTo(s_button, 150, 42, 260, GY_ANIM_SMOOTH);
	YMGUI_Anim_BgColorTo(s_button, UI_PURPLE, 220, GY_ANIM_SMOOTH);
	s_job = YMGUI_Anim_ValueTo(s_ui.ctx, s_bar, 0, 100, 1300,
							  GY_ANIM_SMOOTH, updateProgress, NULL);
	YMGUI_Anim_SetDone(s_ui.ctx, s_job, finish, NULL);
}

int main(int argc, char** argv)
{
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "16  /  SUBMIT FEEDBACK", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 45, 420, 20, "One action changes shape, progress and state", UI_MUTED);
	ui_box(s_ui.ctx->root, 28, 78, 424, 151, UI_PANEL);
	ui_text(s_ui.ctx->root, 51, 94, 360, 22, "Publish changes", UI_WHITE);
	s_status = ui_text(s_ui.ctx->root, 51, 123, 350, 22, "Ready to send", UI_CYAN);
	s_bar = YMGUI_Creat_Bar_Creat(s_ui.ctx->root, 51, 157, 378, 13);
	YMGUI_Bar_SetRange(s_bar, 0, 100);
	YMGUI_Bar_SetColors(s_bar, UI_BG, UI_CYAN);
	s_button = ui_button(s_ui.ctx->root, 142, 184, 196, 42, "Submit", submit);
	ui_text(s_ui.ctx->root, 28, 241, 420, 20, "Click again to restart the operation", UI_MUTED);
	submit(NULL);
	if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
	{
		YMGUI_Inject_Tick(500);
		YMGUI_Inject_Pointer(200, 205, 1);
		YMGUI_Inject_Pointer(200, 205, 0);
		int restarted = YMGUI_Bar_GetValue(s_bar) == 0 && s_job != 0;
		YMGUI_Inject_Tick(1300);
		int finished = YMGUI_Bar_GetValue(s_bar) == 100 && s_job == 0;
		YMGUI_Inject_Pointer(200, 205, 1);
		YMGUI_Inject_Pointer(200, 205, 0);
		YMGUI_Inject_Tick(1300);
		int ok = restarted && finished && YMGUI_Bar_GetValue(s_bar) == 100 && s_job == 0;
		ui_free(&s_ui);
		return ok ? 0 : 2;
	}
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
