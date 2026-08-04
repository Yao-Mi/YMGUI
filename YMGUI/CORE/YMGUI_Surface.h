#ifndef YMGUI_SURFACE_H
#define YMGUI_SURFACE_H

#include "YMGUI_PubType.h"

//===========================================================================
// 表面:光栅化器的绘制目标。不是"整屏",而是"当前 band 在屏幕坐标系里的窗口"
//   所有绘制函数收屏幕坐标,内部 clip 到 clip 再平移到 buf_area 原点写入
//   → 图元完全不知道有没有 OS、是 SDL 还是真 LCD(可移植性锚点)
//===========================================================================
typedef struct
{
	GYpx*   buf;      //指向 disp 的 draw buffer
	GYrect  buf_area; //这块 buffer 当前映射到屏幕的哪个矩形
	GYrect  clip;     //当前裁剪区(屏幕坐标),控件绘制时收窄
	GYcoord stride;   //一行多少像素(通常 = buf_area.w)
}GYsurface;
typedef GYsurface* GYSURFACE;

#endif // !YMGUI_SURFACE_H
