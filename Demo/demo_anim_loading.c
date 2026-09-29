#include "demo_anim_ui.h"
#include <string.h>

static DemoUI s_ui;
static GYOBJ s_placeholders[3];
static GYOBJ s_content[3];
static GYOBJ s_status;
static uint8 s_loaded;

static void showContent(GYANIM anim, uint8 cancelled, void* user)
{
	(void)anim;
	(void)user;
	if (cancelled || !s_loaded)
		return;
	for (uint8 i = 0; i < 3; ++i)
	{
		ui_reposition(s_content[i], 480, 85 + i * 49);
		GYANIM move = YMGUI_Anim_MoveTo(s_content[i], 28, 85 + i * 49, 360, GY_ANIM_EASE_OUT);
		YMGUI_Anim_SetDelay(s_ui.ctx, move, i * 95);
	}
	YMGUI_Label_SetText(s_status, "Content ready  /  three retained cards");
}

static void load(GYOBJ button)
{
	(void)button;
	if (s_loaded)
		return;
	s_loaded = 1;
	YMGUI_Label_SetText(s_status, "Loading content...");
	for (uint8 i = 0; i < 3; ++i)
	{
		GYANIM shrink = YMGUI_Anim_ResizeTo(s_placeholders[i], 424, 1, 370, GY_ANIM_EASE_IN);
		YMGUI_Anim_BgColorTo(s_placeholders[i], UI_BG, 370, GY_ANIM_SMOOTH);
		if (i == 2)
			YMGUI_Anim_SetDone(s_ui.ctx, shrink, showContent, NULL);
	}
}

static void reset(GYOBJ button)
{
	(void)button;
	s_loaded = 0;
	for (uint8 i = 0; i < 3; ++i)
	{
		YMGUI_Anim_MoveTo(s_content[i], 480, 85 + i * 49, 240, GY_ANIM_EASE_IN);
		YMGUI_Anim_ResizeTo(s_placeholders[i], 424, 43, 320, GY_ANIM_SMOOTH);
		YMGUI_Anim_BgColorTo(s_placeholders[i], GY_ARGB(0xFF, 0x35, 0x45, 0x60), 320, GY_ANIM_SMOOTH);
	}
	YMGUI_Label_SetText(s_status, "Waiting for data");
}

int main(int argc, char** argv)
{
	static const char* names[3] = {"Workspace overview", "Team activity", "Recent uploads"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "17  /  SKELETON TO CONTENT", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 43, 420, 20, "Placeholders collapse; real UI cards arrive", UI_MUTED);
	s_status = ui_text(s_ui.ctx->root, 28, 64, 420, 18, "Waiting for data", UI_CYAN);
	for (uint8 i = 0; i < 3; ++i)
	{
		s_placeholders[i] = ui_box(s_ui.ctx->root, 28, 85 + i * 49, 424, 43,
							 GY_ARGB(0xFF, 0x35, 0x45, 0x60));
		s_placeholders[i]->state |= GY_STATE_ClipChildren;
		ui_box(s_placeholders[i], 12, 10, 36, 22, UI_MUTED);
		ui_box(s_placeholders[i], 62, 12, 211, 17, UI_MUTED);
		s_content[i] = ui_box(s_ui.ctx->root, 480, 85 + i * 49, 424, 43, UI_PANEL);
		ui_box(s_content[i], 0, 0, 5, 43, i == 1 ? UI_PURPLE : UI_CYAN);
		ui_text(s_content[i], 18, 10, 380, 21, names[i], UI_WHITE);
	}
	ui_button(s_ui.ctx->root, 256, 235, 92, 32, "Load", load);
	ui_button(s_ui.ctx->root, 358, 235, 94, 32, "Reset", reset);
	load(NULL);
	if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
	{
		YMGUI_Inject_Tick(100);
		YMGUI_Inject_Pointer(400, 250, 1);
		YMGUI_Inject_Pointer(400, 250, 0);
		YMGUI_Inject_Pointer(300, 250, 1);
		YMGUI_Inject_Pointer(300, 250, 0);
		YMGUI_Inject_Tick(400);
		YMGUI_Inject_Tick(600);
		int loaded = s_loaded && s_content[2]->area.x == 28;
		YMGUI_Inject_Pointer(400, 250, 1);
		YMGUI_Inject_Pointer(400, 250, 0);
		YMGUI_Inject_Tick(350);
		int ok = loaded && !s_loaded && s_content[2]->area.x == 480 &&
				 s_placeholders[2]->area.h == 43;
		ui_free(&s_ui);
		return ok ? 0 : 2;
	}
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
