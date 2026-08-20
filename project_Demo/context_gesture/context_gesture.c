#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_TextView.h"
#include "SDL_LCD.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCR_W 480
#define SCR_H 272
#define BAND_H 48
#define CARD_COUNT 3
#define LOG_LINES 8
#define LOG_LINE_MAX 48

#define WORK_X 12
#define WORK_Y 46
#define WORK_W 304
#define WORK_H 194

#define MENU_W 132
#define MENU_ROW_H 22
#define MENU_H (MENU_ROW_H * 3)
#define CARD_W 130
#define CARD_H 58

typedef struct AppState AppState;

typedef struct
{
	GYOBJ obj;
	AppState* app;
	const char* id;
	const char* name;
	GYcolor color;
	GYcoord home_x, home_y;
	GYcoord start_x, start_y;
	GYcoord grab_x, grab_y;
	uint8 dragged;
	uint8 active;
} CardState;

typedef struct
{
	AppState* app;
	uint8 action;
	const char* text;
} MenuRow;

struct AppState
{
	GYCTX ctx;
	CardState cards[CARD_COUNT];
	GYOBJ status;
	GYOBJ capture;
	GYOBJ position;
	GYOBJ log_view;
	GYOBJ backdrop;
	GYOBJ menu;
	GYOBJ menu_rows[3];
	MenuRow row_data[3];
	CardState* menu_target;
	char log_lines[LOG_LINES][LOG_LINE_MAX];
	uint8 log_count;
	uint16 serial;
	uint16 drag_count;
	CardState* last_drag_card;
};

static AppState g_app;

static void fillBorder(GYSURFACE s, const GYrect* a, GYcolor fill, GYcolor border)
{
	YMGUI_Draw_Fill(s, a, fill, GY_OPA_COVER);
	GYrect top = {a->x, a->y, a->w, 1};
	GYrect bot = {a->x, (GYcoord)(a->y + a->h - 1), a->w, 1};
	GYrect lft = {a->x, a->y, 1, a->h};
	GYrect rgt = {(GYcoord)(a->x + a->w - 1), a->y, 1, a->h};
	YMGUI_Draw_Fill(s, &top, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &bot, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &lft, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &rgt, border, GY_OPA_COVER);
}

static void panelDraw(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	(void)obj;
	fillBorder(s, abs, GY_ARGB(0xFF, 0x20, 0x24, 0x2C), GY_ARGB(0xFF, 0x45, 0x4C, 0x59));
}

static void cardDraw(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	CardState* c = (CardState*)obj->user_data;
	fillBorder(s, abs, c->color, c->active ? GY_ARGB(0xFF, 0xFF, 0xE0, 0x70)
	                                              : GY_ARGB(0xFF, 0xE8, 0xEC, 0xF2));
	GYFONT font = &YMGUI_Font_Default;
	YMGUI_Draw_Text(s, font, (GYcoord)(abs->x + 9), (GYcoord)(abs->y + 9), c->name,
	                GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	YMGUI_Draw_Text(s, font, (GYcoord)(abs->x + 9), (GYcoord)(abs->y + 31),
	                c->active ? "CONTEXT DRAG" : "MENU OR DRAG",
	                GY_ARGB(0xFF, 0xF4, 0xF6, 0xFA));
}

static void rebuildLog(AppState* app)
{
	char text[LOG_LINES * LOG_LINE_MAX];
	int off = 0;
	text[0] = '\0';
	for (uint8 i = 0; i < app->log_count; i++)
	{
		int n = snprintf(text + off, sizeof(text) - (size_t)off, "%s%s",
		                 i ? "\n" : "", app->log_lines[i]);
		if (n < 0 || n >= (int)(sizeof(text) - (size_t)off))
			break;
		off += n;
	}
	YMGUI_TextView_SetText(app->log_view, text);
	GYcoord content = (GYcoord)(YMGUI_TextView_GetLineCount(app->log_view) * 16);
	YMGUI_TextView_SetScroll(app->log_view,
	                         content > app->log_view->area.h ? content - app->log_view->area.h : 0);
}

static void appendLog(AppState* app, const char* kind, CardState* c, GYcoord x, GYcoord y)
{
	char line[LOG_LINE_MAX];
	app->serial++;
	snprintf(line, sizeof(line), "%02u %-6s %s %d,%d", app->serial, kind, c->id, x, y);
	if (app->log_count == LOG_LINES)
	{
		for (int i = 1; i < LOG_LINES; i++)
			memcpy(app->log_lines[i - 1], app->log_lines[i], LOG_LINE_MAX);
		app->log_count--;
	}
	snprintf(app->log_lines[app->log_count++], LOG_LINE_MAX, "%s", line);
	app->last_drag_card = NULL;
	app->drag_count = 0;
	rebuildLog(app);
}

static void appendDrag(AppState* app, CardState* c, GYcoord x, GYcoord y)
{
	if (app->last_drag_card != c || app->log_count == 0)
	{
		if (app->log_count == LOG_LINES)
		{
			for (int i = 1; i < LOG_LINES; i++)
				memcpy(app->log_lines[i - 1], app->log_lines[i], LOG_LINE_MAX);
			app->log_count--;
		}
		app->serial++;
		app->drag_count = 0;
		app->last_drag_card = c;
		app->log_count++;
	}
	app->drag_count++;
	snprintf(app->log_lines[app->log_count - 1], LOG_LINE_MAX,
	         "%02u DRAGx%-2u %s %d,%d", app->serial, app->drag_count, c->id, x, y);
	rebuildLog(app);
}

static void setStatus(AppState* app, const char* phase, CardState* c, GYcoord x, GYcoord y)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%s / CARD %s", phase, c != NULL ? c->id : "-");
	YMGUI_Label_SetText(app->status, buf);
	snprintf(buf, sizeof(buf), "CAPTURE: %s", app->ctx->context_obj != NULL && c != NULL ? c->name : "none");
	YMGUI_Label_SetText(app->capture, buf);
	snprintf(buf, sizeof(buf), "POS: %d, %d", x, y);
	YMGUI_Label_SetText(app->position, buf);
}

static void closeMenu(AppState* app)
{
	YMGUI_Obj_SetHidden(app->menu, 1);
	YMGUI_Obj_SetHidden(app->backdrop, 1);
	app->menu_target = NULL;
}

static void openMenu(AppState* app, CardState* c, GYcoord x, GYcoord y)
{
	GYcoord mx = (GYcoord)(x + 6);
	GYcoord my = (GYcoord)(y + 6);
	if (mx + MENU_W > SCR_W - 4) mx = (GYcoord)(SCR_W - MENU_W - 4);
	if (my + MENU_H > SCR_H - 4) my = (GYcoord)(SCR_H - MENU_H - 4);
	if (mx < 4) mx = 4;
	if (my < 4) my = 4;
	app->menu->area.x = mx;
	app->menu->area.y = my;
	app->menu_target = c;
	YMGUI_Obj_SetHidden(app->backdrop, 0);
	YMGUI_Obj_SetHidden(app->menu, 0);
	setStatus(app, "MENU", c, x, y);
}

static void moveCard(CardState* c, GYcoord x, GYcoord y)
{
	GYcoord nx = (GYcoord)(x - c->grab_x);
	GYcoord ny = (GYcoord)(y - c->grab_y);
	GYcoord min_x = WORK_X + 8;
	GYcoord min_y = WORK_Y + 8;
	GYcoord max_x = WORK_X + WORK_W - c->obj->area.w - 8;
	GYcoord max_y = WORK_Y + WORK_H - c->obj->area.h - 8;
	if (nx < min_x) nx = min_x;
	if (ny < min_y) ny = min_y;
	if (nx > max_x) nx = max_x;
	if (ny > max_y) ny = max_y;
	if (nx == c->obj->area.x && ny == c->obj->area.y)
		return;
	YMGUI_Obj_Invalidate(c->obj);
	c->obj->area.x = nx;
	c->obj->area.y = ny;
	YMGUI_Obj_Invalidate(c->obj);
}

static void restoreCard(CardState* c, GYcoord x, GYcoord y)
{
	YMGUI_Obj_Invalidate(c->obj);
	c->obj->area.x = x;
	c->obj->area.y = y;
	YMGUI_Obj_Invalidate(c->obj);
}

static void cardEvent(GYOBJ obj, GYEvent e)
{
	CardState* c = (CardState*)obj->user_data;
	AppState* app = c->app;
	GYcoord x = obj->ctx->point_x;
	GYcoord y = obj->ctx->point_y;
	if (e == GY_EVENT_ContextRequested)
	{
		if (obj->ctx->context_obj == obj)
		{
			c->start_x = obj->area.x;
			c->start_y = obj->area.y;
			c->grab_x = (GYcoord)(x - obj->area.x);
			c->grab_y = (GYcoord)(y - obj->area.y);
			c->dragged = 0;
			c->active = 1;
			YMGUI_Obj_Invalidate(obj);
			appendLog(app, "REQCAP", c, x, y);
			setStatus(app, "REQUESTED", c, x, y);
		}
		else
		{
			appendLog(app, "REQCLK", c, x, y);
			openMenu(app, c, x, y);
		}
	}
	else if (e == GY_EVENT_ContextDragging)
	{
		c->dragged = 1;
		moveCard(c, x, y);
		appendDrag(app, c, x, y);
		setStatus(app, "DRAGGING", c, x, y);
	}
	else if (e == GY_EVENT_ContextReleased)
	{
		c->active = 0;
		YMGUI_Obj_Invalidate(obj);
		appendLog(app, "RELEASE", c, x, y);
		if (c->dragged)
			setStatus(app, "RELEASED", c, x, y);
		else
			openMenu(app, c, x, y);
	}
	else if (e == GY_EVENT_ContextCancelled)
	{
		c->active = 0;
		restoreCard(c, c->start_x, c->start_y);
		appendLog(app, "CANCEL", c, x, y);
		setStatus(app, "CANCELLED", c, x, y);
	}
}

static void backdropEvent(GYOBJ obj, GYEvent e)
{
	if (e == GY_EVENT_Clicked)
		closeMenu((AppState*)obj->user_data);
}

static void menuDraw(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	(void)obj;
	fillBorder(s, abs, GY_ARGB(0xFF, 0x2B, 0x30, 0x3A), GY_ARGB(0xFF, 0xA4, 0xAD, 0xBA));
}

static void menuRowDraw(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	MenuRow* row = (MenuRow*)obj->user_data;
	GYcolor bg = (obj->state & GY_STATE_Pressed) ? GY_ARGB(0xFF, 0x45, 0x50, 0x62)
	                                                   : GY_ARGB(0xFF, 0x2B, 0x30, 0x3A);
	YMGUI_Draw_Fill(s, abs, bg, GY_OPA_COVER);
	YMGUI_Draw_Text(s, &YMGUI_Font_Default, (GYcoord)(abs->x + 8), (GYcoord)(abs->y + 5),
	                row->text, GY_ARGB(0xFF, 0xF0, 0xF3, 0xF7));
}

static void menuRowEvent(GYOBJ obj, GYEvent e)
{
	MenuRow* row = (MenuRow*)obj->user_data;
	if (e == GY_EVENT_Pressed || e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff)
		YMGUI_Obj_Invalidate(obj);
	if (e != GY_EVENT_Clicked)
		return;
	AppState* app = row->app;
	CardState* c = app->menu_target;
	if (c != NULL && row->action == 0)
	{
		appendLog(app, "INSPECT", c, c->obj->area.x, c->obj->area.y);
		setStatus(app, "INSPECTED", c, c->obj->area.x, c->obj->area.y);
	}
	else if (c != NULL && row->action == 1)
	{
		restoreCard(c, c->home_x, c->home_y);
		appendLog(app, "RESET", c, c->home_x, c->home_y);
		setStatus(app, "RESET", c, c->home_x, c->home_y);
	}
	closeMenu(app);
}

static GYOBJ makeLabel(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h,
                       const char* text, GYcolor color)
{
	GYOBJ label = YMGUI_Creat_Label_Creat(parent, x, y, w, h);
	YMGUI_Label_SetText(label, text);
	YMGUI_Label_SetTextColor(label, color);
	return label;
}

static void buildMenu(AppState* app)
{
	GYOBJ top = YMGUI_Ctx_GetTopLayer(app->ctx);
	app->backdrop = YMGUI_Creat_Obj_Creat(top, 0, 0, SCR_W, SCR_H);
	app->backdrop->draw_cb = NULL;
	app->backdrop->event_cb = backdropEvent;
	app->backdrop->user_data = app;
	app->menu = YMGUI_Creat_Obj_Creat(top, 0, 0, MENU_W, MENU_H);
	app->menu->draw_cb = menuDraw;
	const char* names[3] = {"Inspect card", "Reset position", "Dismiss"};
	for (int i = 0; i < 3; i++)
	{
		app->row_data[i] = (MenuRow){app, (uint8)i, names[i]};
		app->menu_rows[i] = YMGUI_Creat_Obj_Creat(app->menu, 1, (GYcoord)(i * MENU_ROW_H + 1),
		                                           MENU_W - 2, MENU_ROW_H - 1);
		app->menu_rows[i]->draw_cb = menuRowDraw;
		app->menu_rows[i]->event_cb = menuRowEvent;
		app->menu_rows[i]->user_data = &app->row_data[i];
	}
	YMGUI_Obj_SetHidden(app->backdrop, 1);
	YMGUI_Obj_SetHidden(app->menu, 1);
}

static void buildCard(AppState* app, int i, GYcoord x, GYcoord y,
                      const char* id, const char* name, GYcolor color)
{
	CardState* c = &app->cards[i];
	*c = (CardState){0};
	c->app = app;
	c->id = id;
	c->name = name;
	c->color = color;
	c->home_x = x;
	c->home_y = y;
	c->obj = YMGUI_Creat_Obj_Creat(app->ctx->root, x, y, CARD_W, CARD_H);
	c->obj->draw_cb = cardDraw;
	c->obj->event_cb = cardEvent;
	c->obj->user_data = c;
}

static void buildUi(AppState* app)
{
	YMGUI_Obj_SetBgColor(app->ctx->root, GY_ARGB(0xFF, 0x15, 0x18, 0x1E));
	makeLabel(app->ctx->root, 12, 5, 456, 17, "CONTEXT GESTURE LAB",
	          GY_ARGB(0xFF, 0xF2, 0xC8, 0x55));
	makeLabel(app->ctx->root, 12, 23, 456, 16,
	          "Right-click: menu | Right-drag / touch-hold + drag: move",
	          GY_ARGB(0xFF, 0xB8, 0xC0, 0xCC));
	GYOBJ work = YMGUI_Creat_Obj_Creat(app->ctx->root, WORK_X, WORK_Y, WORK_W, WORK_H);
	work->draw_cb = panelDraw;
	GYOBJ side = YMGUI_Creat_Obj_Creat(app->ctx->root, 324, 46, 144, 194);
	side->draw_cb = panelDraw;

	buildCard(app, 0, 28, 70, "A", "CARD A / BLUE", GY_ARGB(0xFF, 0x39, 0x87, 0xE5));
	buildCard(app, 1, 174, 70, "B", "CARD B / ORANGE", GY_ARGB(0xFF, 0xD9, 0x59, 0x26));
	buildCard(app, 2, 101, 150, "C", "CARD C / AQUA", GY_ARGB(0xFF, 0x19, 0x9E, 0x70));

	makeLabel(app->ctx->root, 332, 53, 128, 14, "EVENT STATE",
	          GY_ARGB(0xFF, 0xF2, 0xC8, 0x55));
	app->status = makeLabel(app->ctx->root, 332, 70, 128, 15, "IDLE / CARD -",
	                        GY_ARGB(0xFF, 0xF0, 0xF3, 0xF7));
	app->capture = makeLabel(app->ctx->root, 332, 87, 128, 15, "CAPTURE: none",
	                         GY_ARGB(0xFF, 0xB8, 0xC0, 0xCC));
	app->position = makeLabel(app->ctx->root, 332, 104, 128, 15, "POS: -, -",
	                          GY_ARGB(0xFF, 0xB8, 0xC0, 0xCC));
	makeLabel(app->ctx->root, 332, 124, 128, 14, "EVENT STREAM",
	          GY_ARGB(0xFF, 0xF2, 0xC8, 0x55));
	app->log_view = YMGUI_Creat_TextView_Creat(app->ctx->root, 332, 142, 128, 88);
	YMGUI_TextView_SetTextColor(app->log_view, GY_ARGB(0xFF, 0xD8, 0xDE, 0xE8));
	YMGUI_TextView_SetLineHeight(app->log_view, 16);
	YMGUI_TextView_SetText(app->log_view, "Waiting for context input...");
	makeLabel(app->ctx->root, 12, 249, 456, 16,
	          "Focus loss cancels and restores the drag origin",
	          GY_ARGB(0xFF, 0x8F, 0x9A, 0xA8));
	buildMenu(app);
}

static void buildShotScene(AppState* app)
{
	CardState* a = &app->cards[0];
	CardState* b = &app->cards[1];
	CardState* c = &app->cards[2];
	YMGUI_Inject_ContextBegin((GYcoord)(a->obj->area.x + 20), (GYcoord)(a->obj->area.y + 20));
	YMGUI_Inject_ContextMove(85, 132);
	YMGUI_Inject_ContextEnd(85, 132);
	YMGUI_Inject_ContextBegin((GYcoord)(b->obj->area.x + 20), (GYcoord)(b->obj->area.y + 20));
	YMGUI_Inject_ContextMove(235, 160);
	YMGUI_Inject_ContextCancel();
	YMGUI_Inject_ContextRequest((GYcoord)(c->obj->area.x + 34), (GYcoord)(c->obj->area.y + 18));
}

int main(int argc, char** argv)
{
	GYdisp disp = {0};
	int max_frames = argc > 1 ? atoi(argv[1]) : -1;
	int frame = 0;
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = SCR_W * BAND_H;
	disp.buf1 = (GYpx*)GY_malloc1((size_t)disp.buf_px_cnt * sizeof(GYpx));
	if (disp.buf1 == NULL)
		return 1;
	if (SDL_LCD_Init(&disp, 2) != 0)
	{
		GY_free1(disp.buf1);
		return 1;
	}
	memset(&g_app, 0, sizeof(g_app));
	g_app.ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	if (g_app.ctx == NULL)
	{
		SDL_LCD_Destroy();
		GY_free1(disp.buf1);
		return 1;
	}
	buildUi(&g_app);
	YMGUI_Inject_SetCtx(g_app.ctx);
	if (getenv("YMGUI_SHOT") != NULL)
		buildShotScene(&g_app);

	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(g_app.ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Inject_SetCtx(NULL);
	YMGUI_Free_CtxFree(g_app.ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("context_gesture exit ok\n");
	return 0;
}
