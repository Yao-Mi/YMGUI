#ifndef DEMO_ANIM_UI_H
#define DEMO_ANIM_UI_H

#include "YMGUI_Anim.h"
#include "YMGUI_Button.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "SDL_LCD.h"
#include <stdlib.h>

#define UI_W 480
#define UI_H 272
#define UI_BG GY_ARGB(0xFF, 0x10, 0x17, 0x28)
#define UI_PANEL GY_ARGB(0xFF, 0x20, 0x2C, 0x44)
#define UI_WHITE GY_ARGB(0xFF, 0xED, 0xF3, 0xFF)
#define UI_MUTED GY_ARGB(0xFF, 0xA5, 0xB6, 0xD1)
#define UI_CYAN GY_ARGB(0xFF, 0x40, 0xD7, 0xDA)
#define UI_PURPLE GY_ARGB(0xFF, 0x76, 0x78, 0xEF)

typedef struct
{
	GYdisp disp;
	GYCTX ctx;
} DemoUI;

static int ui_init(DemoUI* ui)
{
	ui->disp = (GYdisp){0};
	ui->disp.hor_res = UI_W;
	ui->disp.ver_res = UI_H;
	ui->disp.buf_px_cnt = UI_W * 32;
	ui->disp.buf1 = (GYpx*)GY_malloc1(ui->disp.buf_px_cnt * sizeof(GYpx));
	if (ui->disp.buf1 == NULL)
		return 0;
	if (SDL_LCD_Init(&ui->disp, 1) != 0)
	{
		GY_free1(ui->disp.buf1);
		return 0;
	}
	ui->ctx = YMGUI_Creat_Ctx_Creat(&ui->disp, UI_W, UI_H);
	if (ui->ctx == NULL)
	{
		SDL_LCD_Destroy();
		GY_free1(ui->disp.buf1);
		return 0;
	}
	YMGUI_Obj_SetBgColor(ui->ctx->root, UI_BG);
	YMGUI_Inject_SetCtx(ui->ctx);
	return 1;
}

static void ui_run(DemoUI* ui, int argc, char** argv)
{
	int max_frames = argc > 1 ? atoi(argv[1]) : -1;
	int frame = 0;
	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(ui->ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}
}

static void ui_free(DemoUI* ui)
{
	YMGUI_Inject_SetCtx(NULL);
	YMGUI_Free_CtxFree(ui->ctx);
	SDL_LCD_Destroy();
	GY_free1(ui->disp.buf1);
}

static GYOBJ ui_box(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h, GYcolor color)
{
	GYOBJ box = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	YMGUI_Obj_SetBgColor(box, color);
	return box;
}

static void ui_reposition(GYOBJ obj, GYcoord x, GYcoord y)
{
	GYrect old_area;
	YMGUI_Obj_GetAbsArea(obj, &old_area);
	obj->area.x = x;
	obj->area.y = y;
	YMGUI_Ctx_InvalidateArea(obj->ctx, &old_area);
	YMGUI_Obj_Invalidate(obj);
}

static GYOBJ ui_text(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w,
					 GYcoord h, const char* text, GYcolor color)
{
	GYOBJ label = YMGUI_Creat_Label_Creat(parent, x, y, w, h);
	YMGUI_Label_SetText(label, text);
	YMGUI_Label_SetTextColor(label, color);
	return label;
}

static GYOBJ ui_button(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w,
					   GYcoord h, const char* text, GYbtn_clicked_cb callback)
{
	GYOBJ button = YMGUI_Creat_Button_Creat(parent, x, y, w, h);
	YMGUI_Button_SetText(button, text);
	YMGUI_Button_SetColors(button, UI_PURPLE, GY_ARGB(0xFF, 0x57, 0x5A, 0xC0));
	YMGUI_Button_SetClicked(button, callback);
	return button;
}

#endif
