#ifndef YMGUI_METER_H
#define YMGUI_METER_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 仪表盘:外圈弧 + 刻度线 + 指针(DrawLine 按值角度)。值/范围/角度可设
//===========================================================================

GYOBJ YMGUI_Creat_Meter_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
void  YMGUI_Meter_SetRange(GYOBJ m, int32 min, int32 max);
void  YMGUI_Meter_SetValue(GYOBJ m, int32 value);         //设值(钳制+标脏)
int32 YMGUI_Meter_GetValue(GYOBJ m);
void  YMGUI_Meter_SetAngles(GYOBJ m, int32 start_deg, int32 end_deg);//扫描角度范围
void  YMGUI_Meter_SetTicks(GYOBJ m, uint8 count);         //刻度数
void  YMGUI_Meter_SetShowLabels(GYOBJ m, uint8 on);       //大刻度旁标数值(默认关;开则每个刻度画其对应值,便于肉眼预估)
void  YMGUI_Meter_SetLabelColor(GYOBJ m, GYcolor color);  //刻度值文字色

#endif // !YMGUI_METER_H
