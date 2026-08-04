#ifndef YMGUI_CHECKBOX_H
#define YMGUI_CHECKBOX_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 复选框:方框 + 勾选态 + 右侧标签文字。点击切换 checked
//===========================================================================
#ifndef GY_CB_TEXT_MAX
#define GY_CB_TEXT_MAX 32
#endif

//值变回调(checked=0/1)
typedef void (*GYcb_changed_cb)(GYOBJ cb, uint8 checked);

GYOBJ YMGUI_Creat_Checkbox_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
void  YMGUI_Checkbox_SetText(GYOBJ cb, const char* text);   //设标签文字
void  YMGUI_Checkbox_SetChecked(GYOBJ cb, uint8 checked);   //设勾选态(标脏)
uint8 YMGUI_Checkbox_GetChecked(GYOBJ cb);                  //读勾选态
void  YMGUI_Checkbox_SetChanged(GYOBJ cb, GYcb_changed_cb cb_fn);//值变回调

#endif // !YMGUI_CHECKBOX_H
