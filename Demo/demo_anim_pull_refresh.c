#include "demo_anim_ui.h"
#include <stdio.h>
#include <string.h>

static DemoUI s_ui;
static GYOBJ s_content;
static GYOBJ s_status;
static GYOBJ s_rows[3];
static GYcoord s_start_y;
static GYcoord s_start_offset;
static uint8 s_refreshing;
static uint8 s_revision;

static void idleValue(int32 value, void* user)
{
	(void)value;
	(void)user;
}

static void reboundDone(GYANIM anim, uint8 cancelled, void* user)
{
	(void)anim;
	(void)user;
	if (!cancelled)
		s_refreshing = 0;
}

static void refreshDone(GYANIM anim, uint8 cancelled, void* user)
{
	(void)anim;
	(void)user;
	if (cancelled || !s_refreshing)
		return;
	char label[32];
	++s_revision;
	snprintf(label, sizeof(label), "Updated feed  /  revision %u", (unsigned)s_revision);
	YMGUI_Label_SetText(s_status, label);
	GYANIM rebound = YMGUI_Anim_MoveTo(s_content, 0, 0, 370, GY_ANIM_OVERSHOOT);
	if (rebound)
		YMGUI_Anim_SetDone(s_ui.ctx, rebound, reboundDone, NULL);
	else
	{
		ui_reposition(s_content, 0, 0);
		s_refreshing = 0;
	}
}

static void heldDone(GYANIM anim, uint8 cancelled, void* user)
{
	(void)anim;
	(void)user;
	if (cancelled || !s_refreshing)
		return;
	GYANIM timer = YMGUI_Anim_ValueTo(s_ui.ctx, s_content, 0, 1, 450,
								 GY_ANIM_LINEAR, idleValue, NULL);
	if (timer)
		YMGUI_Anim_SetDone(s_ui.ctx, timer, refreshDone, NULL);
	else
		refreshDone(0, 0, NULL);
}

static void triggerRefresh(void)
{
	if (s_refreshing)
		return;
	s_refreshing = 1;
	YMGUI_Label_SetText(s_status, "Refreshing...  /  release and rebound");
	GYANIM settle = YMGUI_Anim_MoveTo(s_content, 0, 59, 230, GY_ANIM_EASE_OUT);
	if (settle)
		YMGUI_Anim_SetDone(s_ui.ctx, settle, heldDone, NULL);
	else
		heldDone(0, 0, NULL);
}

static void pullEvent(GYOBJ obj, GYEvent event)
{
	(void)obj;
	if (s_refreshing)
		return;
	if (event == GY_EVENT_Pressed)
	{
		YMGUI_Anim_CancelObject(s_content);
		s_start_y = s_ui.ctx->point_y;
		s_start_offset = s_content->area.y;
	}
	else if (event == GY_EVENT_Pressing)
	{
		GYcoord offset = s_start_offset + s_ui.ctx->point_y - s_start_y;
		if (offset < 0) offset = 0;
		if (offset > 72) offset = 72;
		ui_reposition(s_content, 0, offset);
		YMGUI_Label_SetText(s_status, offset >= 49 ? "Release to refresh" : "Pull down to refresh");
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
	{
		if (s_content->area.y >= 49)
			triggerRefresh();
		else
			YMGUI_Anim_MoveTo(s_content, 0, 0, 300, GY_ANIM_OVERSHOOT);
	}
}

static void refreshButton(GYOBJ button)
{
	(void)button;
	if (s_refreshing)
		return;
	ui_reposition(s_content, 0, 65);
	triggerRefresh();
}

int main(int argc, char** argv)
{
	static const char* names[3] = {"Newest activity", "Product update", "Team message"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "19  /  PULL TO REFRESH", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 43, 420, 20, "Pull the feed; release for a refresh cycle", UI_MUTED);
	GYOBJ viewport = ui_box(s_ui.ctx->root, 28, 76, 424, 150, UI_PANEL);
	viewport->state |= GY_STATE_ClipChildren;
	ui_text(viewport, 70, 17, 286, 22, "Release to update feed", UI_CYAN);
	s_content = ui_box(viewport, 0, 0, 424, 150, UI_PANEL);
	s_content->event_cb = pullEvent;
	for (uint8 i = 0; i < 3; ++i)
	{
		s_rows[i] = ui_box(s_content, 8, 5 + i * 47, 408, 42,
							 i & 1 ? GY_ARGB(0xFF, 0x2B, 0x3B, 0x58) : UI_PANEL);
		s_rows[i]->event_cb = pullEvent;
		GYOBJ label = ui_text(s_rows[i], 16, 10, 375, 22, names[i], UI_WHITE);
		label->event_cb = pullEvent;
	}
	s_status = ui_text(s_ui.ctx->root, 28, 238, 288, 22, "Pull down to refresh", UI_CYAN);
	ui_button(s_ui.ctx->root, 342, 232, 110, 34, "Refresh", refreshButton);
	refreshButton(NULL);
	if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
	{
		refreshButton(NULL);
		int stayed = s_refreshing && s_content->area.y == 65;
		YMGUI_Inject_Tick(250);
		YMGUI_Inject_Tick(500);
		YMGUI_Inject_Tick(450);
		refreshButton(NULL);
		refreshButton(NULL);
		stayed = stayed && s_content->area.y == 65;
		YMGUI_Inject_Tick(250);
		YMGUI_Inject_Tick(500);
		YMGUI_Inject_Tick(450);
		YMGUI_Inject_Pointer(100, 113, 1);
		YMGUI_Inject_Pointer(100, 185, 1);
		YMGUI_Inject_Pointer(100, 185, 0);
		int triggered = s_refreshing;
		YMGUI_Inject_Tick(250);
		YMGUI_Inject_Tick(500);
		YMGUI_Inject_Tick(450);
		int ok = stayed && triggered && s_revision == 3 && !s_refreshing && s_content->area.y == 0;
		ui_free(&s_ui);
		return ok ? 0 : 2;
	}
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
