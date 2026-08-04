#ifndef YMGUI_PUBTYPE_H
#define YMGUI_PUBTYPE_H

#include <stdint.h>
#include <stddef.h>

//常用平台数据类型(与 YMCV Pubtype.h 对齐,移植时只改这里)
typedef int32_t  int32;
typedef int16_t  int16;
typedef int64_t  int64;
typedef int8_t   int8;

typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t  uint8;

typedef float    float32;
typedef double   float64;

//===========================================================================
// 颜色深度选择(编译期):决定 framebuffer 里 GYpx 的实际存储格式
//   16 → RGB565(默认,绝大多数中小 LCD)
//    8 → 灰度 8bit
//    1 → 单色 1bpp(位打包,无法抗锯齿)
//===========================================================================
#ifndef YMGUI_COLOR_DEPTH
#define YMGUI_COLOR_DEPTH 16
#endif

//---------------------------------------------------------------------------
// 三层颜色分离(见代码风格 §C.1):
//   GYcolor —— API 颜色,32位 ARGB,用户描述用,不是存储格式
//   GYpx    —— 表面像素,按 COLOR_DEPTH 决定,framebuffer 实际存储
//   GYopa   —— 8位覆盖度,混合那一瞬间用,不进 framebuffer
//---------------------------------------------------------------------------
typedef uint32 GYcolor;  //0xAARRGGBB
typedef uint8  GYopa;    //0=全透明 255=不透明(覆盖度常量 GY_OPA_* 见 PubDefine.h)

#if   YMGUI_COLOR_DEPTH == 16
typedef uint16 GYpx;     //RGB565
#elif YMGUI_COLOR_DEPTH == 8
typedef uint8  GYpx;     //灰度
#elif YMGUI_COLOR_DEPTH == 1
typedef uint8  GYpx;     //单色,位打包时按字节访问
#else
#error "YMGUI_COLOR_DEPTH must be 1, 8 or 16"
#endif

//===========================================================================
// 数值类型:定点/整数为主(无 FPU 友好,见代码风格 §C.2)
//===========================================================================
//几何/布局坐标:整数像素是工作单位。裸机屏小用 int16 省内存,桌面可换 int32
#ifndef YMGUI_COORD_32
typedef int16 GYcoord;
#else
typedef int32 GYcoord;
#endif

//子像素/动画/字体度量:16.16 定点(运算宏 GY_FP* 见 PubDefine.h)
typedef int32 GYvalue;

//===========================================================================
// 基础几何类型(值类型小写 + 指针 typedef 全大写,成对出现)
//===========================================================================
//坐标点
typedef struct
{
	GYcoord x, y;
}GYpoint;
typedef GYpoint* GYPOINT;

//矩形(x,y 为左上角,w,h 为宽高)
typedef struct
{
	GYcoord x, y, w, h;
}GYrect;
typedef GYrect* GYRECT;

#endif // !YMGUI_PUBTYPE_H
