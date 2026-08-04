#ifndef YMGUI_CHART_H
#define YMGUI_CHART_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 折线图:网格 + 多序列多点折线。y 值按 [min,max] 映射到控件高度。
// 点沿 x 均布(point_cnt 个)。支持流式追加(SetNext 整体左移末尾入新值)。
// 只显示不交互(无 event_cb)。
//===========================================================================

#define GY_CHART_MAX_SERIES 4//最大序列数

GYOBJ YMGUI_Creat_Chart_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);

void  YMGUI_Chart_SetRange(GYOBJ chart, int32 min, int32 max);//设 y 值域(钳制既有点)
void  YMGUI_Chart_SetPointCount(GYOBJ chart, uint16 count);//设每序列点数(重置为 min)
void  YMGUI_Chart_SetGrid(GYOBJ chart, uint8 hdiv, uint8 vdiv);//设网格分格数(0=不画)
void  YMGUI_Chart_SetColors(GYOBJ chart, GYcolor bg, GYcolor grid);//背景/网格色

int32 YMGUI_Chart_AddSeries(GYOBJ chart, GYcolor color);//加序列,返回序列索引(满/失败返回 -1)
void  YMGUI_Chart_SetValue(GYOBJ chart, int32 series, uint16 idx, int32 value);//设某序列某点(钳制)
void  YMGUI_Chart_SetNext(GYOBJ chart, int32 series, int32 value);//整体左移,末尾入新值(流式)
int32 YMGUI_Chart_GetValue(GYOBJ chart, int32 series, uint16 idx);//读某点

#endif // !YMGUI_CHART_H
