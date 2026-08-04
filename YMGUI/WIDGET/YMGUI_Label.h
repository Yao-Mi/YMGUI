#ifndef YMGUI_LABEL_H
#define YMGUI_LABEL_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 文本标签控件:GYobj + 文本串 + 前景/背景色。draw_cb 调 DrawText
//   文本存自有缓冲(SetText 时拷贝),最长 GY_LABEL_TEXT_MAX
//===========================================================================
#ifndef GY_LABEL_TEXT_MAX
#define GY_LABEL_TEXT_MAX 64
#endif

//创建标签,挂到 parent
GYOBJ YMGUI_Creat_Label_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
//设置文本(拷贝,自动标脏)
void  YMGUI_Label_SetText(GYOBJ label, const char* text);
//设置文字颜色 / 是否画背景
void  YMGUI_Label_SetTextColor(GYOBJ label, GYcolor color);
void  YMGUI_Label_SetBgEnable(GYOBJ label, uint8 enable);

#endif // !YMGUI_LABEL_H
