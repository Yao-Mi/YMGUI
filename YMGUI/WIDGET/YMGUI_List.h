#ifndef YMGUI_LIST_H
#define YMGUI_LIST_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 可滚动列表容器:开 ClipChildren,子项纵向堆叠,拖动改 scroll_y(钳到内容范围)
//   AddItem 便捷加一个定高文字条目(返回该条目对象,可挂 clicked)
//===========================================================================

GYOBJ YMGUI_Creat_List_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
//加一个条目(文字),高度 item_h,纵向紧接上一条。返回条目对象
GYOBJ YMGUI_List_AddItem(GYOBJ list, const char* text, GYcoord item_h);
//设滚动位置(钳到 [0, 内容高-视口高]),标脏
void  YMGUI_List_SetScroll(GYOBJ list, GYcoord scroll_y);
GYcoord YMGUI_List_GetScroll(GYOBJ list);

#endif // !YMGUI_LIST_H
