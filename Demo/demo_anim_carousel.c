#include "demo_anim_ui.h"
#include <stdlib.h>
#include <string.h>

static DemoUI s_ui;
static GYOBJ s_cards[3];
static GYOBJ s_status;
static GYcoord s_drag_x;
static uint8 s_index;

static void alignCards(void)
{
	for (uint8 i = 0; i < 3; ++i)
		YMGUI_Anim_MoveTo(s_cards[i], 72 + ((int)i - (int)s_index) * 300, 16,
						 410, GY_ANIM_OVERSHOOT);
	YMGUI_Label_SetText(s_status, s_index == 0 ? "Card 1 / 3" :
								s_index == 1 ? "Card 2 / 3" : "Card 3 / 3");
}

static void carouselEvent(GYOBJ obj, GYEvent event)
{
	(void)obj;
	if (event == GY_EVENT_Pressed)
	{
		for (uint8 i = 0; i < 3; ++i)
			YMGUI_Anim_CancelObject(s_cards[i]);
		s_drag_x = s_ui.ctx->point_x;
	}
	else if (event == GY_EVENT_Pressing)
	{
		GYcoord delta = s_ui.ctx->point_x - s_drag_x;
		s_drag_x = s_ui.ctx->point_x;
		for (uint8 i = 0; i < 3; ++i)
			ui_reposition(s_cards[i], s_cards[i]->area.x + delta, 16);
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
	{
		// 以当前卡片中心距视口中心的实际距离决定落点；中途重抓也适用。
		int32 nearest = INT32_MAX;
		for (uint8 i = 0; i < 3; ++i)
		{
			int32 distance = s_cards[i]->area.x + s_cards[i]->area.w / 2 - 212;
			if (distance < 0) distance = -distance;
			if (distance < nearest)
			{
				nearest = distance;
				s_index = i;
			}
		}
		alignCards();
	}
}

static void previous(GYOBJ button)
{
	(void)button;
	if (s_index > 0) --s_index;
	alignCards();
}

static void next(GYOBJ button)
{
	(void)button;
	if (s_index < 2) ++s_index;
	alignCards();
}

int main(int argc, char** argv)
{
	static const char* titles[3] = {"PLAN", "BUILD", "SHIP"};
	static const char* notes[3] = {"Sketch the interaction", "Create real components", "Review the result"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "20  /  SWIPE CAROUSEL", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 43, 420, 20, "Swipe cards and release; nearest page snaps", UI_MUTED);
	GYOBJ viewport = ui_box(s_ui.ctx->root, 28, 79, 424, 143, UI_PANEL);
	viewport->state |= GY_STATE_ClipChildren;
	for (uint8 i = 0; i < 3; ++i)
	{
		s_cards[i] = ui_box(viewport, 72 + i * 300, 16, 280, 111,
							 i == 1 ? UI_PURPLE : GY_ARGB(0xFF, 0x31, 0x48, 0x68));
		s_cards[i]->event_cb = carouselEvent;
		GYOBJ title = ui_text(s_cards[i], 23, 21, 232, 31, titles[i], UI_WHITE);
		GYOBJ note = ui_text(s_cards[i], 23, 63, 232, 24, notes[i], UI_WHITE);
		title->event_cb = carouselEvent;
		note->event_cb = carouselEvent;
	}
	s_status = ui_text(s_ui.ctx->root, 28, 240, 180, 22, "Card 1 / 3", UI_CYAN);
	ui_button(s_ui.ctx->root, 244, 232, 98, 34, "Previous", previous);
	ui_button(s_ui.ctx->root, 352, 232, 100, 34, "Next", next);
	next(NULL);
	if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
	{
		YMGUI_Inject_Tick(450);
		YMGUI_Inject_Pointer(200, 140, 1);
		YMGUI_Inject_Pointer(120, 140, 1);
		YMGUI_Inject_Pointer(120, 140, 0);
		YMGUI_Inject_Tick(450);
		int short_drag_stayed = s_index == 1 && s_cards[1]->area.x == 72;
		YMGUI_Inject_Pointer(200, 140, 1);
		YMGUI_Inject_Pointer(25, 140, 1);
		YMGUI_Inject_Pointer(25, 140, 0);
		YMGUI_Inject_Tick(450);
		int ok = short_drag_stayed && s_index == 2 && s_cards[2]->area.x == 72;
		ui_free(&s_ui);
		return ok ? 0 : 2;
	}
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
