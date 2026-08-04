#ifndef YMGUI_SPINNER_H
#define YMGUI_SPINNER_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 加载转圈:一段固定跨度的圆弧,每次 Tick 旋转一步并标脏
//   无内部定时器(裸机友好):用户主循环按需调 Tick
//===========================================================================

GYOBJ YMGUI_Creat_Spinner_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
void  YMGUI_Spinner_Tick(GYOBJ sp);                //旋转一步(标脏)
void  YMGUI_Spinner_SetSpan(GYOBJ sp, int32 span_deg, int32 step_deg);//弧跨度/每步角度
void  YMGUI_Spinner_SetColor(GYOBJ sp, GYcolor color);
void  YMGUI_Spinner_SetWidth(GYOBJ sp, GYcoord thickness);

#endif // !YMGUI_SPINNER_H
