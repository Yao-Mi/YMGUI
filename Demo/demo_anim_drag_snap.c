#include "demo_anim_ui.h"
#include <string.h>

static DemoUI s_ui;
static GYOBJ s_card;
static GYOBJ s_status;
static GYcoord s_offset_x;
static GYcoord s_offset_y;
static uint8 s_slot;

static const GYcoord s_slot_x[4] = {28, 246, 28, 246};
static const GYcoord s_slot_y[4] = {87, 87, 160, 160};

static void snap(void)
{
	GYcoord cx = s_card->area.x + s_card->area.w / 2;
	GYcoord cy = s_card->area.y + s_card->area.h / 2;
	int32 best = INT32_MAX;
	for (uint8 i = 0; i < 4; ++i)
	{
		int32 dx = cx - (s_slot_x[i] + 103);
		int32 dy = cy - (s_slot_y[i] + 31);
		int32 distance = dx * dx + dy * dy;
		if (distance < best)
		{
			best = distance;
			s_slot = i;
		}
	}
	YMGUI_Anim_MoveTo(s_card, s_slot_x[s_slot], s_slot_y[s_slot], 390, GY_ANIM_OVERSHOOT);
	YMGUI_Label_SetText(s_status, "Released  /  snapped to nearest slot");
}

static void dragEvent(GYOBJ obj, GYEvent event)
{
	GYOBJ card = obj == s_card ? obj : obj->parent;
	if (event == GY_EVENT_Pressed)
	{
		GYrect abs;
		YMGUI_Anim_CancelObject(card);
		YMGUI_Obj_GetAbsArea(card, &abs);
		s_offset_x = card->ctx->point_x - abs.x;
		s_offset_y = card->ctx->point_y - abs.y;
		YMGUI_Label_SetText(s_status, "Dragging  /  release near a slot");
	}
	else if (event == GY_EVENT_Pressing)
	{
		GYcoord x = card->ctx->point_x - s_offset_x;
		GYcoord y = card->ctx->point_y - s_offset_y;
		if (x < 0) x = 0;
		if (x > UI_W - card->area.w) x = UI_W - card->area.w;
		if (y < 77) y = 77;
		if (y > 222 - card->area.h) y = 222 - card->area.h;
		ui_reposition(card, x, y);
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
		snap();
}

static void moveNext(GYOBJ button)
{
	(void)button;
	s_slot = (s_slot + 1) % 4;
	YMGUI_Anim_MoveTo(s_card, s_slot_x[s_slot], s_slot_y[s_slot], 420, GY_ANIM_OVERSHOOT);
	YMGUI_Label_SetText(s_status, "Snapping to the next slot");
}

int main(int argc, char** argv)
{
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "15  /  DRAG CARD SNAP", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "Drag the card; release to find nearest slot", UI_MUTED);
	for (uint8 i = 0; i < 4; ++i)
	{
		GYOBJ slot = ui_box(s_ui.ctx->root, s_slot_x[i], s_slot_y[i], 206, 62,
							 GY_ARGB(0xFF, 0x1B, 0x28, 0x40));
		ui_text(slot, 16, 20, 174, 22, "Drop target", UI_MUTED);
	}
	s_card = ui_box(s_ui.ctx->root, -206, 87, 206, 62, UI_PURPLE);
	ui_box(s_card, 0, 0, 6, 62, UI_CYAN)->event_cb = dragEvent;
	GYOBJ title = ui_text(s_card, 17, 12, 172, 22, "Drag this card", UI_WHITE);
	GYOBJ note = ui_text(s_card, 17, 35, 172, 20, "Pointer capture + tween", UI_WHITE);
	s_card->event_cb = dragEvent;
	title->event_cb = dragEvent;
	note->event_cb = dragEvent;
	s_status = ui_text(s_ui.ctx->root, 28, 233, 285, 22, "Drag or press Next", UI_CYAN);
	ui_button(s_ui.ctx->root, 350, 229, 102, 36, "Next", moveNext);
	YMGUI_Anim_MoveTo(s_card, s_slot_x[0], s_slot_y[0], 500, GY_ANIM_OVERSHOOT);
	if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
	{
		YMGUI_Inject_Tick(550);
		YMGUI_Inject_Pointer(100, 110, 1);
		YMGUI_Inject_Pointer(300, 186, 1);
		YMGUI_Inject_Pointer(300, 186, 0);
		YMGUI_Inject_Tick(450);
		int ok = s_slot == 3 && s_card->area.x == 246 && s_card->area.y == 160;
		ui_free(&s_ui);
		return ok ? 0 : 2;
	}
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
