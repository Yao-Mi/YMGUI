#ifndef YMGUI_ROLLER_H
#define YMGUI_ROLLER_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 滚轮 / 居中高亮平滑滚动列表(Roller):通用等高文本行列表。
//   自绘型(单 draw_cb,type 仍 GY_OBJ_Base,靠 user_data+draw_cb 区分)。
//   语义:选中行永远绘制在控件竖直正中并高亮,其余行上下排开、越界裁掉;
//   当前滚动位置(16.16 定点)每帧 Tick 向"目标行"缓动逼近 → 平滑滚动。
//
//   两用(互不排斥):
//     程序态:只调 SetSelected(idx) 改目标,Tick 驱动动画 —— 歌词/时间/日历用这个。
//     交互态(SetInteractive(1) 开启):拖动改滚动,抬起吸附到最近行并触发 changed。
//
//   行文本深拷进控件内部(定长 GY_ROLLER_LINE_MAX 字节/行)。不关心"歌词"语义。
//===========================================================================

#define GY_ROLLER_LINE_MAX 96 //单行文本上限(含 '\0'),UTF-8 字节

//选中行变化回调(交互吸附或 SetSelected 落定时触发)
typedef void (*GYroller_changed_cb)(GYOBJ roller, int32 index);

GYOBJ YMGUI_Creat_Roller_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);

//整批设行(深拷 n 行;lines 为 n 个 const char*)。重置选中钳到范围内,标脏
void  YMGUI_Roller_SetLines(GYOBJ roller, const char* const* lines, int32 n);
//追加一行(深拷)。返回新行数
int32 YMGUI_Roller_AddLine(GYOBJ roller, const char* text);
//清空所有行
void  YMGUI_Roller_Clear(GYOBJ roller);
//当前行数
int32 YMGUI_Roller_GetCount(GYOBJ roller);

//设目标选中行(钳制)。animate!=0 走缓动,==0 立即跳到位。触发 changed(若变化)
void  YMGUI_Roller_SetSelected(GYOBJ roller, int32 index, uint8 animate);
//读当前选中(目标)行
int32 YMGUI_Roller_GetSelected(GYOBJ roller);

//设可见行数(奇数为宜,居中行两侧对称)。<=0 保持
void  YMGUI_Roller_SetVisibleRows(GYOBJ roller, int32 rows);
//设行高(像素)。<=0 保持(默认字体高+8)
void  YMGUI_Roller_SetRowHeight(GYOBJ roller, GYcoord row_h);
//设颜色:背景 / 普通行文字 / 居中高亮行文字 / 高亮行底色(高亮底传 0 表示不画底)
void  YMGUI_Roller_SetColors(GYOBJ roller, GYcolor bg, GYcolor normal, GYcolor hi, GYcolor hi_bg);
//开/关交互态(拖动滚动+吸附)。默认关(纯程序驱动)
void  YMGUI_Roller_SetInteractive(GYOBJ roller, uint8 on);
//设缓动逼近速度(每帧向目标移动的比例分之一,越大越慢;默认 4)。<=0 视为立即到位
void  YMGUI_Roller_SetEaseDiv(GYOBJ roller, int32 div);
//选中行变化回调
void  YMGUI_Roller_SetChanged(GYOBJ roller, GYroller_changed_cb cb);

//推进动画一帧(当前位置向目标缓动)。有变化则标脏。返回是否仍在移动(1=还在动)
uint8 YMGUI_Roller_Tick(GYOBJ roller);

#endif // !YMGUI_ROLLER_H
