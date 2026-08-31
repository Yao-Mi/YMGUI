#include "YMGUI_Hal.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Event.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Hal.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: HAL 接口的库侧实现。flush 就绪回调 + 输入注入入口
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.当前地基阶段:FlushReady 为空(同步 flush 无需等待);Inject_* 先打印,GUI 层接入后改为事件分发
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

/**
  * @brief flush 完成通知:清 flush_busy,库据此可复用该 buffer。
  *        异步 DMA 在传输完成中断里调;同步 flush_cb 在函数体末尾调。单缓冲(buf2==NULL)时库不检查此标志,调它无副作用。
  */
void YMGUI_Disp_FlushReady(GYDISP d)
{
	if (d != NULL)
		d->flush_busy = 0;
}

#define GY_FRAME_DONE_SLOT_MAX 4
typedef struct
{
	GYDISP display;
	GYframe_done_cb callback;
} gy_frame_done_slot;

static gy_frame_done_slot s_frame_done_slots[GY_FRAME_DONE_SLOT_MAX];

void YMGUI_Disp_SetFrameDoneCb(GYDISP d, GYframe_done_cb cb)
{
	if (d == NULL)
		return;
	for (uint8 i = 0; i < GY_FRAME_DONE_SLOT_MAX; i++)
	{
		if (s_frame_done_slots[i].display == d)
		{
			s_frame_done_slots[i].callback = cb;
			if (cb == NULL)
				s_frame_done_slots[i].display = NULL;
			return;
		}
	}
	if (cb == NULL)
		return;
	for (uint8 i = 0; i < GY_FRAME_DONE_SLOT_MAX; i++)
	{
		if (s_frame_done_slots[i].display == NULL)
		{
			s_frame_done_slots[i].display = d;
			s_frame_done_slots[i].callback = cb;
			return;
		}
	}
}

void YMGUI_Disp_FrameDone(GYDISP d)
{
	if (d == NULL)
		return;
	for (uint8 i = 0; i < GY_FRAME_DONE_SLOT_MAX; i++)
	{
		if (s_frame_done_slots[i].display == d)
		{
			if (s_frame_done_slots[i].callback != NULL)
				s_frame_done_slots[i].callback(d);
			return;
		}
	}
}

//接收注入事件的上下文(GUI 层注册)
static GYCTX s_inject_ctx = NULL;
static GYkey_filter_cb s_key_filter = NULL;
static void* s_key_filter_user = NULL;

/**
  * @brief 注册接收注入事件的上下文
  */
void YMGUI_Inject_SetCtx(void* ctx)
{
	s_inject_ctx = (GYCTX)ctx;
}

void YMGUI_Inject_SetKeyFilter(GYkey_filter_cb cb, void* user_data)
{
	s_key_filter = cb;
	s_key_filter_user = user_data;
}

/**
  * @brief 指针注入入口:转调事件分发
  */
void YMGUI_Inject_Pointer(GYcoord x, GYcoord y, uint8 pressed)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_Pointer(s_inject_ctx, x, y, pressed);
}

/**
  * @brief 按键注入入口:转调事件分发给焦点对象
  */
void YMGUI_Inject_Key(uint32 key, uint8 pressed)
{
	if (s_key_filter != NULL && s_key_filter(key, pressed != 0, s_key_filter_user))
		return;
	//抬起只供过滤器维护状态，不产生字符输入
	if (pressed && s_inject_ctx != NULL)
		YMGUI_Event_Key(s_inject_ctx, key);
}

void YMGUI_Inject_Wheel(GYcoord x, GYcoord y, int32 delta_x, int32 delta_y)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_Wheel(s_inject_ctx, x, y, delta_x, delta_y);
}

void YMGUI_Inject_Tick(uint32 elapsed_ms)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_Tick(s_inject_ctx, elapsed_ms);
}

/**
  * @brief 双击注入入口:转调事件分发(派 GY_EVENT_DoubleClicked 给命中对象)
  */
void YMGUI_Inject_DoubleClick(GYcoord x, GYcoord y)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_DoubleClick(s_inject_ctx, x, y);
}

/**
  * @brief 取消当前指针捕获,不产生普通点击
  */
void YMGUI_Inject_PointerCancel(void)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_PointerCancel(s_inject_ctx);
}

/**
  * @brief 上下文请求注入入口:右键/长按等平台输入统一走此语义
  */
void YMGUI_Inject_ContextRequest(GYcoord x, GYcoord y)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_ContextRequest(s_inject_ctx, x, y);
}

void YMGUI_Inject_ContextBegin(GYcoord x, GYcoord y)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_ContextBegin(s_inject_ctx, x, y);
}

void YMGUI_Inject_ContextMove(GYcoord x, GYcoord y)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_ContextMove(s_inject_ctx, x, y);
}

void YMGUI_Inject_ContextEnd(GYcoord x, GYcoord y)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_ContextEnd(s_inject_ctx, x, y);
}

void YMGUI_Inject_ContextCancel(void)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_ContextCancel(s_inject_ctx);
}

//===========================================================================
// 剪贴板:默认库内静态缓冲(裸机);SDL_LCD 可注册系统剪贴板后端覆盖之
//===========================================================================
#ifndef GY_CLIP_MAX
#define GY_CLIP_MAX 32768  //库内剪贴板缓冲上限(32KB;裸机可用编译宏改小省 RAM)
#endif
static char          s_clip_buf[GY_CLIP_MAX] = { 0 };
static GYclip_set_cb s_clip_set = NULL;
static GYclip_get_cb s_clip_get = NULL;

/**
  * @brief 注册系统剪贴板后端(NULL/NULL 恢复库内静态缓冲)
  */
void YMGUI_Clipboard_SetBackend(GYclip_set_cb set_cb, GYclip_get_cb get_cb)
{
	s_clip_set = set_cb;
	s_clip_get = get_cb;
}

/**
  * @brief 写剪贴板:有后端走后端,否则拷进库内静态缓冲(截断到上限)
  */
void YMGUI_Clipboard_SetText(const char* text)
{
	if (text == NULL)
		text = "";
	if (s_clip_set != NULL)
	{
		s_clip_set(text);
		return;
	}
	uint32 i = 0;
	while (text[i] != '\0' && i < GY_CLIP_MAX - 1)
	{
		s_clip_buf[i] = text[i];
		i++;
	}
	s_clip_buf[i] = '\0';
}

/**
  * @brief 读剪贴板:有后端走后端(NULL 归一为 ""),否则返回库内缓冲
  */
const char* YMGUI_Clipboard_GetText(void)
{
	if (s_clip_get != NULL)
	{
		const char* t = s_clip_get();
		return (t != NULL) ? t : "";
	}
	return s_clip_buf;
}
