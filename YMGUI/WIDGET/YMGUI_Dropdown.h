#ifndef YMGUI_DROPDOWN_H
#define YMGUI_DROPDOWN_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 下拉框(Dropdown):合起时是一格"选中项+下拉箭头";点击展开一张浮动菜单。
//   菜单挂在 ctx 的 top_layer(顶层) → 跨子树置顶、不被任何父的 ClipChildren 裁掉,
//   这是本控件相对 List 的关键区别(List 的条目受视口裁剪,Dropdown 的菜单浮在最上)。
//   菜单外点击(经全屏透明 backdrop)自动收起。菜单在屏幕下方放不下时向上翻。
//===========================================================================

#define GY_DROPDOWN_MAX_OPT  16 //选项上限
#define GY_DROPDOWN_OPT_LEN  32 //单个选项文字上限(含结尾 '\0')

//选中变化回调:sel 为新选中项下标(0 基)
typedef void (*GYdropdown_sel_cb)(GYOBJ dropdown, uint16 sel);

//创建下拉框(合起态尺寸 w x h)
GYOBJ YMGUI_Creat_Dropdown_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
//追加一个选项(文字,拷贝并截断)。返回其下标;满则返回 -1
int   YMGUI_Dropdown_AddOption(GYOBJ dd, const char* text);
//设选中项(下标越界忽略),标脏;不触发回调
void  YMGUI_Dropdown_SetSelected(GYOBJ dd, uint16 sel);
//取当前选中项下标(无选项时返回 0)
uint16 YMGUI_Dropdown_GetSelected(GYOBJ dd);
//取选项个数
uint16 YMGUI_Dropdown_GetOptionCount(GYOBJ dd);
//设选中变化回调(用户选了菜单某项时调用)
void  YMGUI_Dropdown_SetSelectedCb(GYOBJ dd, GYdropdown_sel_cb cb);
//菜单当前是否展开
uint8 YMGUI_Dropdown_IsOpen(GYOBJ dd);
//主动收起菜单(已收起则无操作)
void  YMGUI_Dropdown_Close(GYOBJ dd);

#endif // !YMGUI_DROPDOWN_H
