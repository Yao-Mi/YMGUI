#ifndef YMGUI_EDITVIEW_H
#define YMGUI_EDITVIEW_H

#include "YMGUI_PubType.h"
#include "YMGUI_Obj.h"

//===========================================================================
// 可编辑多行文本框(EditView):多行、可纵向滚动、带光标的文本编辑控件。
//   自绘型(单 draw_cb 画可见行 + 光标,type 仍是 GY_OBJ_Base,靠 user_data+draw_cb 区分)。
//   文本缓冲在创建时按 capacity 一次性堆分配(裸机优先,不动态扩容),换行位置预算成
//   "行表"(offset+字节长),只在文本/宽度变化时重算;绘制只画可见行(scroll_y 裁剪),
//   复用 TextView 的折行/滚动语义。
//
//   容量按需付费:capacity 是创建期参数,调用方按自己的用途申报——密码框给几十字节,
//   PC 文本编辑器给 128KB。控件不再定一个大到人人都得付的默认值(见 GY_EV_TEXT_MAX)。
//   撤销(undo)默认开,会再占一份等容量镜像;裸机想省 RAM 可 SetUndoEnabled(ev,0) 关掉。
//
//   编辑(聚焦时,键走 GY_EVENT_Key + ctx->last_key):
//     可打印字符/UTF-8 字节 → 插入光标处(中文正确:SDL 逐字节注入,按序拼回整码点)
//     ENTER → 插入 '\n' 换行     BACKSPACE/DEL → 删光标前/后一整个码点(跨行则合并行)
//     LEFT/RIGHT → 按码点移光标   UP/DOWN → 跨显示行移(按光标像素列就近对齐)
//   永远按 '\n' 断行(编辑器语义);Wrap 可选(默认关):开则再按宽度折行。
//===========================================================================
//建议容量常量(字节)。**不是**控件内部固定尺寸——控件实际按 Creat 传入的 capacity 分配。
//  仅供需要"大编辑器"的调用方参考取用:UTF-8 下一个汉字占 3 字节,131072(128KB)≈ 43000
//  汉字 / 128000 ASCII。len/cursor/行计数用 size_t(见 .c),容量上限即 size_t 上限。
#ifndef GY_EV_TEXT_MAX
#define GY_EV_TEXT_MAX 131072
#endif

//Tab 键在编辑器里插入的空格数(渲染器无制表位,故用空格而非 '\t')
#ifndef GY_EV_TAB_WIDTH
#define GY_EV_TAB_WIDTH 4
#endif

//文本变更回调(仅内容变化时触发,光标移动不触发);text 为内部缓冲当前值
typedef void (*GYev_changed_cb)(GYOBJ ev, const char* text);
//动作回调(无参):Ctrl+F 收到时触发,供上层弹出查找条(EditView 只转发意图,不管 UI)
typedef void (*GYev_action_cb)(GYOBJ ev);

//创建可编辑多行文本框(可视区 w x h)。初始空文本,Wrap 关,undo 默认开。
//  capacity:文本缓冲可用字节数(不含结尾 '\0',控件内部分配 capacity+1)。按用途申报,
//  0 会被钳到 1。undo 开时另占一份等容量镜像(想省 RAM 见 YMGUI_EditView_SetUndoEnabled)。
GYOBJ YMGUI_Creat_EditView_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h, size_t capacity);
//设文本(拷进内部缓冲;NULL/"" 清空,光标移末尾)。重算行表 + 钳滚动 + 标脏
void  YMGUI_EditView_SetText(GYOBJ ev, const char* text);
//取当前文本(内部缓冲指针;空返回 "")
const char* YMGUI_EditView_GetText(GYOBJ ev);
//是否按控件宽度自动折行(默认关=0)。变则重算行表
void  YMGUI_EditView_SetWrap(GYOBJ ev, uint8 on);
//文字颜色
void  YMGUI_EditView_SetTextColor(GYOBJ ev, GYcolor color);
//背景色(编辑区底色),标脏
void  YMGUI_EditView_SetBgColor(GYOBJ ev, GYcolor color);
//边框颜色(编辑区外框 1px);传全透明(alpha=0)则不画边框。默认画一圈浅灰
void  YMGUI_EditView_SetBorderColor(GYOBJ ev, GYcolor color);
//撤销开关(默认开)。开→分配等容量撤销镜像;关→释放该镜像省 RAM,Undo 变空操作。
//  裸机上不需要撤销时关掉,单实例内存占用从 2*capacity 降到 1*capacity。
void  YMGUI_EditView_SetUndoEnabled(GYOBJ ev, uint8 on);
//设滚动位置(钳到 [0, 内容高-视口高]),标脏。大文件像素高可超 int16,故用 int32
void  YMGUI_EditView_SetScroll(GYOBJ ev, int32 scroll_y);
int32 YMGUI_EditView_GetScroll(GYOBJ ev);
//当前显示行数(断行/折行后)
size_t YMGUI_EditView_GetLineCount(GYOBJ ev);
//光标字节位置(0..文本字节长)
size_t YMGUI_EditView_GetCursor(GYOBJ ev);
//文本变更回调(仅内容变化时触发)
void  YMGUI_EditView_SetChanged(GYOBJ ev, GYev_changed_cb cb);
//查找请求回调(Ctrl+F 触发);上层据此弹出/聚焦查找条
void  YMGUI_EditView_SetFindCb(GYOBJ ev, GYev_action_cb cb);

//---- 选区 ----
//是否有选区(锚点有效且不等于光标)
uint8  YMGUI_EditView_HasSelection(GYOBJ ev);
//取选区字节范围 [start,end)(无选区则 start==end==光标)。start/end 可为 NULL
void   YMGUI_EditView_GetSelection(GYOBJ ev, size_t* start, size_t* end);
//全选(光标到文尾,锚点到文首)
void   YMGUI_EditView_SelectAll(GYOBJ ev);
//清选区(锚点失效,光标不动)
void   YMGUI_EditView_ClearSelection(GYOBJ ev);
//取选中文本长度(字节)
size_t YMGUI_EditView_GetSelectionText(GYOBJ ev, char* out, size_t out_cap);

//---- 编辑动作(菜单/快捷键共用;不依赖焦点态,可从菜单直接调用)----
//撤销(单级;连调在两态间切换)
void   YMGUI_EditView_Undo(GYOBJ ev);
//复制选区到剪贴板(无选区无操作)
void   YMGUI_EditView_Copy(GYOBJ ev);
//剪切选区到剪贴板并删除(无选区无操作)
void   YMGUI_EditView_Cut(GYOBJ ev);
//粘贴剪贴板文本到光标处(有选区先删)
void   YMGUI_EditView_Paste(GYOBJ ev);

//---- 光标行列(1 基,供状态栏显示)----
//光标所在逻辑行号(按 '\n' 计,1 基)与列号(行内第几个码点,1 基)。可为 NULL
void   YMGUI_EditView_GetCursorRowCol(GYOBJ ev, size_t* row, size_t* col);

//---- 查找 / 替换 ----
//设置当前查找词(用于匹配高亮;NULL/"" 清除高亮)。不移动光标
void   YMGUI_EditView_SetFindNeedle(GYOBJ ev, const char* needle);
//从光标处向后查找 needle,命中则选中该匹配并滚入可见区,返回 1;未命中(含回卷)返回 0
uint8  YMGUI_EditView_FindNext(GYOBJ ev, const char* needle);
//向前查找(同上,反向)
uint8  YMGUI_EditView_FindPrev(GYOBJ ev, const char* needle);
//若当前选区正好等于 needle 则替换为 repl 并查找下一个,返回是否发生替换
uint8  YMGUI_EditView_Replace(GYOBJ ev, const char* needle, const char* repl);
//替换全部 needle 为 repl,返回替换次数
size_t YMGUI_EditView_ReplaceAll(GYOBJ ev, const char* needle, const char* repl);

#endif // !YMGUI_EDITVIEW_H
