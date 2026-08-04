#ifndef SDL_LCD_H
#define SDL_LCD_H

#include "YMGUI_Hal.h"

//===========================================================================
// SDL 假 LCD:实现 HAL 的 flush_cb,把软件渲染好的 framebuffer 推给 SDL 纹理
//   "假装自己是一块 LCD 面板"。移植到真实硬件时照此写一个 SPI/并口版 flush_cb
//===========================================================================
//初始化窗口 + 纹理,并填好 disp 的 flush_cb / user_data。
//  disp:调用者提供的 GYdisp,本函数负责挂上 flush_cb
//  scale:窗口放大倍数(小屏调试用,1=原始)
void SDL_LCD_Init(GYDISP disp, int scale);
//销毁
void SDL_LCD_Destroy(void);
//抽干事件队列;返回 0 表示收到退出请求
int  SDL_LCD_PumpEvents(void);
//延时(ms)
void SDL_LCD_Delay(int ms);

#endif // !SDL_LCD_H
