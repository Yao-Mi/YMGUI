#ifndef YMGUI_DRAWARC_H
#define YMGUI_DRAWARC_H

#include "YMGUI_PubType.h"
#include "YMGUI_Surface.h"

//画圆环(中点画圆,1px 描边)
void YMGUI_Draw_Circle(GYSURFACE s, GYcoord cx, GYcoord cy, GYcoord r, GYcolor color);
//画实心圆盘
void YMGUI_Draw_CircleFill(GYSURFACE s, GYcoord cx, GYcoord cy, GYcoord r, GYcolor color);
//画圆弧(角度 start..end 度,顺时针,0=右,90=下)。定点三角表步进
void YMGUI_Draw_Arc(GYSURFACE s, GYcoord cx, GYcoord cy, GYcoord r, int32 start_deg, int32 end_deg, GYcolor color);
//画粗弧环(半径 r_in..r_out 之间的扇环,无缝隙)。角度自适应细分 + 径向填充
void YMGUI_Draw_ArcThick(GYSURFACE s, GYcoord cx, GYcoord cy, GYcoord r_in, GYcoord r_out, int32 start_deg, int32 end_deg, GYcolor color);

#endif // !YMGUI_DRAWARC_H
