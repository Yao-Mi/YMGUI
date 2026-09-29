#include "demo_anim_ui.h"

static DemoUI s_ui;
static GYOBJ s_cards[4];
static uint8 s_swapped;

static const GYcoord s_x[4] = {28, 246, 28, 246};
static const GYcoord s_y[4] = {75, 75, 158, 158};

static void stagger(GYOBJ button)
{
	(void)button;
	s_swapped = 0;
	for (uint8 i = 0; i < 4; ++i)
	{
		ui_reposition(s_cards[i], i & 1 ? 490 : -214, s_y[i]);
		GYANIM motion = YMGUI_Anim_MoveTo(s_cards[i], s_x[i], s_y[i], 390, GY_ANIM_OVERSHOOT);
		YMGUI_Anim_SetDelay(s_ui.ctx, motion, i * 85);
	}
}

static void shuffle(GYOBJ button)
{
	(void)button;
	s_swapped = !s_swapped;
	for (uint8 i = 0; i < 4; ++i)
	{
		uint8 slot = s_swapped ? (uint8)(3 - i) : i;
		YMGUI_Anim_MoveTo(s_cards[i], s_x[slot], s_y[slot], 470, GY_ANIM_SMOOTH);
		YMGUI_Anim_BgColorTo(s_cards[i], s_swapped ? UI_PURPLE : UI_PANEL,
							  470, GY_ANIM_SMOOTH);
	}
}

int main(int argc, char** argv)
{
	static const char* names[4] = {"Inbox", "Calendar", "Files", "Analytics"};
	static const char* notes[4] = {"12 updates", "03 events", "08 folders", "Live view"};
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "03  /  STAGGERED CARDS", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 46, 405, 20, "A retained UI subtree moves with each card", UI_MUTED);
	for (uint8 i = 0; i < 4; ++i)
	{
		s_cards[i] = ui_box(s_ui.ctx->root, s_x[i], s_y[i], 206, 70, UI_PANEL);
		ui_box(s_cards[i], 0, 0, 6, 70, i & 1 ? UI_PURPLE : UI_CYAN);
		ui_text(s_cards[i], 18, 10, 176, 22, names[i], UI_WHITE);
		ui_text(s_cards[i], 18, 40, 176, 18, notes[i], UI_MUTED);
	}
	ui_button(s_ui.ctx->root, 264, 241, 90, 28, "Stagger", stagger);
	ui_button(s_ui.ctx->root, 366, 241, 86, 28, "Reorder", shuffle);
	stagger(NULL);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
