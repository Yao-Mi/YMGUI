#ifndef YMGUI_LAYOUT_H
#define YMGUI_LAYOUT_H

#include "YMGUI_PubType.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 轻量一次性布局助手(无状态、零每对象 RAM)
//   调用时按父/对象的当前尺寸算好坐标写进 area,再标脏一次返回。
//   不存约束、不跑 reflow、不加每对象内存 —— 与 flex/grid 引擎的取舍分野。
//   布局变动(加子/改尺寸)时由调用者重跑一次,不像 reactive 引擎每帧算。
//   YMGUI_LAYOUT=0 时整模块 #if 裁空,库核心不依赖它 → 无悬空符号。
//===========================================================================

#if YMGUI_LAYOUT

//主轴方向:子对象沿此轴依次排布
typedef enum
{
	GY_LAYOUT_VER = 0, //竖排(主轴=y)
	GY_LAYOUT_HOR,     //横排(主轴=x)
}GYLayoutDir;

//交叉轴对齐:子对象在垂直于主轴的方向上如何摆放
typedef enum
{
	GY_CROSS_START = 0, //贴交叉轴起点(左/上,离边 pad)
	GY_CROSS_CENTER,    //交叉轴居中
	GY_CROSS_END,       //贴交叉轴终点(右/下,离边 pad)
}GYLayoutCross;

//九点对齐(相对父内容盒)
typedef enum
{
	GY_ALIGN_CENTER = 0, //正中
	GY_ALIGN_TL, GY_ALIGN_TM, GY_ALIGN_TR, //上排:左/中/右
	GY_ALIGN_ML,              GY_ALIGN_MR, //中排:左/右
	GY_ALIGN_BL, GY_ALIGN_BM, GY_ALIGN_BR, //下排:左/中/右
}GYAlign;

//把 parent 的子对象沿主轴依次排布(主轴按 尺寸+gap 累进,起点=pad;交叉轴按 cross 对齐)。
//  跳过 Hidden 子(不占位);只写位置不改子的 w/h(不做 flex 的 grow/stretch)。
//  结尾标脏 parent 一次(覆盖所有子的旧+新位置)。
void YMGUI_Layout_Stack(GYOBJ parent, GYLayoutDir dir, GYcoord gap, GYcoord pad, GYLayoutCross cross);

//把 obj 在其父的内容盒里做九点对齐(pad=离边内缩)。父为 NULL 时不动作。
void YMGUI_Layout_Align(GYOBJ obj, GYAlign align, GYcoord pad);

#endif // YMGUI_LAYOUT

#endif // !YMGUI_LAYOUT_H
