#ifndef YMGUI_TABLE_H
#define YMGUI_TABLE_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 表格(Table):固定列(各列宽+标题) + 可滚动的行体。
//   自绘型控件(单 draw_cb 画完全部单元格,类似 Chart),不为每格建对象:
//   N 行 x M 列在裸机上逐格建对象太重。行数据内部用链表存(无需 realloc)。
//   复用:Chart 的网格线、List 的拖动滚动语义、ClipChildren 的裁剪思路
//   (draw_cb 把 clip 收窄到自身;表体再收窄到表头之下 → 滚动行不会盖表头)。
//   交互:行体纵向拖动滚动;单击某行选中(带拖动阈值,滚动拖拽不算点击)。
//===========================================================================

#define GY_TABLE_MAX_COLS  8  //列数上限
#define GY_TABLE_CELL_LEN  24 //单元格文字上限(含 '\0')
#define GY_TABLE_HEAD_LEN  16 //列标题上限(含 '\0')

//行选中回调:row 为被选中的行下标(0 基);取消选中时 row = -1
typedef void (*GYtable_row_cb)(GYOBJ table, int32 row);

//创建表格(可视区尺寸 w x h)
GYOBJ YMGUI_Creat_Table_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
//加一列(标题 + 列宽像素)。返回列下标;满或越界返回 -1
int   YMGUI_Table_AddColumn(GYOBJ table, const char* title, GYcoord width);
//加一行(返回行下标;失败返回 -1)。新行各格初始为空串
int   YMGUI_Table_AddRow(GYOBJ table);
//设某格文字(行/列越界忽略),标脏
void  YMGUI_Table_SetCell(GYOBJ table, uint16 row, uint16 col, const char* text);
//读某格文字(越界返回空串 "")
const char* YMGUI_Table_GetCell(GYOBJ table, uint16 row, uint16 col);
//行数 / 列数
uint16 YMGUI_Table_GetRowCount(GYOBJ table);
uint16 YMGUI_Table_GetColCount(GYOBJ table);
//行高 / 表头高(默认各 22)
void  YMGUI_Table_SetRowHeight(GYOBJ table, GYcoord row_h, GYcoord head_h);
//设滚动位置(钳到 [0, 内容高-表体视口高]),标脏
void  YMGUI_Table_SetScroll(GYOBJ table, GYcoord scroll_y);
GYcoord YMGUI_Table_GetScroll(GYOBJ table);
//选中行(-1 取消);标脏,不触发回调
void  YMGUI_Table_SetSelectedRow(GYOBJ table, int32 row);
int32 YMGUI_Table_GetSelectedRow(GYOBJ table);
//设行选中回调(用户单击某行时调用)
void  YMGUI_Table_SetRowCb(GYOBJ table, GYtable_row_cb cb);

#endif // !YMGUI_TABLE_H
