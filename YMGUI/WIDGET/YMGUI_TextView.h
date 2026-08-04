#ifndef YMGUI_TEXTVIEW_H
#define YMGUI_TEXTVIEW_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 多行文本视图(TextView):只读、可纵向滚动的多行文本显示控件。
//   自绘型(单 draw_cb 画全部可见行,type 仍是 GY_OBJ_Base,靠 user_data+draw_cb 区分),
//   内部持有文本副本(SetText 时深拷),换行位置预算成"行表"(offset+字节长),只在
//   文本/属性/宽度变化时重算,绘制时只画可见行(scroll_y 裁剪),复用 Table 的拖动滚动语义。
//
//   两个可选属性(互相独立):
//     Multiline(默认开):遇 '\n' 断行。关掉则忽略 '\n' 当普通字符,整段视作逻辑一行。
//     Wrap(默认关):按控件宽度自动折行(贪心按码点断)。关掉则长行右侧裁掉(不横向滚)。
//   编辑光标、富文本多色段、横向滚动不在本控件职责内(编辑归 TextInput)。
//===========================================================================

//创建文本视图(可视区尺寸 w x h)。初始空文本,Multiline 开、Wrap 关
GYOBJ YMGUI_Creat_TextView_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
//设文本(深拷一份到控件内;传 NULL 或 "" 清空)。重算行表 + 钳滚动 + 标脏
void  YMGUI_TextView_SetText(GYOBJ tv, const char* text);
//取当前文本(内部副本指针;从未设过返回空串 "")
const char* YMGUI_TextView_GetText(GYOBJ tv);
//是否按 '\n' 断行(默认开=1)。变则重算行表
void  YMGUI_TextView_SetMultiline(GYOBJ tv, uint8 on);
//是否按控件宽度自动折行(默认关=0)。变则重算行表
void  YMGUI_TextView_SetWrap(GYOBJ tv, uint8 on);
//行高(像素,<=0 保持原值,默认字体高+4)。变则钳滚动 + 标脏
void  YMGUI_TextView_SetLineHeight(GYOBJ tv, GYcoord line_h);
//文字颜色
void  YMGUI_TextView_SetTextColor(GYOBJ tv, GYcolor color);
//设滚动位置(钳到 [0, 内容高-视口高]),标脏
void  YMGUI_TextView_SetScroll(GYOBJ tv, GYcoord scroll_y);
GYcoord YMGUI_TextView_GetScroll(GYOBJ tv);
//当前行数(断行/折行后的显示行数)
uint16 YMGUI_TextView_GetLineCount(GYOBJ tv);

#endif // !YMGUI_TEXTVIEW_H
