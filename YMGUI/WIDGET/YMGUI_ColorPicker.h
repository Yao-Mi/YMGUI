#ifndef YMGUI_COLORPICKER_H
#define YMGUI_COLORPICKER_H

#include "YMGUI_PubType.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Obj.h"

#if YMGUI_COLORPICKER

//===========================================================================
// 取色器(ColorPicker):HSV 选色控件。自绘型(单 draw_cb 画方块 + 色相条,
//   type 保持 GY_OBJ_Base,靠 user_data+draw_cb 区分)。
//
//   布局:左侧 饱和度(x)×明度(y) 方块(SV square)+ 右侧 一条竖直色相条(hue bar)。
//     - 在方块内点击/拖动 → 设 S(左0..右255)、V(上255..下0)
//     - 在色相条内点击/拖动 → 设 H(上0..下359)
//   选中色 = HSV(H,S,V) → RGB。变化时回调 changed(obj, GYcolor)。
//
//   整数 HSV<->RGB(无 FPU/libm):H 0..359、S/V 0..255。
//   分层:控件只出"当前颜色 + 交互",不认识调色板/图层等 app 语义。
//===========================================================================

//颜色变化回调(拖动/点击后新的 RGB 颜色,alpha 恒 0xFF)
typedef void (*GYcolorpicker_cb)(GYOBJ picker, GYcolor color);

//创建取色器(w x h;内部按比例切出 SV 方块 + 色相条)
GYOBJ YMGUI_Creat_ColorPicker_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);

//设当前颜色(RGB → 内部 HSV;alpha 忽略),标脏,不触发回调
void  YMGUI_ColorPicker_SetColor(GYOBJ picker, GYcolor color);
//读当前颜色(HSV → RGB,alpha=0xFF)
GYcolor YMGUI_ColorPicker_GetColor(GYOBJ picker);

//直接设 HSV(H 0..359 取模,S/V 0..255 钳制),标脏,不触发回调
void  YMGUI_ColorPicker_SetHSV(GYOBJ picker, uint16 h, uint8 s, uint8 v);
void  YMGUI_ColorPicker_GetHSV(GYOBJ picker, uint16* h, uint8* s, uint8* v);

//回调
void  YMGUI_ColorPicker_SetChangedCb(GYOBJ picker, GYcolorpicker_cb cb);

//工具函数(供 app 复用,不依赖控件实例):整数 HSV<->RGB
//  HSV_to_RGB:h 0..359、s/v 0..255 → GYcolor(alpha 0xFF)
GYcolor YMGUI_ColorPicker_HSVtoRGB(uint16 h, uint8 s, uint8 v);
//  RGB_to_HSV:color → *h 0..359、*s/*v 0..255(出参可为 NULL)
void    YMGUI_ColorPicker_RGBtoHSV(GYcolor color, uint16* h, uint8* s, uint8* v);

#endif // YMGUI_COLORPICKER

#endif // !YMGUI_COLORPICKER_H
