#ifndef YMGUI_ARCWIDGET_H
#define YMGUI_ARCWIDGET_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 环形进度控件:背景弧(整段) + 前景弧(按值占比)。多层圆环描粗
//   角度:0=右,90=下(与图元一致),start..end 顺时针
//===========================================================================

GYOBJ YMGUI_Creat_Arc_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
void  YMGUI_Arc_SetRange(GYOBJ arc, int32 min, int32 max);
void  YMGUI_Arc_SetValue(GYOBJ arc, int32 value);          //设值(钳制+标脏)
int32 YMGUI_Arc_GetValue(GYOBJ arc);
void  YMGUI_Arc_SetAngles(GYOBJ arc, int32 start_deg, int32 end_deg);//弧跨度
void  YMGUI_Arc_SetWidth(GYOBJ arc, GYcoord thickness);    //环粗(px)
void  YMGUI_Arc_SetColors(GYOBJ arc, GYcolor bg, GYcolor fg);

#endif // !YMGUI_ARCWIDGET_H
