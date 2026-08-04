#ifndef YMGUI_INVALIDATE_H
#define YMGUI_INVALIDATE_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 失效/脏矩形系统(保留模式发动机)
//   控件状态变 → Invalidate → 向上传播 + 合并脏区 → Refresh 只重绘脏区
//===========================================================================

//标记对象需要重绘(算出其屏幕绝对区域,并入 ctx 脏矩形)
void YMGUI_Obj_Invalidate(GYOBJ obj);
//标记一块屏幕区域需要重绘(并入 ctx 脏矩形)
void YMGUI_Ctx_InvalidateArea(GYCTX ctx, const GYrect* area);

//刷新一帧:合并脏矩形 → 按 buf_px_cnt 切 band → 逐 band 重绘相交对象 → flush
//  无脏区时直接返回(保留模式空闲不耗)
void YMGUI_Refresh(GYCTX ctx);

#endif // !YMGUI_INVALIDATE_H
