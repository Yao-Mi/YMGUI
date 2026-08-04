#ifndef YMGUI_TREEVIEW_H
#define YMGUI_TREEVIEW_H

#include "YMGUI_PubType.h"
#include "YMGUI_PubDefine.h" //YMGUI_TREEVIEW
#include "YMGUI_Obj.h"

#if YMGUI_TREEVIEW

//===========================================================================
// 树形视图(TreeView):可展开/收起的层级列表(文件树式)。
//   自绘型控件(单 draw_cb 画全部可见行,type 仍是 GY_OBJ_Base,靠 user_data+draw_cb
//   区分),内部持有一棵"节点树"(name/是否目录/是否展开/子链表)+ 一份"拍平的可见行
//   数组"(只含祖先全部展开的节点)。展开/收起时重建可见数组,绘制时只画视口内可见行
//   (复用 Table 的裁剪 + 拖动滚动语义)。
//
//   每行 = 缩进(depth × 层缩进宽) + 展开标记(目录画 ▶/▼ 小三角,文件不画) + 名称。
//   交互:单击行选中(触发 select_cb);点击标记区或双击目录行切换展开;双击文件行触发
//   activate_cb(供上层"打开文件"用)。
//
//   懒加载:目录节点首次展开且未 loaded 时触发 expand_cb,让上层填充其子节点
//   (文件管理器里 = opendir/readdir)。收起只隐藏不释放已加载子节点。
//===========================================================================

//节点句柄(不透明;用访问器读写)
typedef struct GYtree_node GYtree_node;
typedef GYtree_node* GYTREENODE;

//目录节点首次展开的回调:上层在此填充 node 的子节点(懒加载)
typedef void (*GYtree_expand_cb)(GYOBJ tree, GYTREENODE node);
//行选中回调(单击某行):node 为被选中节点
typedef void (*GYtree_select_cb)(GYOBJ tree, GYTREENODE node);
//行激活回调(双击文件行 / 对文件回车):node 为被激活节点
typedef void (*GYtree_activate_cb)(GYOBJ tree, GYTREENODE node);

//---- 生命周期 ----
//创建树形视图(可视区尺寸 w x h)。初始空树
GYOBJ YMGUI_Creat_TreeView_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);

//---- 节点增删 ----
//加一个节点:parent_node=NULL 表示加到根层级;name 深拷一份;is_dir 非 0 为目录(可展开)。
//返回新节点句柄(失败返回 NULL)。加节点后重建可见数组 + 标脏
GYTREENODE YMGUI_TreeView_AddNode(GYOBJ tree, GYTREENODE parent_node, const char* name, uint8 is_dir);
//清空某节点的全部子节点(级联释放),并把该节点标回"未加载"(供刷新/懒加载重填)。
//node=NULL 表示清空根层级。重建可见数组 + 标脏
void YMGUI_TreeView_ClearChildren(GYOBJ tree, GYTREENODE node);
//清空整棵树(等价 ClearChildren(NULL))
void YMGUI_TreeView_Clear(GYOBJ tree);

//---- 展开/收起 ----
//设某目录节点展开态(非目录忽略)。展开且未加载时触发 expand_cb。重建可见数组 + 标脏
void YMGUI_TreeView_SetExpanded(GYOBJ tree, GYTREENODE node, uint8 expanded);
uint8 YMGUI_TreeView_IsExpanded(GYTREENODE node);

//---- 节点访问器 ----
const char* YMGUI_TreeView_NodeName(GYTREENODE node);      //节点名(内部副本指针)
uint8       YMGUI_TreeView_NodeIsDir(GYTREENODE node);     //是否目录
GYTREENODE  YMGUI_TreeView_NodeParent(GYTREENODE node);    //父节点(根层级节点返回 NULL)
void*       YMGUI_TreeView_NodeUserPtr(GYTREENODE node);   //上层挂的自定义数据
void        YMGUI_TreeView_SetNodeUserPtr(GYTREENODE node, void* p);

//---- 选中 ----
GYTREENODE YMGUI_TreeView_GetSelectedNode(GYOBJ tree);     //当前选中节点(无返回 NULL)
void       YMGUI_TreeView_SetSelectedNode(GYOBJ tree, GYTREENODE node);//设选中(不触发回调),标脏

//---- 回调 ----
void YMGUI_TreeView_SetExpandCb(GYOBJ tree, GYtree_expand_cb cb);
void YMGUI_TreeView_SetSelectCb(GYOBJ tree, GYtree_select_cb cb);
void YMGUI_TreeView_SetActivateCb(GYOBJ tree, GYtree_activate_cb cb);

//---- 外观 ----
void YMGUI_TreeView_SetRowHeight(GYOBJ tree, GYcoord row_h);//行高(<=0 保持,默认字体高+6)
void YMGUI_TreeView_SetIndent(GYOBJ tree, GYcoord indent);  //每层缩进像素(<=0 保持,默认 16)

//---- 滚动 ----
void    YMGUI_TreeView_SetScroll(GYOBJ tree, GYcoord scroll_y);//钳到 [0, 内容高-视口高],标脏
GYcoord YMGUI_TreeView_GetScroll(GYOBJ tree);
uint16  YMGUI_TreeView_GetVisibleCount(GYOBJ tree);        //当前可见行数(展开后拍平的行数)

#endif // YMGUI_TREEVIEW

#endif // !YMGUI_TREEVIEW_H
