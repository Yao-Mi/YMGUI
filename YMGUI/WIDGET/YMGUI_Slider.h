#ifndef YMGUI_SLIDER_H
#define YMGUI_SLIDER_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 水平滑块:轨道 + 已填充段 + 拖柄。按下/拖动按指针 x 改值(min..max)
//===========================================================================

//值变回调
typedef void (*GYsld_changed_cb)(GYOBJ sld, int32 value);

GYOBJ YMGUI_Creat_Slider_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
void  YMGUI_Slider_SetRange(GYOBJ sld, int32 min, int32 max);//设范围
void  YMGUI_Slider_SetValue(GYOBJ sld, int32 value);         //设值(钳制+标脏)
int32 YMGUI_Slider_GetValue(GYOBJ sld);                      //读值
void  YMGUI_Slider_SetChanged(GYOBJ sld, GYsld_changed_cb cb);//值变回调

#endif // !YMGUI_SLIDER_H
