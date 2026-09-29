#include "demo_anim_ui.h"

static DemoUI s_ui;
static GYOBJ s_buttons[2];
static GYOBJ s_indicator;
static GYOBJ s_panel;
static GYOBJ s_banner;
static GYOBJ s_swatches[3];
static GYOBJ s_palette_name;

static void setTheme(GYOBJ button)
{
	uint8 sunset = button == s_buttons[1];
	const GYcolor ocean[3] = {
		GY_ARGB(0xFF, 0x40, 0xD7, 0xDA),
		GY_ARGB(0xFF, 0x45, 0x8A, 0xDB),
		GY_ARGB(0xFF, 0x76, 0x78, 0xEF)
	};
	const GYcolor warm[3] = {
		GY_ARGB(0xFF, 0xE8, 0x79, 0x8D),
		GY_ARGB(0xFF, 0xEF, 0xA7, 0x75),
		GY_ARGB(0xFF, 0xF0, 0xC5, 0x75)
	};
	YMGUI_Anim_BgColorTo(s_ui.ctx->root,
						  sunset ? GY_ARGB(0xFF, 0x28, 0x19, 0x2B) : UI_BG,
						  650, GY_ANIM_SMOOTH);
	YMGUI_Anim_BgColorTo(s_panel,
						  sunset ? GY_ARGB(0xFF, 0x4C, 0x32, 0x50) : UI_PANEL,
						  650, GY_ANIM_SMOOTH);
	YMGUI_Anim_BgColorTo(s_banner, sunset ? warm[0] : ocean[0], 650, GY_ANIM_SMOOTH);
	for (uint8 i = 0; i < 3; ++i)
		YMGUI_Anim_BgColorTo(s_swatches[i], sunset ? warm[i] : ocean[i],
							 650, GY_ANIM_SMOOTH);
	YMGUI_Anim_MoveTo(s_indicator, sunset ? 244 : 28, 105, 470, GY_ANIM_EASE_OUT);
	YMGUI_Anim_BgColorTo(s_indicator, sunset ? warm[0] : ocean[0], 650, GY_ANIM_SMOOTH);
	YMGUI_Label_SetText(s_palette_name, sunset ? "Sunset palette" : "Ocean palette");
}

int main(int argc, char** argv)
{
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "10  /  THEME TRANSITION", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 44, 420, 20, "Coordinated color tweens across the UI", UI_MUTED);
	s_buttons[0] = ui_button(s_ui.ctx->root, 28, 70, 206, 31, "Ocean", setTheme);
	s_buttons[1] = ui_button(s_ui.ctx->root, 244, 70, 208, 31, "Sunset", setTheme);
	s_indicator = ui_box(s_ui.ctx->root, 28, 105, 206, 4, UI_CYAN);
	s_panel = ui_box(s_ui.ctx->root, 28, 120, 424, 128, UI_PANEL);
	s_banner = ui_box(s_panel, 14, 15, 8, 98, UI_CYAN);
	s_palette_name = ui_text(s_panel, 38, 13, 354, 22, "Ocean palette", UI_WHITE);
	ui_text(s_panel, 38, 39, 354, 21, "A theme is more than one color", UI_MUTED);
	for (uint8 i = 0; i < 3; ++i)
		s_swatches[i] = ui_box(s_panel, 38 + i * 119, 75, 105, 32,
								  i == 0 ? UI_CYAN : i == 1 ?
								  GY_ARGB(0xFF, 0x45, 0x8A, 0xDB) : UI_PURPLE);
	setTheme(s_buttons[1]);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
