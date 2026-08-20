#include "SDL_LCD.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Event.h"
#include <SDL2/SDL.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    SDL_LCD.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: SDL 假 LCD:实现 HAL flush_cb,把 GYpx framebuffer 上传为 SDL 纹理显示
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.这是 HAL 的一个具体实现(port)。真实 LCD 只需换掉 sdlFlushCb 的函数体
  * 2.GYpx=RGB565 时用 SDL_PIXELFORMAT_RGB565,像素布局与硬件一致
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

static SDL_Window*   s_win = NULL;
static SDL_Renderer* s_ren = NULL;
static SDL_Texture*  s_tex = NULL;
static int           s_w = 0, s_h = 0;
static int           s_scale = 1;
static int           s_touch_active = 0;
static SDL_FingerID  s_touch_finger = 0;
static GYcoord       s_touch_start_x = 0, s_touch_start_y = 0;
static GYcoord       s_touch_x = 0, s_touch_y = 0;
static Uint32        s_touch_start_tick = 0;
static int           s_long_press_eligible = 0;
static int           s_context_fired = 0;
static int           s_right_state = 0;
static GYcoord       s_right_start_x = 0, s_right_start_y = 0;
//全帧累积 buffer:flush 只给分条(band)像素,截图要整屏,故每条 band 落进这里。
//退出时若设了环境变量 YMGUI_SHOT=<path.bmp> 就存图(所有 demo 零改动即可截屏)。
static GYpx*         s_frame = NULL;

#define SDL_LCD_LONG_PRESS_MS   600u
#define SDL_LCD_LONG_PRESS_SLOP 10
#define SDL_LCD_RIGHT_DRAG_SLOP 4
#define SDL_LCD_RIGHT_IDLE      0
#define SDL_LCD_RIGHT_CANDIDATE 1
#define SDL_LCD_RIGHT_DRAGGING  2

static void sdlTouchPoint(float nx, float ny, GYcoord* x, GYcoord* y)
{
	int px = (int)(nx * s_w);
	int py = (int)(ny * s_h);
	if (px < 0) px = 0;
	if (py < 0) py = 0;
	if (px >= s_w) px = s_w - 1;
	if (py >= s_h) py = s_h - 1;
	*x = (GYcoord)px;
	*y = (GYcoord)py;
}

static void sdlTouchReset(void)
{
	s_touch_active = 0;
	s_touch_finger = 0;
	s_touch_start_x = 0;
	s_touch_start_y = 0;
	s_touch_x = 0;
	s_touch_y = 0;
	s_touch_start_tick = 0;
	s_long_press_eligible = 0;
	s_context_fired = 0;
}

static void sdlTouchCancel(void)
{
	if (s_touch_active)
	{
		if (s_context_fired)
			YMGUI_Inject_ContextCancel();
		else
			YMGUI_Inject_PointerCancel();
	}
	sdlTouchReset();
}

static void sdlRightReset(void)
{
	if (s_right_state != SDL_LCD_RIGHT_IDLE)
		SDL_CaptureMouse(SDL_FALSE);
	s_right_state = SDL_LCD_RIGHT_IDLE;
	s_right_start_x = 0;
	s_right_start_y = 0;
}

static void sdlRightCancel(void)
{
	if (s_right_state == SDL_LCD_RIGHT_DRAGGING)
		YMGUI_Inject_ContextCancel();
	sdlRightReset();
}

static void sdlCheckLongPress(Uint32 now)
{
	if (!s_touch_active || !s_long_press_eligible || s_context_fired)
		return;
	if ((Uint32)(now - s_touch_start_tick) < SDL_LCD_LONG_PRESS_MS)
		return;
	s_context_fired = 1;
	s_long_press_eligible = 0;
	YMGUI_Inject_PointerCancel();
	YMGUI_Inject_ContextBegin(s_touch_start_x, s_touch_start_y);
	if (s_touch_x != s_touch_start_x || s_touch_y != s_touch_start_y)
		YMGUI_Inject_ContextMove(s_touch_x, s_touch_y);
}

#if YMGUI_COLOR_DEPTH == 16
#define SDL_LCD_PIXFMT SDL_PIXELFORMAT_RGB565
#else
#define SDL_LCD_PIXFMT SDL_PIXELFORMAT_RGB332
#endif

//剪贴板后端(把 YMGUI 剪贴板缝接到系统剪贴板)。SDL_GetClipboardText 返回需 SDL_free 的堆串,
//而 GYclip_get_cb 约定返回借用指针(调用方只读不释放),故用静态缓冲持有一份再返回。
#define SDL_LCD_CLIP_MAX 2048
static char s_clip_hold[SDL_LCD_CLIP_MAX];
static void sdlClipSet(const char* text)
{
	SDL_SetClipboardText(text != NULL ? text : "");
}
static const char* sdlClipGet(void)
{
	char* t = SDL_GetClipboardText();//堆分配,须 SDL_free
	s_clip_hold[0] = '\0';
	if (t != NULL)
	{
		int i = 0;
		while (t[i] != '\0' && i < SDL_LCD_CLIP_MAX - 1)
		{
			s_clip_hold[i] = t[i];
			i++;
		}
		s_clip_hold[i] = '\0';
		SDL_free(t);
	}
	return s_clip_hold;
}

/**
  * @brief HAL flush 回调:把 buf 里 area 大小的一块连续像素更新到纹理并呈现
  *        真实硬件在此改成 SPI/并口 DMA 传输
  */
static void sdlFlushCb(GYdisp* d, const GYrect* area, const GYpx* buf)
{
	SDL_Rect r;
	r.x = area->x;
	r.y = area->y;
	r.w = area->w;
	r.h = area->h;
	//buf 是这条 band 的连续像素,pitch = 一行字节数 = w * sizeof(GYpx)
	SDL_UpdateTexture(s_tex, &r, buf, area->w * (int)sizeof(GYpx));

	//把这条 band 落进全帧累积 buffer(供退出时截图);逐行拷贝对齐到全屏 pitch
	if (s_frame != NULL)
	{
		for (int row = 0; row < area->h; row++)
		{
			GYpx*       dst = s_frame + (size_t)(area->y + row) * s_w + area->x;
			const GYpx* src = buf + (size_t)row * area->w;
			SDL_memcpy(dst, src, (size_t)area->w * sizeof(GYpx));
		}
	}

	//整帧最后一条 band 刷完再 present(这里简单起见每条都 present)
	SDL_RenderClear(s_ren);
	SDL_RenderCopy(s_ren, s_tex, NULL, NULL);
	SDL_RenderPresent(s_ren);

	//同步实现:传完即可复用 buffer
	YMGUI_Disp_FlushReady(d);
}

/**
  * @brief 初始化 SDL 假 LCD,挂好 disp 的 flush_cb
  */
int SDL_LCD_Init(GYDISP disp, int scale)
{
	if (scale < 1)
		scale = 1;
	s_scale = scale;
	s_w = disp->hor_res;
	s_h = disp->ver_res;
	sdlTouchReset();
	sdlRightReset();

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		return -1;
	s_win = SDL_CreateWindow("YMGUI SDL_LCD",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		s_w * scale, s_h * scale, SDL_WINDOW_SHOWN);
	if (s_win == NULL)
		goto fail;
	s_ren = SDL_CreateRenderer(s_win, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (s_ren == NULL)
		s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_SOFTWARE);
	if (s_ren == NULL)
		goto fail;
	s_tex = SDL_CreateTexture(s_ren, SDL_LCD_PIXFMT,
		SDL_TEXTUREACCESS_STREAMING, s_w, s_h);
	if (s_tex == NULL)
		goto fail;

	//截图用全帧 buffer(仅当设了 YMGUI_SHOT 才分配,不截图零开销)
	if (SDL_getenv("YMGUI_SHOT") != NULL)
	{
		s_frame = (GYpx*)SDL_calloc((size_t)s_w * s_h, sizeof(GYpx));
		if (s_frame == NULL)
			goto fail;
	}

	//挂上 flush 回调 —— 这是库与硬件之间唯一的连接点
	disp->flush_cb = sdlFlushCb;
	//把剪贴板缝接到系统剪贴板(裸机不注册则用库内静态缓冲)
	YMGUI_Clipboard_SetBackend(sdlClipSet, sdlClipGet);
	SDL_StartTextInput();//启用文本输入事件(SDL_TEXTINPUT)
	return 0;

fail:
	SDL_LCD_Destroy();
	return -1;
}

/**
  * @brief 销毁
  */
void SDL_LCD_Destroy(void)
{
	//若设了 YMGUI_SHOT,把最后一帧存成 BMP(RGB565 → SDL surface → SaveBMP)
	if (s_frame != NULL)
	{
		const char* path = SDL_getenv("YMGUI_SHOT");
		if (path != NULL && path[0] != '\0')
		{
			SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
				s_frame, s_w, s_h, YMGUI_COLOR_DEPTH,
				s_w * (int)sizeof(GYpx), SDL_LCD_PIXFMT);
			if (surf != NULL)
			{
				if (SDL_SaveBMP(surf, path) != 0)
					SDL_Log("YMGUI_SHOT: SaveBMP 失败: %s", SDL_GetError());
				SDL_FreeSurface(surf);
			}
		}
		SDL_free(s_frame);
		s_frame = NULL;
	}
	if (s_tex)
	{
		SDL_DestroyTexture(s_tex);
		s_tex = NULL;
	}
	if (s_ren)
	{
		SDL_DestroyRenderer(s_ren);
		s_ren = NULL;
	}
	if (s_win)
	{
		SDL_DestroyWindow(s_win);
		s_win = NULL;
	}
	sdlTouchReset();
	sdlRightReset();
	s_w = 0;
	s_h = 0;
	SDL_Quit();
}

/**
  * @brief 抽干事件队列。真实工程里会把 SDL_Event 翻译成 YMGUI_Inject_*;
  *        当前地基阶段只处理退出。返回 0 表示收到退出请求
  */
int SDL_LCD_PumpEvents(void)
{
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_QUIT)
		{
			sdlTouchCancel();
			sdlRightCancel();
			YMGUI_Inject_PointerCancel();
			return 0;
		}
		if (e.type == SDL_APP_WILLENTERBACKGROUND ||
		    e.type == SDL_APP_DIDENTERBACKGROUND ||
		    e.type == SDL_APP_TERMINATING ||
		    (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST))
		{
			sdlTouchCancel();
			sdlRightCancel();
			YMGUI_Inject_PointerCancel();
			continue;
		}

		//事件积压时先按事件时间检查,避免到期的 FINGERUP 被误派为普通点击。
		//外部 SDL_PushEvent 可能留下 timestamp=0,这种事件交给队列末尾的当前 tick 检查。
		if (e.common.timestamp != 0)
			sdlCheckLongPress(e.common.timestamp);

		// SDL touch coordinates are normalized to the output. Track one finger;
		// this matches YMGUI's single captured-pointer event model.
		if (e.type == SDL_FINGERDOWN && !s_touch_active)
		{
			sdlTouchPoint(e.tfinger.x, e.tfinger.y, &s_touch_x, &s_touch_y);
			s_touch_active = 1;
			s_touch_finger = e.tfinger.fingerId;
			s_touch_start_x = s_touch_x;
			s_touch_start_y = s_touch_y;
			s_touch_start_tick = e.tfinger.timestamp;
			s_long_press_eligible = 1;
			s_context_fired = 0;
			YMGUI_Inject_Pointer(s_touch_x, s_touch_y, 1);
		}
		else if (e.type == SDL_FINGERMOTION && s_touch_active &&
		         e.tfinger.fingerId == s_touch_finger)
		{
			sdlTouchPoint(e.tfinger.x, e.tfinger.y, &s_touch_x, &s_touch_y);
			if (s_long_press_eligible)
			{
				int32 dx = (int32)s_touch_x - s_touch_start_x;
				int32 dy = (int32)s_touch_y - s_touch_start_y;
				if (dx * dx + dy * dy > SDL_LCD_LONG_PRESS_SLOP * SDL_LCD_LONG_PRESS_SLOP)
					s_long_press_eligible = 0;
			}
			if (s_context_fired)
				YMGUI_Inject_ContextMove(s_touch_x, s_touch_y);
			else
				YMGUI_Inject_Pointer(s_touch_x, s_touch_y, 1);
		}
		else if (e.type == SDL_FINGERUP && s_touch_active &&
		         e.tfinger.fingerId == s_touch_finger)
		{
			sdlTouchPoint(e.tfinger.x, e.tfinger.y, &s_touch_x, &s_touch_y);
			if (s_context_fired)
				YMGUI_Inject_ContextEnd(s_touch_x, s_touch_y);
			else
				YMGUI_Inject_Pointer(s_touch_x, s_touch_y, 0);
			sdlTouchReset();
		}
		//鼠标 → YMGUI_Inject_Pointer(窗口坐标除以放大倍数还原为屏幕坐标)
		else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.which != SDL_TOUCH_MOUSEID &&
		         e.button.button == SDL_BUTTON_LEFT)
			YMGUI_Inject_Pointer((GYcoord)(e.button.x / s_scale), (GYcoord)(e.button.y / s_scale), 1);
		else if (e.type == SDL_MOUSEBUTTONUP && e.button.which != SDL_TOUCH_MOUSEID &&
		         e.button.button == SDL_BUTTON_LEFT)
		{
			YMGUI_Inject_Pointer((GYcoord)(e.button.x / s_scale), (GYcoord)(e.button.y / s_scale), 0);
			//双击:抬起后(第一击已完成 press+release,已聚焦/定位光标)再补派双击 → 编辑器选词
			if (e.button.clicks == 2)
				YMGUI_Inject_DoubleClick((GYcoord)(e.button.x / s_scale), (GYcoord)(e.button.y / s_scale));
		}
		else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.which != SDL_TOUCH_MOUSEID &&
		         e.button.button == SDL_BUTTON_RIGHT)
		{
			if (s_right_state == SDL_LCD_RIGHT_IDLE)
			{
				s_right_state = SDL_LCD_RIGHT_CANDIDATE;
				s_right_start_x = (GYcoord)(e.button.x / s_scale);
				s_right_start_y = (GYcoord)(e.button.y / s_scale);
				SDL_CaptureMouse(SDL_TRUE);
			}
		}
		else if (e.type == SDL_MOUSEBUTTONUP && e.button.which != SDL_TOUCH_MOUSEID &&
		         e.button.button == SDL_BUTTON_RIGHT)
		{
			GYcoord x = (GYcoord)(e.button.x / s_scale);
			GYcoord y = (GYcoord)(e.button.y / s_scale);
			int state = s_right_state;
			sdlRightReset();
			if (state == SDL_LCD_RIGHT_CANDIDATE)
				YMGUI_Inject_ContextRequest(x, y);
			else if (state == SDL_LCD_RIGHT_DRAGGING)
				YMGUI_Inject_ContextEnd(x, y);
		}
		else if (e.type == SDL_MOUSEMOTION && e.motion.which != SDL_TOUCH_MOUSEID &&
		         (e.motion.state & SDL_BUTTON_RMASK) && s_right_state != SDL_LCD_RIGHT_IDLE)
		{
			GYcoord x = (GYcoord)(e.motion.x / s_scale);
			GYcoord y = (GYcoord)(e.motion.y / s_scale);
			if (s_right_state == SDL_LCD_RIGHT_CANDIDATE)
			{
				int32 dx = (int32)x - s_right_start_x;
				int32 dy = (int32)y - s_right_start_y;
				if (dx * dx + dy * dy > SDL_LCD_RIGHT_DRAG_SLOP * SDL_LCD_RIGHT_DRAG_SLOP)
				{
					s_right_state = SDL_LCD_RIGHT_DRAGGING;
					YMGUI_Inject_ContextBegin(s_right_start_x, s_right_start_y);
					YMGUI_Inject_ContextMove(x, y);
				}
			}
			else
				YMGUI_Inject_ContextMove(x, y);
		}
		else if (e.type == SDL_MOUSEMOTION && e.motion.which != SDL_TOUCH_MOUSEID &&
		         (e.motion.state & SDL_BUTTON_LMASK))
			YMGUI_Inject_Pointer((GYcoord)(e.motion.x / s_scale), (GYcoord)(e.motion.y / s_scale), 1);//按住拖动
		//文本输入(可打印字符,已处理布局/大小写)→ 按字符注入
		else if (e.type == SDL_TEXTINPUT)
		{
			for (const char* p = e.text.text; *p != '\0'; p++)
				YMGUI_Inject_Key((uint32)(uint8)*p, 1);
		}
		//控制键 → 映射到 GY_KEY_*。修饰键(Ctrl/Shift)在此合成为编辑器虚拟键,
		//库/控件只认虚拟键,无需 ctx 存修饰键位(见 YMGUI_Event.h 0x1100 段说明)。
		else if (e.type == SDL_KEYDOWN)
		{
			SDL_Keycode  k    = e.key.keysym.sym;
			SDL_Keymod   mod  = SDL_GetModState();
			int          ctrl = (mod & KMOD_CTRL)  != 0;
			int          shft = (mod & KMOD_SHIFT) != 0;

			if (k == SDLK_ESCAPE)
			{
				sdlTouchCancel();
				sdlRightCancel();
				YMGUI_Inject_PointerCancel();
				return 0;
			}
			else if (ctrl)
			{
				//Ctrl+组合键:剪贴板/全选/撤销/查找 + Ctrl+Home/End 文首尾
				switch (k)
				{
				case SDLK_c:    YMGUI_Inject_Key(GY_KEY_COPY, 1);     break;
				case SDLK_x:    YMGUI_Inject_Key(GY_KEY_CUT, 1);      break;
				case SDLK_v:    YMGUI_Inject_Key(GY_KEY_PASTE, 1);    break;
				case SDLK_z:    YMGUI_Inject_Key(GY_KEY_UNDO, 1);     break;
				case SDLK_a:    YMGUI_Inject_Key(GY_KEY_SEL_ALL, 1);  break;
				case SDLK_f:    YMGUI_Inject_Key(GY_KEY_FIND, 1);     break;
				case SDLK_HOME: YMGUI_Inject_Key(GY_KEY_DOC_HOME, 1); break;
				case SDLK_END:  YMGUI_Inject_Key(GY_KEY_DOC_END, 1);  break;
				default: break;
				}
			}
			else if (k == SDLK_BACKSPACE) YMGUI_Inject_Key(GY_KEY_BACKSPACE, 1);
			else if (k == SDLK_RETURN)    YMGUI_Inject_Key(GY_KEY_ENTER, 1);
			else if (k == SDLK_DELETE)    YMGUI_Inject_Key(GY_KEY_DEL, 1);
			else if (k == SDLK_LEFT)      YMGUI_Inject_Key(shft ? GY_KEY_SHIFT_LEFT  : GY_KEY_LEFT, 1);
			else if (k == SDLK_RIGHT)     YMGUI_Inject_Key(shft ? GY_KEY_SHIFT_RIGHT : GY_KEY_RIGHT, 1);
			else if (k == SDLK_UP)        YMGUI_Inject_Key(shft ? GY_KEY_SHIFT_UP    : GY_KEY_UP, 1);
			else if (k == SDLK_DOWN)      YMGUI_Inject_Key(shft ? GY_KEY_SHIFT_DOWN  : GY_KEY_DOWN, 1);
			else if (k == SDLK_HOME)      YMGUI_Inject_Key(shft ? GY_KEY_SHIFT_HOME  : GY_KEY_HOME, 1);
			else if (k == SDLK_END)       YMGUI_Inject_Key(shft ? GY_KEY_SHIFT_END   : GY_KEY_END, 1);
			else if (k == SDLK_TAB)       YMGUI_Inject_Key(GY_KEY_TAB, 1);
		}
	}
	//手指完全静止时不会再有 SDL 事件,每轮消息泵末尾主动检查超时。
	sdlCheckLongPress(SDL_GetTicks());
	return 1;
}

/**
  * @brief 延时
  */
void SDL_LCD_Delay(int ms)
{
	SDL_Delay(ms);
}
