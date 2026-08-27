#ifndef YMGUI_TEXTINPUT_H
#define YMGUI_TEXTINPUT_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 文本输入框:创建时按需分配文本缓冲 + 光标。聚焦时绘光标,按键插入/退格/移光标
//   聚焦靠点击(GY_STATE_Focusable),键盘走 GY_EVENT_Key + ctx->last_key
//===========================================================================

//文本变更回调(仅内容变化时触发,光标移动不触发);text 为内部缓冲当前值
typedef void (*GYti_changed_cb)(GYOBJ ti, const char* text);
//编辑态按 Enter 时触发；触发前已退出编辑态。设置后替代默认的 Enter 焦点轮转
typedef void (*GYti_submitted_cb)(GYOBJ ti, const char* text);

//capacity 为最大文本字节数(不含结尾 '\0');内部实际申请 capacity+1 字节
GYOBJ YMGUI_Creat_TextInput_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h, size_t capacity);
void  YMGUI_TextInput_SetText(GYOBJ ti, const char* text);//设文本(拷贝,光标移末尾)
const char* YMGUI_TextInput_GetText(GYOBJ ti);            //读文本
void  YMGUI_TextInput_SetChanged(GYOBJ ti, GYti_changed_cb cb);//文本变更回调
void  YMGUI_TextInput_SetSubmitted(GYOBJ ti, GYti_submitted_cb cb);//Enter 提交回调,NULL 恢复默认轮转

//选区与剪贴板(字节范围始终落在 UTF-8 码点边界)
uint8 YMGUI_TextInput_HasSelection(GYOBJ ti);
void  YMGUI_TextInput_GetSelection(GYOBJ ti, size_t* start, size_t* end);
void  YMGUI_TextInput_SelectAll(GYOBJ ti);
void  YMGUI_TextInput_ClearSelection(GYOBJ ti);
size_t YMGUI_TextInput_GetSelectionText(GYOBJ ti, char* out, size_t out_cap);
void  YMGUI_TextInput_Copy(GYOBJ ti);
void  YMGUI_TextInput_Cut(GYOBJ ti);
void  YMGUI_TextInput_Paste(GYOBJ ti);//过滤 CR/LF,按完整 UTF-8 码点钳到容量

//当前自动水平滚动量(像素);输入、移动光标、鼠标选择时自动维护
int32 YMGUI_TextInput_GetScrollX(GYOBJ ti);

#endif // !YMGUI_TEXTINPUT_H
