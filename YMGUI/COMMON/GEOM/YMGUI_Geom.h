#ifndef YMGUI_GEOM_H
#define YMGUI_GEOM_H

#include "YMGUI_PubType.h"

//平台无关纯几何运算(不碰 GYsurface,可脱离 GUI 复用)
//两矩形求交:结果写入 res。返回 1=有交集,0=无交集(res 内容未定义)
int GY_Rect_Intersect(GYRECT res, const GYrect* a, const GYrect* b);
//点是否在矩形内
int GY_Rect_Contains(const GYrect* r, GYcoord x, GYcoord y);

#endif // !YMGUI_GEOM_H
