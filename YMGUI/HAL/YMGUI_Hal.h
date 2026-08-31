#ifndef YMGUI_HAL_H
#define YMGUI_HAL_H

#include "YMGUI_PubType.h"

//===========================================================================
// 显示 HAL:最窄腰部。硬件唯一必须实现的是 flush_cb
//   SDL_LCD/ 里 flush_cb = 上传纹理(假 LCD);真实硬件 flush_cb = SPI/并口 DMA
//===========================================================================
typedef struct GYdisp
{
	GYcoord hor_res;   //屏幕宽(整数像素)
	GYcoord ver_res;   //屏幕高

	GYpx*   buf1;      //draw buffer,可远小于整屏
	GYpx*   buf2;      //可选第二块,flush 异步时双缓冲重叠(NULL 表示单缓冲,走同步路径)
	uint32  buf_px_cnt;//每块 buffer 能放多少像素 → 决定 band 高度

	//把 buf 里 area 大小的一块连续像素推到面板(唯一碰硬件处)
	//  同步 port:传完(阻塞)后在此调 FlushReady 再返回;异步 port:发起 DMA 即返回,DMA 中断里调 FlushReady
	void (*flush_cb)(struct GYdisp* d, const GYrect* area, const GYpx* buf);

	//—— 异步双缓冲(仅 buf2!=NULL 时用;单缓冲这两字段忽略) ——
	volatile uint8 flush_busy;//库发起异步 flush 前置 1,DMA 完成经 FlushReady 清 0。库据此不覆盖在传的 buffer
	void (*wait_cb)(struct GYdisp* d);//等 flush_busy 清零时调(裸机可填 __WFI 省电);NULL=忙等自旋

	void* user_data;   //存 SPI 句柄 / SDL_Texture 等
}GYdisp;
typedef GYdisp* GYDISP;

//flush 完成通知:清 flush_busy,库据此复用该 buffer。
//  同步 flush_cb 在函数体末尾调(此后立即可复用);异步 DMA 在传输完成中断里调。
void YMGUI_Disp_FlushReady(GYDISP d);

//一轮 Refresh 的所有脏区/band 都 flush 完成后通知显示端口。
//桌面端可在这里整帧 present；真实 LCD 通常无需注册。
typedef void (*GYframe_done_cb)(GYDISP d);
void YMGUI_Disp_SetFrameDoneCb(GYDISP d, GYframe_done_cb cb);
void YMGUI_Disp_FrameDone(GYDISP d);

//===========================================================================
// 输入注入(拉取式,裸机无 OS 消息泵)
//   SDL_LCD/ 把 SDL_Event 翻译成这些调用;裸机把触摸/按键读数翻译成这些调用
//   注入前需先注册接收事件的上下文(GUI 层的 GYCTX,用 void* 避免 HAL 反依赖 OPOBJ)
//===========================================================================
//注册接收注入事件的上下文(传 GYCTX)
void YMGUI_Inject_SetCtx(void* ctx);
//可选按键过滤器:按下/抬起均先到此处；返回 1 表示已消费。未消费的按下事件继续派给焦点控件，
//抬起事件只用于过滤器维护状态，不会进入现有文本控件。适合输入法/快捷键层统一实体与虚拟键盘。
typedef uint8 (*GYkey_filter_cb)(uint32 key, uint8 pressed, void* user_data);
void YMGUI_Inject_SetKeyFilter(GYkey_filter_cb cb, void* user_data);
//指针(触摸/鼠标):x,y 屏幕坐标,pressed=1 按下/0 抬起
void YMGUI_Inject_Pointer(GYcoord x, GYcoord y, uint8 pressed);
//按键:key 键码,pressed=1 按下/0 抬起
void YMGUI_Inject_Key(uint32 key, uint8 pressed);
//滚轮:x,y 为当前指针屏幕坐标,delta_x/y 为滚动增量
void YMGUI_Inject_Wheel(GYcoord x, GYcoord y, int32 delta_x, int32 delta_y);
//推进 GUI 时钟；SDL 端由事件泵自动调用，裸机主循环按实际经过毫秒数调用
void YMGUI_Inject_Tick(uint32 elapsed_ms);
//双击(触摸/鼠标 button.clicks==2):x,y 屏幕坐标。派 GY_EVENT_DoubleClicked 给命中对象
void YMGUI_Inject_DoubleClick(GYcoord x, GYcoord y);
//取消当前指针捕获:派 ReleasedOff 并清按下状态,不产生 Clicked
void YMGUI_Inject_PointerCancel(void);
//一次性上下文请求(右键短点击):派 ContextRequested,不建立拖动捕获
void YMGUI_Inject_ContextRequest(GYcoord x, GYcoord y);
//捕获式上下文手势(右键拖动/长按后拖动)
void YMGUI_Inject_ContextBegin(GYcoord x, GYcoord y);
void YMGUI_Inject_ContextMove(GYcoord x, GYcoord y);
void YMGUI_Inject_ContextEnd(GYcoord x, GYcoord y);
void YMGUI_Inject_ContextCancel(void);

//===========================================================================
// 剪贴板 HAL 缝(移植点,与 flush_cb/glyph_read 同哲学)
//   桌面 port(SDL_LCD)注册系统剪贴板后端,能与 OS 剪贴板互通;
//   未注册(裸机默认)时回退到库内一块固定大小静态缓冲,进程内复制粘贴照常工作。
//   控件(EditView)只调 YMGUI_Clipboard_SetText/GetText,永不直接依赖具体后端。
//===========================================================================
//剪贴板后端:set 存文本、get 返回文本指针(内部持有,调用方只读不释放)。裸机可不注册
typedef void        (*GYclip_set_cb)(const char* text);
typedef const char* (*GYclip_get_cb)(void);
//注册系统剪贴板后端(NULL/NULL 恢复库内静态缓冲)。SDL_LCD 用 SDL_SetClipboardText/GetClipboardText
void        YMGUI_Clipboard_SetBackend(GYclip_set_cb set_cb, GYclip_get_cb get_cb);
//写剪贴板(拷进后端或库内缓冲)
void        YMGUI_Clipboard_SetText(const char* text);
//读剪贴板(返回只读指针;空则返回 "")
const char* YMGUI_Clipboard_GetText(void);

#endif // !YMGUI_HAL_H
