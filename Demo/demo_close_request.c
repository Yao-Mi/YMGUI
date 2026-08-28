#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "SDL_LCD.h"
#include <SDL2/SDL.h>
#include <stdlib.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_close_request.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-28
  *	@Description: SDL 标题栏关闭请求 demo:第一次取消,第二次允许退出
  *	@Version:     1.0
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

typedef struct
{
	GYOBJ status;
	int   attempts;
} CloseState;

static int onCloseRequest(void* user)
{
	CloseState* state = (CloseState*)user;
	state->attempts++;
	if (state->attempts == 1)
	{
		YMGUI_Label_SetText(state->status, "Close cancelled. Try again to exit.");
		gy_log_print("close request cancelled\n");
		return 0;
	}
	gy_log_print("close request accepted\n");
	return 1;
}

static void pushQuit(void)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_QUIT;
	SDL_PushEvent(&e);
}

int main(int argc, char** argv)
{
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;
	int fail = 0;
	GYdisp disp = {0};
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = SCR_W * BAND_H;
	disp.buf1 = (GYpx*)GY_malloc1((size_t)disp.buf_px_cnt * sizeof(GYpx));
	if (disp.buf1 == NULL || SDL_LCD_Init(&disp, 2) != 0)
	{
		GY_free1(disp.buf1);
		return 1;
	}

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	if (ctx == NULL)
	{
		SDL_LCD_Destroy();
		GY_free1(disp.buf1);
		return 1;
	}
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 20, 60, 280, 24);
	YMGUI_Label_SetText(title, "Window close request");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	CloseState state = {0};
	state.status = YMGUI_Creat_Label_Creat(ctx->root, 20, 110, 280, 40);
	YMGUI_Label_SetText(state.status, "Close the window to test");
	YMGUI_Label_SetTextColor(state.status, GY_ARGB(0xFF, 0xC0, 0xC0, 0xC0));

	YMGUI_Inject_SetCtx(ctx);
	SDL_LCD_SetCloseRequestCb(onCloseRequest, &state);

	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		frame++;
		if (max_frames >= 4 && (frame == 2 || frame == 3))
			pushQuit();
		else if (max_frames > 0 && frame >= max_frames)
			break;
	}

	if (max_frames >= 4)
	{
		fail = state.attempts != 2;
		gy_log_print(fail ? "selftest FAIL: close callback\n" :
		                    "selftest: close cancel + accept OK\n");
	}

	SDL_LCD_SetCloseRequestCb(NULL, NULL);
	YMGUI_Inject_SetCtx(NULL);
	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_close_request exit ok\n");
	return fail ? 1 : 0;
}
