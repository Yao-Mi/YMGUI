#include "demo_anim_ui.h"
#include <string.h>

static DemoUI s_ui;
static GYOBJ s_list;
static GYOBJ s_rows[3];
static GYOBJ s_open[3];
static GYOBJ s_detail;
static GYOBJ s_detail_title;
static GYOBJ s_detail_note;
static GYOBJ s_status;

static void closeDetail(GYOBJ button)
{
	(void)button;
	YMGUI_Anim_MoveTo(s_detail, 480, 78, 400, GY_ANIM_EASE_IN);
	YMGUI_Anim_ResizeTo(s_list, 424, 145, 400, GY_ANIM_SMOOTH);
	for (uint8 i = 0; i < 3; ++i)
		YMGUI_Anim_ResizeTo(s_rows[i], 408, 42, 400, GY_ANIM_SMOOTH);
	YMGUI_Label_SetText(s_status, "Detail closed  /  list expands");
}

static void openDetail(GYOBJ button)
{
	static const char* titles[3] = {"Design review", "Release notes", "Asset library"};
	static const char* notes[3] = {"Due today at 17:00", "Version 2.4 ready", "18 files available"};
	uint8 index = 0;
	for (uint8 i = 0; i < 3; ++i)
		if (button == s_open[i]) index = i;
	YMGUI_Label_SetText(s_detail_title, titles[index]);
	YMGUI_Label_SetText(s_detail_note, notes[index]);
	YMGUI_Anim_ResizeTo(s_list, 205, 145, 400, GY_ANIM_SMOOTH);
	for (uint8 i = 0; i < 3; ++i)
		YMGUI_Anim_ResizeTo(s_rows[i], 189, 42, 400, GY_ANIM_SMOOTH);
	YMGUI_Anim_MoveTo(s_detail, 246, 78, 430, GY_ANIM_OVERSHOOT);
	YMGUI_Label_SetText(s_status, "Selected row  /  detail slides in");
}

int main(int argc, char** argv)
{
	static const char* titles[3] = {"Design", "Release", "Assets"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "18  /  MASTER DETAIL", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "Select a row; list gives room to details", UI_MUTED);
	s_list = ui_box(s_ui.ctx->root, 28, 78, 424, 145, UI_PANEL);
	s_list->state |= GY_STATE_ClipChildren;
	for (uint8 i = 0; i < 3; ++i)
	{
		s_rows[i] = ui_box(s_list, 8, 6 + i * 46, 408, 42,
							 i & 1 ? GY_ARGB(0xFF, 0x2A, 0x38, 0x56) : UI_PANEL);
		s_open[i] = ui_button(s_rows[i], 4, 4, 160, 34, titles[i], openDetail);
	}
	s_detail = ui_box(s_ui.ctx->root, 480, 78, 206, 145, GY_ARGB(0xFF, 0x32, 0x40, 0x65));
	s_detail_title = ui_text(s_detail, 10, 17, 186, 23, "", UI_WHITE);
	s_detail_note = ui_text(s_detail, 10, 50, 186, 21, "", UI_CYAN);
	ui_text(s_detail, 10, 83, 186, 20, "A responsive side pane", UI_MUTED);
	ui_button(s_detail, 103, 107, 92, 30, "Close", closeDetail);
	s_status = ui_text(s_ui.ctx->root, 28, 238, 424, 22, "Select any row", UI_CYAN);
	openDetail(s_open[0]);
	if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
	{
		YMGUI_Inject_Tick(450);
		YMGUI_Inject_Pointer(392, 200, 1);
		YMGUI_Inject_Pointer(392, 200, 0);
		YMGUI_Inject_Pointer(100, 106, 1);
		YMGUI_Inject_Pointer(100, 106, 0);
		YMGUI_Inject_Tick(450);
		int reopened = s_detail->area.x == 246 && s_list->area.w == 205;
		YMGUI_Inject_Pointer(392, 200, 1);
		YMGUI_Inject_Pointer(392, 200, 0);
		YMGUI_Inject_Tick(450);
		int ok = reopened && s_detail->area.x == 480 && s_list->area.w == 424;
		ui_free(&s_ui);
		return ok ? 0 : 2;
	}
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
