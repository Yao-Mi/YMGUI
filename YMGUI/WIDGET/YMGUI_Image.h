#ifndef YMGUI_IMAGE_H
#define YMGUI_IMAGE_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"
#include "YMGUI_DrawImg.h"

//===========================================================================
// 图片控件:持有 GYimg 指针,在控件区内按缩放模式显示。不复制像素(用户保证图存活)
//   缩放模式(调库缺口 Draw_ImgScaled 落地):
//     GY_IMG_NONE —— 原尺寸居中(大图裁掉溢出;默认,向后兼容旧行为)
//     GY_IMG_FIT  —— 等比缩放到刚好放进控件内容盒,留黑边(letterbox/pillarbox)
//     GY_IMG_FILL —— 拉伸铺满控件内容盒(不保持宽高比)
//   视频画面 = Image 处于 FIT 模式 + app 每帧换 GYimg 像素(见 project_Demo/video_player)。
//===========================================================================

typedef enum
{
	GY_IMG_NONE = 0, //原尺寸居中(默认)
	GY_IMG_FIT,      //等比适配 + 黑边
	GY_IMG_FILL,     //拉伸铺满
}GYimg_scale_mode;

GYOBJ YMGUI_Creat_Image_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
void  YMGUI_Image_SetSrc(GYOBJ img, GYIMG src);//设图片源(标脏)
//设缩放模式(标脏)。默认 GY_IMG_NONE(居中原尺寸)
void  YMGUI_Image_SetScaleMode(GYOBJ img, GYimg_scale_mode mode);
GYimg_scale_mode YMGUI_Image_GetScaleMode(GYOBJ img);

#endif // !YMGUI_IMAGE_H
