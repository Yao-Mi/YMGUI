#ifndef YMGUI_TRIG_H
#define YMGUI_TRIG_H

#include "YMGUI_PubType.h"

//===========================================================================
// 定点三角:sin/cos 表,Q15(值 = 实际 * 32768),整数查表,无 FPU/libm
//   角度单位:度(0..359),超范围自动取模
//===========================================================================
#define GY_TRIG_SHIFT 15   //Q15
#define GY_TRIG_ONE   32768

extern const int16 GY_SinTab[360];

//sin/cos 定点值(Q15)。deg 任意整数,内部取模
int16 GY_Sin(int32 deg);
int16 GY_Cos(int32 deg);

//细角度 sin/cos:角度以 1/64 度为单位(deg64 = 度*64),线性插值查表,供画弧细分用
int32 GY_Sin64(int32 deg64);
int32 GY_Cos64(int32 deg64);

//整数平方根:返回 floor(sqrt(v))。无 FPU(逐位法),供圆/弧抗锯齿算到理想半径的距离用
uint16 GY_Isqrt(uint32 v);

#endif // !YMGUI_TRIG_H
