#ifndef YMGUI_DRAWLINE_H
#define YMGUI_DRAWLINE_H

#include "YMGUI_PubType.h"
#include "YMGUI_Surface.h"

//画线(Bresenham 整数)。水平/垂直特判。线宽 1px
void YMGUI_Draw_Line(GYSURFACE s, GYcoord x1, GYcoord y1, GYcoord x2, GYcoord y2, GYcolor color);

#endif // !YMGUI_DRAWLINE_H
