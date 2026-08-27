#ifndef YMGUI_BUTTON_H
#define YMGUI_BUTTON_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"
#include "YMGUI_DrawImg.h"

//===========================================================================
// 按钮控件:GYobj + 自定义 draw_cb(按下态换色 + 居中标题/贴图) + 用户可挂 clicked 回调
//   贴图:设了图源就居中 blit 图、不画文字(图优先);状态切换(如播放/暂停)由 app 换图,
//   同 SetText 换字一个套路。底色+边框可 SetBgVisible 关掉(纯图标按钮)。
//===========================================================================
#ifndef GY_BTN_TEXT_MAX
#define GY_BTN_TEXT_MAX 32
#endif

//用户点击回调
typedef void (*GYbtn_clicked_cb)(GYOBJ btn);

//创建按钮,挂到 parent
GYOBJ YMGUI_Creat_Button_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
//设置常态/按下态颜色
void  YMGUI_Button_SetColors(GYOBJ btn, GYcolor normal, GYcolor pressed);
//设置点击回调
void  YMGUI_Button_SetClicked(GYOBJ btn, GYbtn_clicked_cb cb);
//启用按住连发；delay_ms 后首次触发，之后每 interval_ms 触发。任一参数为 0 表示关闭
void  YMGUI_Button_SetRepeat(GYOBJ btn, uint32 delay_ms, uint32 interval_ms);
//设置按钮标题文字(居中显示)
void  YMGUI_Button_SetText(GYOBJ btn, const char* text);
//设置按钮图源(居中 blit,不拥有像素,调用方保证存活;NULL=清图回退文字)。设了图则不画文字
void  YMGUI_Button_SetImage(GYOBJ btn, GYIMG src);
//底色+边框是否绘制(默认 1 开;关掉=纯图标/透明按钮,靠图自身或 colorkey 抠形)
void  YMGUI_Button_SetBgVisible(GYOBJ btn, uint8 on);

#endif // !YMGUI_BUTTON_H
