#ifndef YMGUI_BAR_H
#define YMGUI_BAR_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 进度条:背景槽 + 按值填充前景。只显示不交互(无 event_cb)
//===========================================================================

GYOBJ YMGUI_Creat_Bar_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
void  YMGUI_Bar_SetRange(GYOBJ bar, int32 min, int32 max);
void  YMGUI_Bar_SetValue(GYOBJ bar, int32 value);//设值(钳制+标脏)
int32 YMGUI_Bar_GetValue(GYOBJ bar);
void  YMGUI_Bar_SetColors(GYOBJ bar, GYcolor bg, GYcolor fg);//槽/前景色

#endif // !YMGUI_BAR_H
