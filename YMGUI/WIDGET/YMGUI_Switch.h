#ifndef YMGUI_SWITCH_H
#define YMGUI_SWITCH_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 开关:轨道 + 滑钮。点击在 on/off 间切换,钮左右移动
//===========================================================================

//值变回调(on=0/1)
typedef void (*GYsw_changed_cb)(GYOBJ sw, uint8 on);

GYOBJ YMGUI_Creat_Switch_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
void  YMGUI_Switch_SetOn(GYOBJ sw, uint8 on);      //设开关态(标脏)
uint8 YMGUI_Switch_GetOn(GYOBJ sw);                //读开关态
void  YMGUI_Switch_SetChanged(GYOBJ sw, GYsw_changed_cb cb);//值变回调

#endif // !YMGUI_SWITCH_H
