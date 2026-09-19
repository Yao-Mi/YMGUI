#include "SDL_LCD.h"
#include "YMGUI_Event.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include <SDL2/SDL.h>
#include <stdio.h>

#define SCR_W 100
#define SCR_H 80

static int fails;
static GYEvent events[32];
static int event_count;
static uint32 key_values[4];
static int key_count;
static int close_calls;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static void flushCb(GYdisp* d, const GYrect* area, const GYpx* buf)
{
	(void)area; (void)buf;
	YMGUI_Disp_FlushReady(d);
}

static void onEvent(GYOBJ obj, GYEvent e)
{
	if (e == GY_EVENT_Key && key_count < (int)(sizeof(key_values) / sizeof(key_values[0])))
		key_values[key_count++] = obj->ctx->last_key;
	if ((e == GY_EVENT_ContextRequested || e == GY_EVENT_ContextDragging ||
	     e == GY_EVENT_ContextReleased || e == GY_EVENT_ContextCancelled) &&
	    event_count < (int)(sizeof(events) / sizeof(events[0])))
		events[event_count++] = e;
}

static void pushKey(SDL_Keycode key)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_KEYDOWN;
	e.key.state = SDL_PRESSED;
	e.key.keysym.sym = key;
	SDL_PushEvent(&e);
}

static void resetEvents(void)
{
	event_count = 0;
}

static void pushButton(Uint32 type, Uint8 button, int x, int y)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = type;
	e.button.which = 1;
	e.button.button = button;
	e.button.x = x;
	e.button.y = y;
	SDL_PushEvent(&e);
}

static void pushMotion(Uint32 state, int x, int y)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_MOUSEMOTION;
	e.motion.which = 1;
	e.motion.state = state;
	e.motion.x = x;
	e.motion.y = y;
	SDL_PushEvent(&e);
}

static void pushFinger(Uint32 type, float x, float y)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = type;
	e.tfinger.timestamp = SDL_GetTicks();
	e.tfinger.fingerId = 7;
	e.tfinger.x = x;
	e.tfinger.y = y;
	SDL_PushEvent(&e);
}

static int onCloseRequest(void* user)
{
	close_calls++;
	return *(int*)user;
}

static void pushQuit(void)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_QUIT;
	SDL_PushEvent(&e);
}

int main(void)
{
	CHECK(SDL_LCD_WindowId() == 0, "no window ID before initialization");
	CHECK(SDL_LCD_SetTitle("before init") == 0, "no title update before initialization");
	GYdisp disp = {0};
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = SCR_W * 10;
	disp.buf1 = (GYpx*)GY_malloc1((size_t)disp.buf_px_cnt * sizeof(GYpx));
	disp.flush_cb = flushCb;
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	ctx->root->event_cb = onEvent;
	YMGUI_Inject_SetCtx(ctx);

	CHECK(SDL_LCD_Init(&disp, 1) == 0, "SDL LCD initializes with dummy driver");
	SDL_Window* window = SDL_GetWindowFromID(SDL_LCD_WindowId());
	CHECK(window != NULL, "window ID identifies the live SDL window");
	CHECK(SDL_LCD_SetTitle(NULL) == 0, "null title rejected");
	CHECK(SDL_LCD_SetTitle("YMGUI SDK") == 1, "window title update succeeds");
	CHECK(window && SDL_strcmp(SDL_GetWindowTitle(window), "YMGUI SDK") == 0,
	      "title update reaches SDL");

	//主键盘 Enter 与数字小键盘 Enter 必须统一为同一个 YMGUI 虚拟键。
	key_count = 0;
	YMGUI_SetFocus(ctx, ctx->root);
	pushKey(SDLK_RETURN);
	pushKey(SDLK_KP_ENTER);
	SDL_LCD_PumpEvents();
	CHECK(key_count == 2 && key_values[0] == GY_KEY_ENTER && key_values[1] == GY_KEY_ENTER,
	      "main and keypad Enter both map to GY_KEY_ENTER");

	//右键短点击:只在抬起时派一次 Request。
	resetEvents();
	pushButton(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 10, 10);
	SDL_LCD_PumpEvents();
	CHECK(event_count == 0, "right down does not request context");
	pushButton(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 10, 10);
	SDL_LCD_PumpEvents();
	CHECK(event_count == 1 && events[0] == GY_EVENT_ContextRequested,
	      "right click requests context once on release");

	//4px 内抖动仍是短点击。
	resetEvents();
	pushButton(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 10, 10);
	pushMotion(SDL_BUTTON_RMASK, 14, 10);
	pushButton(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 14, 10);
	SDL_LCD_PumpEvents();
	CHECK(event_count == 1 && events[0] == GY_EVENT_ContextRequested,
	      "right motion within slop remains a click");

	//超过 4px:Request → Dragging → Released,不再补短点击。
	resetEvents();
	pushButton(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 10, 10);
	pushMotion(SDL_BUTTON_RMASK, 15, 10);
	pushMotion(SDL_BUTTON_RMASK, 20, 12);
	pushButton(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 20, 12);
	SDL_LCD_PumpEvents();
	CHECK(event_count == 4 &&
	      events[0] == GY_EVENT_ContextRequested &&
	      events[1] == GY_EVENT_ContextDragging &&
	      events[2] == GY_EVENT_ContextDragging &&
	      events[3] == GY_EVENT_ContextReleased,
	      "right drag dispatches captured lifecycle");

	//拖动中失焦:取消而非释放/点击。
	resetEvents();
	pushButton(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 10, 10);
	pushMotion(SDL_BUTTON_RMASK, 20, 10);
	SDL_Event lost;
	SDL_zero(lost);
	lost.type = SDL_WINDOWEVENT;
	lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
	SDL_PushEvent(&lost);
	SDL_LCD_PumpEvents();
	CHECK(event_count == 3 && events[2] == GY_EVENT_ContextCancelled,
	      "focus loss cancels right drag");

	//长按激活后 motion/up 接入相同 Dragging/Released 生命周期。
	resetEvents();
	pushFinger(SDL_FINGERDOWN, 0.25f, 0.25f);
	SDL_LCD_PumpEvents();
	SDL_Delay(620);
	SDL_LCD_PumpEvents();
	pushFinger(SDL_FINGERMOTION, 0.27f, 0.25f);
	pushFinger(SDL_FINGERUP, 0.27f, 0.25f);
	SDL_LCD_PumpEvents();
	CHECK(event_count == 3 &&
	      events[0] == GY_EVENT_ContextRequested &&
	      events[1] == GY_EVENT_ContextDragging &&
	      events[2] == GY_EVENT_ContextReleased,
	      "long press continues as context drag and release");

	//标题栏关闭请求可由应用取消或允许。
	int allow_close = 0;
	close_calls = 0;
	SDL_LCD_SetCloseRequestCb(onCloseRequest, &allow_close);
	pushQuit();
	CHECK(SDL_LCD_PumpEvents() == 1 && close_calls == 1,
	      "close callback can cancel window close");
	allow_close = 1;
	pushQuit();
	CHECK(SDL_LCD_PumpEvents() == 0 && close_calls == 2,
	      "close callback can allow window close");

	SDL_LCD_Destroy();
	CHECK(SDL_LCD_WindowId() == 0, "window ID cleared after destroy");
	CHECK(SDL_LCD_SetTitle("after destroy") == 0, "no title update after destroy");
	YMGUI_Inject_SetCtx(NULL);
	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_sdl_context: ALL PASS\n");
	else
		printf("test_sdl_context: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
