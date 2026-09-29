#include "demo_anim_ui.h"
#include <stdio.h>
#include <string.h>

static DemoUI s_ui;
static GYOBJ s_rows[4];
static GYOBJ s_status;
static uint8 s_count;
static uint8 s_busy;
static uint8 s_serial;

static void rowDone(GYANIM anim, uint8 cancelled, void* user)
{
	(void)anim;
	if (!cancelled)
	{
		GYOBJ row = (GYOBJ)user;
		YMGUI_Free_ObjFree(row);
		s_busy = 0;
	}
}

static GYOBJ makeRow(uint8 number, GYcoord y)
{
	char title[32];
	GYOBJ row = ui_box(s_ui.ctx->root, 28, y, 424, 34, UI_PANEL);
	ui_box(row, 0, 0, 5, 34, number & 1 ? UI_PURPLE : UI_CYAN);
	snprintf(title, sizeof(title), "Task %02u  /  retained row", (unsigned)number);
	ui_text(row, 18, 7, 385, 21, title, UI_WHITE);
	return row;
}

static void addRow(GYOBJ button)
{
	(void)button;
	if (s_busy || s_count == 4)
		return;
	for (uint8 i = s_count; i > 1; --i)
		s_rows[i] = s_rows[i - 1];
	s_rows[1] = makeRow(++s_serial, 266);
	++s_count;
	for (uint8 i = 1; i < s_count; ++i)
		YMGUI_Anim_MoveTo(s_rows[i], 28, 85 + i * 37, 410, GY_ANIM_SMOOTH);
	YMGUI_Label_SetText(s_status, "Inserted row  /  neighbors make space");
}

static void removeRow(GYOBJ button)
{
	(void)button;
	if (s_busy || s_count < 2)
		return;
	s_busy = 1;
	GYOBJ removed = s_rows[1];
	for (uint8 i = 1; i + 1 < s_count; ++i)
		s_rows[i] = s_rows[i + 1];
	--s_count;
	GYANIM anim = YMGUI_Anim_MoveTo(removed, 480, removed->area.y, 360, GY_ANIM_EASE_IN);
	if (anim)
		YMGUI_Anim_SetDone(s_ui.ctx, anim, rowDone, removed);
	else
		rowDone(0, 0, removed);
	for (uint8 i = 1; i < s_count; ++i)
		YMGUI_Anim_MoveTo(s_rows[i], 28, 85 + i * 37, 420, GY_ANIM_SMOOTH);
	YMGUI_Label_SetText(s_status, "Removed row  /  gap closes smoothly");
}

static void reorder(GYOBJ button)
{
	(void)button;
	if (s_busy || s_count < 2)
		return;
	GYOBJ last = s_rows[s_count - 1];
	for (uint8 i = s_count - 1; i > 0; --i)
		s_rows[i] = s_rows[i - 1];
	s_rows[0] = last;
	for (uint8 i = 0; i < s_count; ++i)
		YMGUI_Anim_MoveTo(s_rows[i], 28, 85 + i * 37, 410, GY_ANIM_SMOOTH);
	YMGUI_Label_SetText(s_status, "Reordered  /  last row moves to top");
}

int main(int argc, char** argv)
{
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "14  /  LIVE LIST OPERATIONS", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "Insert and delete real objects; rows reflow", UI_MUTED);
	s_status = ui_text(s_ui.ctx->root, 28, 65, 424, 18, "Tap Add or Remove", UI_CYAN);
	for (uint8 i = 0; i < 3; ++i)
		s_rows[i] = makeRow(i + 1, 85 + i * 37);
	s_count = 3;
	s_serial = 3;
	ui_button(s_ui.ctx->root, 128, 235, 100, 32, "Reorder", reorder);
	ui_button(s_ui.ctx->root, 236, 235, 100, 32, "Add", addRow);
	ui_button(s_ui.ctx->root, 346, 235, 106, 32, "Remove", removeRow);
	addRow(NULL);
	if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
	{
		YMGUI_Inject_Tick(450);
		GYOBJ last = s_rows[3];
		YMGUI_Inject_Pointer(180, 250, 1);
		YMGUI_Inject_Pointer(180, 250, 0);
		int reordered = s_rows[0] == last;
		YMGUI_Inject_Tick(450);
		YMGUI_Inject_Pointer(392, 250, 1);
		YMGUI_Inject_Pointer(392, 250, 0);
		YMGUI_Inject_Pointer(180, 250, 1);
		YMGUI_Inject_Pointer(180, 250, 0);
		int guarded = s_count == 3 && s_busy && s_rows[0] == last;
		YMGUI_Inject_Tick(450);
		YMGUI_Inject_Pointer(285, 250, 1);
		YMGUI_Inject_Pointer(285, 250, 0);
		YMGUI_Inject_Tick(450);
		int ok = reordered && guarded && !s_busy && s_count == 4 && s_rows[1] != NULL;
		ui_free(&s_ui);
		return ok ? 0 : 2;
	}
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
