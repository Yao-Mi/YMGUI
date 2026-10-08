#ifndef YMGUI_DRAWFILL_H
#define YMGUI_DRAWFILL_H

#include "YMGUI_PubDefine.h"
#include "YMGUI_PubType.h"
#include "YMGUI_Surface.h"

void YMGUI_Draw_Fill(GYSURFACE s, const GYrect* area, GYcolor color, GYopa opa);//在表面填充矩形区域(带裁剪与混合)
extern volatile uint8 YMGUI_BlendOptimizeEnabled;

#endif // !YMGUI_DRAWFILL_H
