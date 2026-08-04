#ifndef YMGUI_CANVAS_H
#define YMGUI_CANVAS_H

#include "YMGUI_PubType.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Obj.h"

#if YMGUI_CANVAS

//===========================================================================
// 画布(Canvas):可写像素缓冲的缩放位图视口。自绘型控件(单 draw_cb,
//   type 保持 GY_OBJ_Base,靠 user_data+draw_cb 区分)。
//
//   补的库缺口:全库此前无"可编辑像素"控件——Image/GYimg 只读只显示(blit const GYpx*)。
//   PS 式画板需要一块能被画进去、能缩放/平移看清、能把屏幕点击映射回画布像素的表面。
//   对标 lv_canvas。
//
//   职责边界(经用户拍板"App 侧合成"):
//     - 控件只管:拥有一块画布分辨率的显示缓冲(cw×ch 的 GYpx)、整数缩放 + 平移显示、
//       屏幕坐标 → 画布像素坐标映射、把绘制意图(按下/移动/抬起 + 画布坐标)回调给 app。
//     - 不管:图层栈、融合模式(blend)、合成、画笔语义 —— 全在 app 侧(见 project_Demo/image_edit)。
//   app 把可见图层合成进画布显示缓冲(YMGUI_Canvas_GetBuffer 拿指针直接写),控件负责显示。
//
//   容量创建期传参(学 EditView/Grid 教训:不用大默认尺寸绑架调用方 RAM)。
//===========================================================================

//绘制阶段
typedef enum
{
	GY_CANVAS_DOWN = 0, //指针在画布上按下(画笔起笔 / 一次点击开始)
	GY_CANVAS_MOVE,     //按住拖动(画笔行进)
	GY_CANVAS_UP,       //抬起(画笔收笔)
}GYcanvas_phase;

//绘制意图回调:app 据此往图层里落笔。cx,cy = 画布像素坐标(可能越界,见 in_bounds)。
//  in_bounds = 1 表示 (cx,cy) 落在 [0,cw)×[0,ch) 内;0 表示指针在画布外(app 通常忽略或据此连线)。
typedef void (*GYcanvas_paint_cb)(GYOBJ canvas, GYcanvas_phase phase,
                                  int32 cx, int32 cy, uint8 in_bounds);

//创建画布(视口 w x h;cw x ch = 画布像素分辨率,分配 cw*ch 个 GYpx 显示缓冲)
GYOBJ YMGUI_Creat_Canvas_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h,
                               uint16 cw, uint16 ch);

//画布像素分辨率
uint16 YMGUI_Canvas_GetW(GYOBJ canvas);
uint16 YMGUI_Canvas_GetH(GYOBJ canvas);

//显示缓冲直取(cw*ch 行优先 GYpx,app 合成后直接写这里)。SetBuffer 后需 Invalidate 才上屏
GYpx* YMGUI_Canvas_GetBuffer(GYOBJ canvas);
//标记画布内容已变,需重绘(app 写完显示缓冲后调)
void  YMGUI_Canvas_Invalidate(GYOBJ canvas);

//整数缩放(1..GY_CANVAS_ZOOM_MAX,每画布像素显示成 zoom×zoom 块),标脏
void  YMGUI_Canvas_SetZoom(GYOBJ canvas, uint8 zoom);
uint8 YMGUI_Canvas_GetZoom(GYOBJ canvas);
//平移(画布左上角相对视口左上角的像素偏移;负=画布向左上移),钳到合理范围,标脏
void  YMGUI_Canvas_SetPan(GYOBJ canvas, int32 pan_x, int32 pan_y);
void  YMGUI_Canvas_GetPan(GYOBJ canvas, int32* pan_x, int32* pan_y);

//平移模式:1 = 拖动平移画布(手型工具),0 = 拖动派发绘制回调(默认)
void  YMGUI_Canvas_SetPanMode(GYOBJ canvas, uint8 on);
uint8 YMGUI_Canvas_GetPanMode(GYOBJ canvas);

//屏幕坐标 → 画布像素坐标(考虑缩放/平移)。返回 1 = 落在画布内,0 = 画布外(out 仍写映射值)
uint8 YMGUI_Canvas_ScreenToCanvas(GYOBJ canvas, GYcoord sx, GYcoord sy, int32* cx, int32* cy);

//当前是否正在绘制(指针按下且非平移模式)+ 最近画布坐标。
//  供 app 逐帧驱动喷枪:喷口固定时 MOVE 不再来,app 每帧查此函数持续喷洒。出参可 NULL。
uint8 YMGUI_Canvas_IsDrawing(GYOBJ canvas, int32* cx, int32* cy);

//回调
void  YMGUI_Canvas_SetPaintCb(GYOBJ canvas, GYcanvas_paint_cb cb);

//缩放上限
#ifndef GY_CANVAS_ZOOM_MAX
#define GY_CANVAS_ZOOM_MAX 16
#endif

#endif // YMGUI_CANVAS

#endif // !YMGUI_CANVAS_H
