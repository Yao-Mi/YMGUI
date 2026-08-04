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

//接收注入事件的上下文(GUI 层注册)
static GYCTX s_inject_ctx = NULL;

/**
  * @brief 注册接收注入事件的上下文
  */
void YMGUI_Inject_SetCtx(void* ctx)
{
	s_inject_ctx = (GYCTX)ctx;
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
	//只在按下时派发(抬起不产生字符输入)
	if (pressed && s_inject_ctx != NULL)
		YMGUI_Event_Key(s_inject_ctx, key);
}

/**
  * @brief 双击注入入口:转调事件分发(派 GY_EVENT_DoubleClicked 给命中对象)
  */
void YMGUI_Inject_DoubleClick(GYcoord x, GYcoord y)
{
	if (s_inject_ctx != NULL)
		YMGUI_Event_DoubleClick(s_inject_ctx, x, y);
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
