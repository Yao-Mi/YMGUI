#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawArc.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_Joystick.h"
#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "SDL_LCD.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SCR_W 320
#define SCR_H 240
#define BAND_H 60
#define KNOB_TRAVEL 45

static GYOBJ s_joystick;
static GYOBJ s_output;
static GYOBJ s_value_label;
static GYOBJ s_level_label;
static int16 s_value_x;
static int16 s_value_y;

static void updateLabels(void)
{
	char text[40];
	int32 magnitude = (int32)sqrt((double)s_value_x * s_value_x + (double)s_value_y * s_value_y);
	if (magnitude > 100) magnitude = 100;
	snprintf(text, sizeof(text), "X %+4d   Y %+4d", (int)s_value_x, (int)s_value_y);
	YMGUI_Label_SetText(s_value_label, text);
	snprintf(text, sizeof(text), "Magnitude %3d%%", (int)magnitude);
	YMGUI_Label_SetText(s_level_label, text);
	YMGUI_Obj_Invalidate(s_output);
}

static void onJoystickChanged(GYOBJ joystick, int16 x, int16 y)
{
	(void)joystick;
	s_value_x = x;
	s_value_y = y;
	updateLabels();
}

static void onJoystickReleased(GYOBJ joystick, int16 x, int16 y)
{
	(void)joystick;
	gy_log_print("joystick released: x=%d y=%d\n", (int)x, (int)y);
}

//Demo 自定义皮肤：控件仍负责事件、限位、死区和回中，只替换绘制。
static void joystickDraw(GYOBJ joystick, GYSURFACE surface, const GYrect* abs)
{
	int16 value_x = 0, value_y = 0;
	YMGUI_Joystick_GetValue(joystick, &value_x, &value_y);
	GYcoord cx = abs->x + abs->w / 2;
	GYcoord cy = abs->y + abs->h / 2;
	GYcoord knob_x = cx + (GYcoord)((int32)value_x * KNOB_TRAVEL / GY_JOYSTICK_VALUE_MAX);
	GYcoord knob_y = cy - (GYcoord)((int32)value_y * KNOB_TRAVEL / GY_JOYSTICK_VALUE_MAX);
	GYcolor knob = YMGUI_Joystick_IsActive(joystick)
		? GY_ARGB(0xFF, 0xEF, 0xB3, 0x4D)
		: GY_ARGB(0xFF, 0x44, 0xC2, 0xB5);

	YMGUI_Draw_CircleFill(surface, cx, cy, 67, GY_ARGB(0xFF, 0x2C, 0x9A, 0xA0));
	YMGUI_Draw_CircleFill(surface, cx, cy, 63, GY_ARGB(0xFF, 0x1D, 0x2A, 0x31));
	YMGUI_Draw_Line(surface, cx - 51, cy, cx + 51, cy, GY_ARGB(0xFF, 0x4D, 0x5D, 0x63));
	YMGUI_Draw_Line(surface, cx, cy - 51, cx, cy + 51, GY_ARGB(0xFF, 0x4D, 0x5D, 0x63));
	YMGUI_Draw_CircleFill(surface, knob_x + 2, knob_y + 3, 24, GY_ARGB(0xFF, 0x0C, 0x12, 0x16));
	YMGUI_Draw_CircleFill(surface, knob_x, knob_y, 23, knob);
	YMGUI_Draw_CircleFill(surface, knob_x - 6, knob_y - 7, 6, GY_ARGB(0xFF, 0xA5, 0xE8, 0xDE));
}

static void outputDraw(GYOBJ obj, GYSURFACE surface, const GYrect* abs)
{
	(void)obj;
	GYcoord cx = abs->x + abs->w / 2;
	GYcoord cy = abs->y + abs->h / 2;
	GYcoord px = cx + (GYcoord)((int32)s_value_x * 42 / GY_JOYSTICK_VALUE_MAX);
	GYcoord py = cy - (GYcoord)((int32)s_value_y * 42 / GY_JOYSTICK_VALUE_MAX);
	GYcolor grid = GY_ARGB(0xFF, 0x50, 0x61, 0x68);

	YMGUI_Draw_CircleFill(surface, cx, cy, 49, GY_ARGB(0xFF, 0x22, 0x29, 0x2E));
	YMGUI_Draw_Circle(surface, cx, cy, 48, grid);
	YMGUI_Draw_Circle(surface, cx, cy, 24, GY_ARGB(0xFF, 0x3D, 0x49, 0x50));
	YMGUI_Draw_Line(surface, cx - 44, cy, cx + 44, cy, grid);
	YMGUI_Draw_Line(surface, cx, cy - 44, cx, cy + 44, grid);
	YMGUI_Draw_Line(surface, cx, cy, px, py, GY_ARGB(0xFF, 0xEF, 0xB3, 0x4D));
	YMGUI_Draw_CircleFill(surface, px, py, 6, GY_ARGB(0xFF, 0xEF, 0xB3, 0x4D));
}

static void toggleMode(GYOBJ button)
{
	uint8 auto_center = (uint8)!YMGUI_Joystick_GetAutoCenter(s_joystick);
	YMGUI_Joystick_SetAutoCenter(s_joystick, auto_center);
	YMGUI_Button_SetText(button, auto_center ? "AUTO" : "HOLD");
}

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;

	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL;
	disp.user_data = NULL;
	if (SDL_LCD_Init(&disp, 2) != 0)
	{
		GY_free1(disp.buf1);
		return 1;
	}

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x12, 0x18, 0x1C));
	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 0, 8, SCR_W, 20);
	YMGUI_Label_SetText(title, "Virtual Joystick");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xE9, 0xF1, 0xF2));

	s_joystick = YMGUI_Creat_Joystick_Creat(ctx->root, 14, 43, 150, 150);
	YMGUI_Joystick_SetDeadzone(s_joystick, 10);
	YMGUI_Joystick_SetChangedCb(s_joystick, onJoystickChanged);
	YMGUI_Joystick_SetReleasedCb(s_joystick, onJoystickReleased);
	YMGUI_Joystick_SetDrawCb(s_joystick, joystickDraw);

	s_output = YMGUI_Creat_Obj_Creat(ctx->root, 190, 45, 112, 112);
	s_output->draw_cb = outputDraw;
	s_output->event_cb = NULL;
	s_value_label = YMGUI_Creat_Label_Creat(ctx->root, 174, 166, 140, 18);
	YMGUI_Label_SetTextColor(s_value_label, GY_ARGB(0xFF, 0xE9, 0xF1, 0xF2));
	s_level_label = YMGUI_Creat_Label_Creat(ctx->root, 174, 190, 140, 18);
	YMGUI_Label_SetTextColor(s_level_label, GY_ARGB(0xFF, 0x8B, 0xA0, 0xA7));
	GYOBJ mode_button = YMGUI_Creat_Button_Creat(ctx->root, 211, 214, 70, 22);
	YMGUI_Button_SetText(mode_button, "AUTO");
	YMGUI_Button_SetColors(mode_button, GY_ARGB(0xFF, 0x31, 0x71, 0x76), GY_ARGB(0xFF, 0x20, 0x4D, 0x52));
	YMGUI_Button_SetClicked(mode_button, toggleMode);
	updateLabels();
	YMGUI_Obj_Invalidate(s_joystick);

	YMGUI_Inject_SetCtx(ctx);
	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Inject_SetCtx(NULL);
	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_joystick exit ok\n");
	return 0;
}
