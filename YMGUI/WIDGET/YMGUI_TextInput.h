#ifndef YMGUI_TEXTINPUT_H
#define YMGUI_TEXTINPUT_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 文本输入框:文本缓冲 + 光标。聚焦时绘光标,按键插入/退格/移光标
//   聚焦靠点击(GY_STATE_Focusable),键盘走 GY_EVENT_Key + ctx->last_key
//===========================================================================
#ifndef GY_TI_TEXT_MAX
#define GY_TI_TEXT_MAX 64
#endif

//文本变更回调(仅内容变化时触发,光标移动不触发);text 为内部缓冲当前值
typedef void (*GYti_changed_cb)(GYOBJ ti, const char* text);

GYOBJ YMGUI_Creat_TextInput_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
void  YMGUI_TextInput_SetText(GYOBJ ti, const char* text);//设文本(拷贝,光标移末尾)
const char* YMGUI_TextInput_GetText(GYOBJ ti);            //读文本
void  YMGUI_TextInput_SetChanged(GYOBJ ti, GYti_changed_cb cb);//文本变更回调

#endif // !YMGUI_TEXTINPUT_H
