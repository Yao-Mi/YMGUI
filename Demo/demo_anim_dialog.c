#include "demo_anim_ui.h"
#include "YMGUI_DrawFill.h"

static DemoUI s_ui;
static GYOBJ s_overlay;
static GYOBJ s_dialog;
static GYOBJ s_status;

static void drawOverlay(GYOBJ obj, GYSURFACE surface, const GYrect* abs)
{
	(void)obj;
	YMGUI_Draw_Fill(surface, abs, GY_ARGB(0xFF, 0x07, 0x0B, 0x18), 185);
}

static void closed(GYANIM animation, uint8 cancelled, void* user_data)
{
	(void)animation;
	(void)user_data;
	if (!cancelled)
		YMGUI_Obj_SetHidden(s_overlay, 1);
}

static void dismiss(GYOBJ button)
{
	(void)button;
	GYANIM motion = YMGUI_Anim_MoveTo(s_dialog, 70, -170, 340, GY_ANIM_EASE_IN);
	YMGUI_Anim_SetDone(s_ui.ctx, motion, closed, NULL);
}

static void confirm(GYOBJ button)
{
	YMGUI_Label_SetText(s_status, "Settings saved. Open again to review.");
	dismiss(button);
}

static void openDialog(GYOBJ button)
{
	(void)button;
	YMGUI_Obj_SetHidden(s_overlay, 0);
	ui_reposition(s_dialog, 70, -170);
	YMGUI_Anim_MoveTo(s_dialog, 70, 70, 420, GY_ANIM_OVERSHOOT);
}

int main(int argc, char** argv)
{
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 18, 428, 24, "02  /  MODAL DIALOG", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 62, 420, 24, "Preferences", UI_WHITE);
	ui_box(s_ui.ctx->root, 28, 100, 422, 102, UI_PANEL);
	ui_text(s_ui.ctx->root, 48, 119, 380, 20, "Notifications                 ON", UI_MUTED);
	ui_text(s_ui.ctx->root, 48, 152, 380, 20, "Motion effects               ON", UI_MUTED);
	s_status = ui_text(s_ui.ctx->root, 28, 213, 304, 24, "Open the dialog to apply changes", UI_MUTED);
	ui_button(s_ui.ctx->root, 348, 210, 104, 35, "Configure", openDialog);

	s_overlay = ui_box(YMGUI_Ctx_GetTopLayer(s_ui.ctx), 0, 0, UI_W, UI_H,
					   GY_ARGB(0xB8, 0x07, 0x0B, 0x18));
	s_overlay->draw_cb = drawOverlay;
	s_dialog = ui_box(s_overlay, 70, -170, 340, 150, GY_ARGB(0xFF, 0x32, 0x3F, 0x66));
	ui_text(s_dialog, 20, 17, 290, 24, "Apply workspace settings?", UI_WHITE);
	ui_text(s_dialog, 20, 48, 302, 22, "The panel animates as one UI subtree.", UI_MUTED);
	ui_button(s_dialog, 25, 96, 130, 36, "Cancel", dismiss);
	ui_button(s_dialog, 184, 96, 130, 36, "Apply", confirm);
	YMGUI_Obj_SetHidden(s_overlay, 1);
	openDialog(NULL);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
