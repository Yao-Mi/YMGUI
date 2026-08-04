#ifndef YMGUI_GRID_H
#define YMGUI_GRID_H

#include "YMGUI_PubType.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Obj.h"

#if YMGUI_GRID

//===========================================================================
// 网格(Grid):通用可编辑单元格网格。自绘型控件(单 draw_cb 画全部单元格,
//   type 保持 GY_OBJ_Base,靠 user_data+draw_cb 区分,不为每格建对象)。
//
//   相对 Table 的差别(Table 只做只读行选中/纵向滚动):
//     - 单元格级选中(sel_row/sel_col)与单元格级编辑意图回调
//     - 二维滚动(scroll_x/scroll_y),列宽各异
//     - sticky 行列表头:顶部列名行(A/B/C…)+ 左侧行号列(1/2/3…)+ 左上角块
//   复用:Table 的 sticky+clip 绘制思路、拖动阈值区分点击/滚动。
//
//   分层:Grid 只管"显示的字符串 + 选中 + 滚动 + 编辑意图",不认识公式/数字类型。
//   电子表格语义(A1 地址、公式、重算、定点格式化)全在 app 侧(见 project_Demo/excel_edit)。
//
//   容量创建期传参(学 EditView v5 教训:不用大默认容量绑架调用方 RAM)。
//===========================================================================

#ifndef GY_GRID_MAX_COLS
#define GY_GRID_MAX_COLS 26   //列上限(A..Z);要更多列可调大
#endif
#ifndef GY_GRID_MAX_ROWS
#define GY_GRID_MAX_ROWS 128  //行上限
#endif
#ifndef GY_GRID_CELL_LEN
#define GY_GRID_CELL_LEN 24   //单元格显示文字上限(含 '\0')
#endif
#ifndef GY_GRID_MAX_MERGES
#define GY_GRID_MAX_MERGES 16 //合并区上限(定长,不用堆增长)
#endif

//单元格文本水平对齐
enum { GY_ALIGN_LEFT = 0, GY_ALIGN_CENTER = 1, GY_ALIGN_RIGHT = 2 };

//单元格选中回调:被单击选中的单元格(row/col 0 基)
typedef void (*GYgrid_sel_cb)(GYOBJ grid, uint16 row, uint16 col);
//单元格编辑请求回调:双击或按回车/开始打字时,app 决定弹就地输入框/进公式栏
typedef void (*GYgrid_edit_cb)(GYOBJ grid, uint16 row, uint16 col);

//创建网格(可视区 w x h;rows/cols 为容量,>上限则钳到上限,0 兜底为 1)
GYOBJ YMGUI_Creat_Grid_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h,
                             uint16 rows, uint16 cols);

//列宽(像素;<=0 忽略),标脏
void  YMGUI_Grid_SetColWidth(GYOBJ grid, uint16 col, GYcoord width);
//行高 / 表头(列名行高 = 行号列不另设,复用 head_h)。<=0 保持原值。全局设:铺到每一行
void  YMGUI_Grid_SetRowHeight(GYOBJ grid, GYcoord row_h, GYcoord head_h);
//设/读单行行高(仅该行;<=0 忽略 / 越界返回默认)
void    YMGUI_Grid_SetRowHeightAt(GYOBJ grid, uint16 row, GYcoord h);
GYcoord YMGUI_Grid_GetRowHeightAt(GYOBJ grid, uint16 row);
//读单列列宽(越界返回 0)
GYcoord YMGUI_Grid_GetColWidth(GYOBJ grid, uint16 col);
//行号列宽(左侧 1/2/3 那列的像素宽;<=0 保持)
void  YMGUI_Grid_SetHeadColWidth(GYOBJ grid, GYcoord w);

//设/读某格显示文字(越界忽略/返回 "")
void  YMGUI_Grid_SetCellText(GYOBJ grid, uint16 row, uint16 col, const char* text);
const char* YMGUI_Grid_GetCellText(GYOBJ grid, uint16 row, uint16 col);

uint16 YMGUI_Grid_GetRowCount(GYOBJ grid);
uint16 YMGUI_Grid_GetColCount(GYOBJ grid);

//选中单元格(row/col 任一 <0 或越界 → 取消选中);锚点=活动格(单格选区);不触发回调,标脏
void  YMGUI_Grid_SetSelected(GYOBJ grid, int32 row, int32 col);
//读"活动格"(选区的移动端;未选中时两个出参写 -1)。向后兼容:单格选区时即那一格。出参可为 NULL
void  YMGUI_Grid_GetSelected(GYOBJ grid, int32* row, int32* col);

//设矩形选区(锚点 (r0,c0) + 活动格 (r1,c1));任一越界 → 取消选中。不触发回调,标脏
void  YMGUI_Grid_SetSelectedRange(GYOBJ grid, int32 r0, int32 c0, int32 r1, int32 c1);
//读矩形选区(归一后 top<=bottom、left<=right;未选中时 4 个出参写 -1)。出参可为 NULL
void  YMGUI_Grid_GetSelectedRange(GYOBJ grid, int32* r0, int32* c0, int32* r1, int32* c1);

//二维滚动(像素,钳到内容范围),标脏
void  YMGUI_Grid_SetScroll(GYOBJ grid, int32 scroll_x, int32 scroll_y);
void  YMGUI_Grid_GetScroll(GYOBJ grid, int32* scroll_x, int32* scroll_y);
//滚动使指定单元格进入可见区(选中格移动/编辑时用)
void  YMGUI_Grid_EnsureVisible(GYOBJ grid, uint16 row, uint16 col);

//取某格当前的屏幕绝对矩形(用于在格上叠就地编辑框)。已减去滚动、含表头偏移。
//返回 1 = 该格与单元格视口有交叠(至少部分可见)且 out 有效;0 = 越界/完全滚出/被表头遮住(out 不写)。
uint8 YMGUI_Grid_GetCellRect(GYOBJ grid, uint16 row, uint16 col, GYRECT out);

//---- 合并区(纯视觉:锚点格横跨整片,被覆盖格不画/命中归一到锚点)----
//合并 [r0,c0]..[r1,c1](自动归一)。与已有合并区重叠 / 越界 / 单格 / 满额 → 忽略,返回 0;成功返回 1,标脏
uint8 YMGUI_Grid_MergeCells(GYOBJ grid, int32 r0, int32 c0, int32 r1, int32 c1);
//取消 (row,col) 所在的合并区(不在任何合并区则无操作),标脏
void  YMGUI_Grid_UnmergeAt(GYOBJ grid, uint16 row, uint16 col);
//查询 (row,col) 是否落在某合并区:是则把该区(归一)写入 *out(可 NULL)并返回 1;否返回 0
uint8 YMGUI_Grid_GetMergeAt(GYOBJ grid, uint16 row, uint16 col, int32* r0, int32* c0, int32* r1, int32* c1);

//---- 每格文本水平对齐(GY_ALIGN_*)----
void  YMGUI_Grid_SetCellAlign(GYOBJ grid, uint16 row, uint16 col, uint8 align);
uint8 YMGUI_Grid_GetCellAlign(GYOBJ grid, uint16 row, uint16 col);

//---- 插入/删除整行整列(Grid 侧仅搬自身拥有的:文字/对齐/行高列宽/合并区)----
//   公式引用调整、A1 语义归 app。插入点及其后的行/列后移一格;末行/末列被挤出丢弃。
//   合并区:跨插入点的合并区随之增大;完全落在被删行/列的合并区丢弃;跨删除线的缩小。
//   选区一律清除(简化,交 app 重设)。越界忽略。
void  YMGUI_Grid_InsertRow(GYOBJ grid, uint16 at);
void  YMGUI_Grid_DeleteRow(GYOBJ grid, uint16 at);
void  YMGUI_Grid_InsertCol(GYOBJ grid, uint16 at);
void  YMGUI_Grid_DeleteCol(GYOBJ grid, uint16 at);

//回调
void  YMGUI_Grid_SetSelectCb(GYOBJ grid, GYgrid_sel_cb cb);
void  YMGUI_Grid_SetEditCb(GYOBJ grid, GYgrid_edit_cb cb);

#endif // YMGUI_GRID

#endif // !YMGUI_GRID_H
