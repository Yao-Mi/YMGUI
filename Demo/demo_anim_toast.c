#include "demo_anim_ui.h"

static DemoUI s_ui;
static GYOBJ s_toasts[3];

static void showToasts(GYOBJ button)
{
	(void)button;
	for (uint8 i = 0; i < 3; ++i)
	{
		ui_reposition(s_toasts[i], 480, 82 + i * 57);
		GYANIM motion = YMGUI_Anim_MoveTo(s_toasts[i], 156, 82 + i * 57,
									  430, GY_ANIM_OVERSHOOT);
		YMGUI_Anim_SetDelay(s_ui.ctx, motion, i * 130);
	}
}

static void clearToasts(GYOBJ button)
{
	(void)button;
	for (uint8 i = 0; i < 3; ++i)
	{
		GYANIM motion = YMGUI_Anim_MoveTo(s_toasts[i], 480, 82 + i * 57,
									  320, GY_ANIM_EASE_IN);
		YMGUI_Anim_SetDelay(s_ui.ctx, motion, (2 - i) * 60);
	}
}

int main(int argc, char** argv)
{
	static const char* titles[3] = {"Upload complete", "New message", "Backup ready"};
	static const char* notes[3] = {"Project assets are synced", "A teammate replied", "A safe copy was created"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "08  /  NOTIFICATION STACK", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "Toasts enter with stagger and overshoot", UI_MUTED);
	ui_button(s_ui.ctx->root, 28, 81, 112, 34, "Notify", showToasts);
	ui_button(s_ui.ctx->root, 28, 125, 112, 34, "Clear", clearToasts);
	ui_box(s_ui.ctx->root, 28, 177, 112, 70, UI_PANEL);
	ui_text(s_ui.ctx->root, 39, 188, 95, 20, "Live UI", UI_WHITE);
	ui_text(s_ui.ctx->root, 39, 213, 95, 20, "feedback", UI_MUTED);
	for (uint8 i = 0; i < 3; ++i)
	{
		s_toasts[i] = ui_box(s_ui.ctx->root, 480, 82 + i * 57, 296, 48, UI_PANEL);
		ui_box(s_toasts[i], 0, 0, 5, 48, i == 1 ? UI_PURPLE : UI_CYAN);
		ui_text(s_toasts[i], 16, 4, 268, 21, titles[i], UI_WHITE);
		ui_text(s_toasts[i], 16, 26, 268, 19, notes[i], UI_MUTED);
	}
	showToasts(NULL);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
