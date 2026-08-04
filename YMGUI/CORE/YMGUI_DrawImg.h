#ifndef YMGUI_DRAWIMG_H
#define YMGUI_DRAWIMG_H

#include "YMGUI_PubType.h"
#include "YMGUI_Surface.h"

//===========================================================================
// 图片描述符:native 像素数据(GYpx,与 COLOR_DEPTH 一致)+ 可选 colorkey 透明
//===========================================================================
typedef struct
{
	const GYpx* data;     //像素数据(行优先,stride = w)
	GYcoord     w, h;     //尺寸
	uint8       use_key;  //是否启用 colorkey 透明
	GYpx        key;      //透明色(等于此值的像素不画)
}GYimg;
typedef const GYimg* GYIMG;

//把图片 blit 到 surface(左上角在屏幕 x,y),裁剪+band偏移
void YMGUI_Draw_Img(GYSURFACE s, GYIMG img, GYcoord x, GYcoord y);

//把图片缩放 blit 到目标屏幕矩形 dst(定点最近邻采样)。裁剪+band偏移+colorkey 保留。
//  dst.w/dst.h 决定缩放比例(可放大可缩小);dst 为空(w/h<=0)或 img 无数据直接返回。
//  纯机械缩放 —— 不含"等比适配/居中/黑边"策略(那属控件层,见 YMGUI_Image 的 FIT/FILL)。
//  通用途径:缩略图、DPI 图标、摄像头预览、视频帧显示都用它。
void YMGUI_Draw_ImgScaled(GYSURFACE s, GYIMG img, GYrect dst);

#endif // !YMGUI_DRAWIMG_H
