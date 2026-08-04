#ifndef YMGUI_TABVIEW_H
#define YMGUI_TABVIEW_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 标签页容器(Tabview):顶部一条 tab bar(等分分段,当前页高亮+下划线) + 下方内容区。
//   每个 tab 对应一个"页"对象(内容区里的子容器,开 ClipChildren)。
//   同一时刻只显示当前页,切页复用第 11 轮的 YMGUI_Obj_SetHidden
//   (隐藏页被 draw / hit-test 双双跳过 → 切页干净,无需 z-order 处理)。
//   tab bar 的点击落在 tabview 自身(页在 bar 之下),由 point_x 算命中的 tab 下标。
//===========================================================================

#define GY_TABVIEW_MAX_TABS 8  //标签页上限
#define GY_TABVIEW_TAB_LEN  16 //单个 tab 标题上限(含 '\0')

//切页回调:active 为新当前页下标(0 基)
typedef void (*GYtabview_changed_cb)(GYOBJ tabview, uint16 active);

//创建标签页容器(整体尺寸 w x h,含 tab bar 与内容区)
GYOBJ YMGUI_Creat_Tabview_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
//加一个标签页(标题拷贝截断)。返回该页的内容容器对象(用户往里加控件);满或失败返回 NULL
GYOBJ YMGUI_Tabview_AddTab(GYOBJ tabview, const char* title);
//切到第 idx 页(越界忽略);标脏,不触发回调
void  YMGUI_Tabview_SetActive(GYOBJ tabview, uint16 idx);
//取当前页下标
uint16 YMGUI_Tabview_GetActive(GYOBJ tabview);
//取标签页数
uint16 YMGUI_Tabview_GetTabCount(GYOBJ tabview);
//取第 idx 页的内容容器(越界返回 NULL)
GYOBJ YMGUI_Tabview_GetPage(GYOBJ tabview, uint16 idx);
//设 tab bar 高(默认 24);重排各页内容区,标脏
void  YMGUI_Tabview_SetBarHeight(GYOBJ tabview, GYcoord bar_h);
//设切页回调(用户点 tab bar 切换时调用)
void  YMGUI_Tabview_SetChangedCb(GYOBJ tabview, GYtabview_changed_cb cb);

#endif // !YMGUI_TABVIEW_H
