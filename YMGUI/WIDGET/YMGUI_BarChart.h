#ifndef YMGUI_BARCHART_H
#define YMGUI_BARCHART_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 通用柱状图(BarChart):N 个竖直柱,每柱按 value/[0,max] 映射高度。
//   自绘型(单 draw_cb,type 仍 GY_OBJ_Base)。只显示不交互。
//   两个正交维度,组合出频谱/直方图/电平表/统计柱图等:
//
//   【配色模式 ColorMode】
//     BY_HEIGHT —— 柱身逐行竖直渐变:每根柱从底部 lo 色向上渐变到 hi 色(以控件全高为标尺,
//                  柱越高其顶端越接近 hi)。适合"同一物理量不同强度"(频谱/电平表)。SetGradient 设两端色。
//     PER_BAR   —— 每柱一个独立颜色(调色板),与高度无关。适合"不同类目"
//                  (各分区销量/各核负载)。SetBarColor / SetBarColors 设色。
//
//   【顶部回落 TopMode】(原"峰值保持"泛化,库侧只做高亮条那种;文字顶标交给 app 叠 Label)
//     NONE —— 不画顶标。配合 SetDecay(0,0) 即为一张静态普通柱状图。
//     BAR  —— 顶部一根高亮细条:被新值顶上去,之后每帧按 top_fall 回落 → 峰值悬停手感。
//
//   库不做数据分析(频段能量/统计),值一律由外部喂进来(见 project_Demo/music_player)。
//===========================================================================

#define GY_BARCHART_MAX_BARS 64 //柱数上限

//配色模式
#define GY_BARCHART_COLOR_BY_HEIGHT 0 //随高度 lo→hi 渐变(默认)
#define GY_BARCHART_COLOR_PER_BAR   1 //每柱独立调色板

//顶部回落模式
#define GY_BARCHART_TOP_NONE 0 //不画顶标
#define GY_BARCHART_TOP_BAR  1 //高亮细条(峰值保持+回落,默认)

GYOBJ YMGUI_Creat_BarChart_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);

//设柱数(钳到 [1, GY_BARCHART_MAX_BARS]),值/顶标清零,PER_BAR 调色板重铺默认渐变
void  YMGUI_BarChart_SetBarCount(GYOBJ bc, int32 n);
int32 YMGUI_BarChart_GetBarCount(GYOBJ bc);
//设值域上限(value 映射到 [0,max] → 高度)。<=0 视为 1
void  YMGUI_BarChart_SetRange(GYOBJ bc, int32 max);
//设某柱值(钳制)。TopMode=BAR 时更高则同时顶起顶标
void  YMGUI_BarChart_SetValue(GYOBJ bc, int32 i, int32 value);
//整批设值(arr 有 n 个;超柱数忽略,不足留旧)
void  YMGUI_BarChart_SetValues(GYOBJ bc, const int32* arr, int32 n);
int32 YMGUI_BarChart_GetValue(GYOBJ bc, int32 i);

//配色模式(GY_BARCHART_COLOR_*)
void  YMGUI_BarChart_SetColorMode(GYOBJ bc, uint8 mode);
int32 YMGUI_BarChart_GetColorMode(GYOBJ bc);
//BY_HEIGHT:设渐变两端色(低段柱色 → 高段柱色)
void  YMGUI_BarChart_SetGradient(GYOBJ bc, GYcolor lo, GYcolor hi);
//PER_BAR:设某柱颜色(越界忽略)
void  YMGUI_BarChart_SetBarColor(GYOBJ bc, int32 i, GYcolor color);
//PER_BAR:整批设柱色(arr 有 n 个;超柱数忽略,不足留旧)
void  YMGUI_BarChart_SetBarColors(GYOBJ bc, const GYcolor* arr, int32 n);
//背景色
void  YMGUI_BarChart_SetBgColor(GYOBJ bc, GYcolor bg);
//顶部回落模式(GY_BARCHART_TOP_*)
void  YMGUI_BarChart_SetTopMode(GYOBJ bc, uint8 mode);
//顶标条颜色(TopMode=BAR 时用)
void  YMGUI_BarChart_SetTopColor(GYOBJ bc, GYcolor color);
//设每帧衰减量(柱身回落 fall,顶标回落 top_fall)。<0 保持
void  YMGUI_BarChart_SetDecay(GYOBJ bc, int32 fall, int32 top_fall);
//柱间空隙(像素)。<0 保持(默认 2)
void  YMGUI_BarChart_SetGap(GYOBJ bc, GYcoord gap);

//推进动画一帧:柱身与顶标按衰减量回落。有变化则标脏。返回是否仍在动
uint8 YMGUI_BarChart_Tick(GYOBJ bc);

#endif // !YMGUI_BARCHART_H
