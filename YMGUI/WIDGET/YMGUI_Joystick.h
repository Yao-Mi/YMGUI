#ifndef YMGUI_JOYSTICK_H
#define YMGUI_JOYSTICK_H

#include "YMGUI_PubDefine.h"
#include "YMGUI_Obj.h"

#if YMGUI_JOYSTICK

#define GY_JOYSTICK_VALUE_MAX 100

typedef void (*GYjoystick_changed_cb)(GYOBJ joystick, int16 x, int16 y);
//操控结束回调:x/y 为松手前的最终值；回调执行时自动回中已按配置完成。
typedef void (*GYjoystick_released_cb)(GYOBJ joystick, int16 x, int16 y);
typedef void (*GYjoystick_draw_cb)(GYOBJ joystick, GYSURFACE surface, const GYrect* abs);

//创建二维虚拟摇杆。输出为圆形范围内的 -100..100，Y 正方向向上。
GYOBJ YMGUI_Creat_Joystick_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);

//程序设值；超出单位圆的向量会按方向钳到边缘。不触发 changed 回调。
void YMGUI_Joystick_SetValue(GYOBJ joystick, int16 x, int16 y);
void YMGUI_Joystick_GetValue(GYOBJ joystick, int16* x, int16* y);

//死区为 0..100 的归一化半径，默认 8。
void YMGUI_Joystick_SetDeadzone(GYOBJ joystick, uint16 deadzone);
uint16 YMGUI_Joystick_GetDeadzone(GYOBJ joystick);

//默认开启；关闭后松手保留最后位置和值。
void YMGUI_Joystick_SetAutoCenter(GYOBJ joystick, uint8 enabled);
uint8 YMGUI_Joystick_GetAutoCenter(GYOBJ joystick);
uint8 YMGUI_Joystick_IsActive(GYOBJ joystick);

void YMGUI_Joystick_SetChangedCb(GYOBJ joystick, GYjoystick_changed_cb cb);
void YMGUI_Joystick_SetReleasedCb(GYOBJ joystick, GYjoystick_released_cb cb);

//设置自定义绘制回调，完全替换基础外观；NULL 恢复默认绘制。
//位置、按下态等通过上面的 GetValue/IsActive 读取。
void YMGUI_Joystick_SetDrawCb(GYOBJ joystick, GYjoystick_draw_cb cb);

#endif // YMGUI_JOYSTICK

#endif // !YMGUI_JOYSTICK_H
