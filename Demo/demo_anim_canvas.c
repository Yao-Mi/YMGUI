#include "demo_anim_ui.h"
#include "YMGUI_Bar.h"
#include "YMGUI_Canvas.h"
#include <stdio.h>

#define CHART_W 400
#define CHART_H 116

static DemoUI s_ui;
static GYOBJ s_canvas;
static GYOBJ s_bar;
static GYOBJ s_position;
static GYANIM s_play_anim;

static void chartChanged(int32 value, void* user_data)
{
	(void)user_data;
	GYpx* pixels = YMGUI_Canvas_GetBuffer(s_canvas);
	GYpx bg = GY_ColorToPx(GY_ARGB(0xFF, 0x18, 0x27, 0x41));
	GYpx faint = GY_ColorToPx(GY_ARGB(0xFF, 0x3B, 0x51, 0x73));
	GYpx live = GY_ColorToPx(UI_CYAN);
	GYpx cursor = GY_ColorToPx(GY_ARGB(0xFF, 0xFA, 0xC5, 0x68));
	int32 play_x = value * (CHART_W - 1) / 100;
	char label[32];
	for (int y = 0; y < CHART_H; ++y)
		for (int x = 0; x < CHART_W; ++x)
			pixels[y * CHART_W + x] = bg;
	for (int x = 5; x < CHART_W - 5; x += 5)
	{
		int height = 10 + ((x * 17 + x / 7 * 31) % 44);
		int mid = CHART_H / 2;
		for (int y = mid - height; y <= mid + height; ++y)
			pixels[y * CHART_W + x] = x <= play_x ? live : faint;
	}
	for (int y = 5; y < CHART_H - 5; ++y)
		pixels[y * CHART_W + play_x] = cursor;
	YMGUI_Canvas_Invalidate(s_canvas);
	YMGUI_Bar_SetValue(s_bar, value);
	snprintf(label, sizeof(label), "Position %ld%%", (long)value);
	YMGUI_Label_SetText(s_position, label);
}

static void finished(GYANIM animation, uint8 cancelled, void* user_data)
{
	(void)animation;
	(void)user_data;
	if (!cancelled)
		s_play_anim = 0;
}

static void play(GYOBJ button)
{
	(void)button;
	if (s_play_anim)
		YMGUI_Anim_Cancel(s_ui.ctx, s_play_anim);
	chartChanged(0, NULL);
	s_play_anim = YMGUI_Anim_ValueTo(s_ui.ctx, s_canvas, 0, 100, 2800,
								 GY_ANIM_LINEAR, chartChanged, NULL);
	YMGUI_Anim_SetDone(s_ui.ctx, s_play_anim, finished, NULL);
}

static void pause(GYOBJ button)
{
	(void)button;
	if (s_play_anim)
		YMGUI_Anim_Cancel(s_ui.ctx, s_play_anim);
	s_play_anim = 0;
}

int main(int argc, char** argv)
{
	if (!ui_init(&s_ui))
		return 1;
	ui_text(s_ui.ctx->root, 26, 17, 428, 24, "05  /  CANVAS PLAYHEAD", UI_CYAN);
	ui_text(s_ui.ctx->root, 28, 49, 420, 22, "One pixel canvas, with YMGUI playback controls", UI_MUTED);
	s_canvas = YMGUI_Creat_Canvas_Creat(s_ui.ctx->root, 40, 78,
											  CHART_W, CHART_H, CHART_W, CHART_H);
	s_bar = YMGUI_Creat_Bar_Creat(s_ui.ctx->root, 40, 202, CHART_W, 12);
	YMGUI_Bar_SetRange(s_bar, 0, 100);
	YMGUI_Bar_SetColors(s_bar, UI_PANEL, UI_CYAN);
	s_position = ui_text(s_ui.ctx->root, 40, 232, 200, 22, "Position 0%", UI_WHITE);
	ui_button(s_ui.ctx->root, 278, 228, 76, 33, "Replay", play);
	ui_button(s_ui.ctx->root, 364, 228, 76, 33, "Pause", pause);
	play(NULL);
	ui_run(&s_ui, argc, argv);
	ui_free(&s_ui);
	return 0;
}
