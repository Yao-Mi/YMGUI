#include "YMGUI_Anim.h"
#include "YMGUI_Button.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_PubDefine.h"
#include "SDL_LCD.h"
#include <stdio.h>
#include <stdlib.h>

#define SCR_W 480
#define SCR_H 272

typedef struct
{
	GYCTX ctx;
	GYOBJ card;
	GYOBJ pulse;
	GYOBJ value_label;
	uint8 forward;
} DemoState;

static DemoState s_demo;

static void valueChanged(int32 value, void* user_data)
{
	GYOBJ label = (GYOBJ)user_data;
	char text[48];
	snprintf(text, sizeof(text), "Animated value  %ld", (long)value);
	YMGUI_Label_SetText(label, text);
}

static void startWave(DemoState* demo);

static void waveDone(GYANIM animation, uint8 cancelled, void* user_data)
{
	(void)animation;
	if (!cancelled)
	{
		DemoState* demo = (DemoState*)user_data;
		demo->forward = !demo->forward;
		startWave(demo);
	}
}

static void startWave(DemoState* demo)
{
	GYcoord x = demo->forward ? 286 : 34;
	GYcolor color = demo->forward ? GY_ARGB(0xFF, 0x36, 0xC2, 0xA2) : GY_ARGB(0xFF, 0x48, 0x68, 0xE8);
	GYANIM move = YMGUI_Anim_MoveTo(demo->card, x, 74, 1100, GY_ANIM_OVERSHOOT);
	YMGUI_Anim_SetDelay(demo->ctx, move, 180);
	YMGUI_Anim_SetDone(demo->ctx, move, waveDone, demo);
	YMGUI_Anim_BgColorTo(demo->card, color, 900, GY_ANIM_SMOOTH);
	YMGUI_Anim_ResizeTo(demo->pulse, demo->forward ? 178 : 112, demo->forward ? 54 : 38,
						720, GY_ANIM_THERE_AND_BACK);
	YMGUI_Anim_ValueTo(demo->ctx, demo->value_label, demo->forward ? 0 : 100,
					   demo->forward ? 100 : 0, 1100, GY_ANIM_EASE_OUT, valueChanged, demo->value_label);
}

static void replayClicked(GYOBJ button)
{
	(void)button;
	YMGUI_Anim_CancelAll(s_demo.ctx);
	s_demo.forward = 1;
	startWave(&s_demo);
}

int main(int argc, char** argv)
{
	int max_frames = argc > 1 ? atoi(argv[1]) : -1;
	int frame = 0;
	GYdisp disp = {0};
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = SCR_W * 32;
	disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
	if (disp.buf1 == NULL || SDL_LCD_Init(&disp, 1) != 0)
		return 1;
	s_demo.ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(s_demo.ctx->root, GY_ARGB(0xFF, 0x10, 0x16, 0x24));

	GYOBJ title = YMGUI_Creat_Label_Creat(s_demo.ctx->root, 28, 20, 420, 26);
	YMGUI_Label_SetText(title, "YMGUI ANIM / context-driven tweening");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF2, 0xC7, 0x64));
	s_demo.card = YMGUI_Creat_Obj_Creat(s_demo.ctx->root, 34, 74, 160, 82);
	YMGUI_Obj_SetBgColor(s_demo.card, GY_ARGB(0xFF, 0x48, 0x68, 0xE8));
	GYOBJ card_title = YMGUI_Creat_Label_Creat(s_demo.card, 14, 14, 130, 20);
	YMGUI_Label_SetText(card_title, "Move + color");
	GYOBJ card_note = YMGUI_Creat_Label_Creat(s_demo.card, 14, 44, 132, 18);
	YMGUI_Label_SetText(card_note, "dirty: old/new");

	s_demo.pulse = YMGUI_Creat_Obj_Creat(s_demo.ctx->root, 34, 182, 112, 38);
	YMGUI_Obj_SetBgColor(s_demo.pulse, GY_ARGB(0xFF, 0xE0, 0x68, 0x88));
	s_demo.value_label = YMGUI_Creat_Label_Creat(s_demo.ctx->root, 236, 190, 210, 24);
	YMGUI_Label_SetText(s_demo.value_label, "Animated value  0");
	GYOBJ replay = YMGUI_Creat_Button_Creat(s_demo.ctx->root, 332, 226, 116, 32);
	YMGUI_Button_SetText(replay, "Replay");
	YMGUI_Button_SetClicked(replay, replayClicked);

	YMGUI_Inject_SetCtx(s_demo.ctx);
	s_demo.forward = 1;
	startWave(&s_demo);
	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(s_demo.ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Inject_SetCtx(NULL);
	YMGUI_Free_CtxFree(s_demo.ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	return 0;
}
