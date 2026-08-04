#include "YMGUI_TreeView.h"

#if YMGUI_TREEVIEW

#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_TreeView.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-03
  *	@Description: 树形视图(文件树式)。自绘型:内部持一棵节点树 + 一份"拍平的可见行数组"
  *	              (只含祖先全部展开的节点),展开/收起时重建可见数组,绘制时只画视口内可见行。
  *	              复用 Table 的裁剪 / 拖动滚动 / 只画可见行;每层缩进 + 目录画三角展开标记。
  *	              目录首次展开触发 expand_cb 懒加载(文件管理器里 = opendir/readdir)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.节点树用链表(child_head/sibling,尾插 O(1)),无需 realloc;可见数组是指针数组(realloc 扩容)。
  * 2.拖动滚动带阈值:移动超阈值判滚动 → 抬起不算行点击(同 Table)。
  * 3.收起只从可见数组摘除,不释放已加载子节点(再展开无需重新懒加载)。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define TV_NAME_MAX  64 //节点名上限(含 '\0')
#define DRAG_THRESH  4  //拖动超此像素则本次不算行点击(判为滚动)
#define TV_MARK_SZ   8  //三角展开标记边长(像素)

//节点(链表树)
struct GYtree_node
{
	char   name[TV_NAME_MAX];
	uint8  is_dir;     //是否目录(可展开)
	uint8  expanded;   //是否展开
	uint8  loaded;     //目录子节点是否已加载(懒加载标记)
	uint16 depth;      //层级深度(根层级=0),缓存供绘制缩进
	struct GYtree_node* parent;     //父(根层级节点为 NULL)
	struct GYtree_node* child_head; //子链表头
	struct GYtree_node* child_tail; //子链表尾(尾插 O(1))
	struct GYtree_node* sibling;    //下一兄弟
	void*  user_ptr;   //上层自定义数据
};

//树形视图私有数据
typedef struct
{
	GYtree_node*  root_head;   //根层级子链表头
	GYtree_node*  root_tail;   //根层级子链表尾
	GYtree_node** vis;         //拍平的可见行数组(指针,realloc 扩容)
	uint16        vis_count;   //可见行数
	uint16        vis_cap;     //可见数组容量
	GYtree_node*  selected;    //选中节点(NULL 无)
	GYcoord       row_h;       //行高
	GYcoord       indent;      //每层缩进像素
	GYcoord       scroll_y;    //滚动偏移
	//拖动状态
	GYcoord       drag_start_y, drag_start_scr, drag_moved;
	//回调
	GYtree_expand_cb   expand_cb;
	GYtree_select_cb   select_cb;
	GYtree_activate_cb activate_cb;
	//配色
	GYcolor bg, sel_bg, txt, dir_txt, mark;
}GYtree_data;

//---- 节点基本操作 ----

/**
  * @brief 递归释放一棵子树(节点自身 + 所有后代)
  */
static void freeSubtree(GYtree_node* n)
{
	GYtree_node* c = n->child_head;
	while (c != NULL)
	{
		GYtree_node* next = c->sibling;
		freeSubtree(c);
		c = next;
	}
	GY_free0(n);
}

/**
  * @brief 释放某节点下的全部子节点(不释放该节点本身)
  */
static void freeChildren(GYtree_node* n)
{
	GYtree_node* c = n->child_head;
	while (c != NULL)
	{
		GYtree_node* next = c->sibling;
		freeSubtree(c);
		c = next;
	}
	n->child_head = NULL;
	n->child_tail = NULL;
}

/**
  * @brief 向可见数组追加一个节点(容量不足则扩容;失败静默丢弃该行)
  */
static void visPush(GYtree_data* d, GYtree_node* n)
{
	if (d->vis_count >= d->vis_cap)
	{
		uint16 ncap = (d->vis_cap == 0) ? 16 : (uint16)(d->vis_cap * 2);
		GYtree_node** nv = (GYtree_node**)GY_malloc0((uint32)ncap * sizeof(GYtree_node*));
		if (nv == NULL)
			return;//扩容失败:保留已有可见行,丢弃后续(不崩)
		for (uint16 i = 0; i < d->vis_count; i++)
			nv[i] = d->vis[i];
		if (d->vis != NULL)
			GY_free0(d->vis);
		d->vis = nv;
		d->vis_cap = ncap;
	}
	d->vis[d->vis_count++] = n;
}

/**
  * @brief 深度优先把"祖先全展开"的节点收进可见数组
  */
static void collectVisible(GYtree_data* d, GYtree_node* head)
{
	for (GYtree_node* n = head; n != NULL; n = n->sibling)
	{
		visPush(d, n);
		if (n->is_dir && n->expanded && n->child_head != NULL)
			collectVisible(d, n->child_head);
	}
}

/**
  * @brief 重建可见行数组(展开/收起/增删节点后调)
  */
static void rebuildVisible(GYOBJ tree)
{
	GYtree_data* d = (GYtree_data*)tree->user_data;
	d->vis_count = 0;
	collectVisible(d, d->root_head);
}

/**
  * @brief 内容总高 = 可见行数 × 行高
  */
static GYcoord contentH(GYtree_data* d)
{
	return (GYcoord)((int32)d->vis_count * d->row_h);
}

/**
  * @brief scroll_y 钳到 [0, max(0, 内容高 - 视口高)]
  */
static void clampScroll(GYOBJ tree)
{
	GYtree_data* d = (GYtree_data*)tree->user_data;
	GYcoord maxs = contentH(d) - tree->area.h;
	if (maxs < 0)
		maxs = 0;
	d->scroll_y = GYLimitMaxMin(0, d->scroll_y, maxs);
}

//---- 绘制 ----

/**
  * @brief 画展开三角:collapsed=▶(指右),expanded=▼(指下)。逐扫描线填充(只有线/矩形图元)
  * @param cx,cy 三角外接方块左上角(屏幕坐标),边长 TV_MARK_SZ
  */
static void drawTriangle(GYSURFACE s, GYcoord cx, GYcoord cy, uint8 expanded, GYcolor color)
{
	if (!expanded)
	{
		//▶ 指右:第 i 行(0..SZ-1)横向从 x=cx 画到 cx+i(顶宽底尖?)——用对称收敛成箭头
		//逐行:上半展宽、下半收窄,得到指向右的实心三角
		for (GYcoord i = 0; i < TV_MARK_SZ; i++)
		{
			//到中线距离 → 该行右端延伸量(中线最长,顶/底最短)
			GYcoord dist = (i < TV_MARK_SZ / 2) ? i : (GYcoord)(TV_MARK_SZ - 1 - i);
			GYcoord x2 = cx + dist;
			YMGUI_Draw_Line(s, cx, cy + i, x2, cy + i, color);
		}
	}
	else
	{
		//▼ 指下:第 i 列(0..SZ-1)纵向;中列最长,左右收窄,尖朝下
		for (GYcoord i = 0; i < TV_MARK_SZ; i++)
		{
			GYcoord dist = (i < TV_MARK_SZ / 2) ? i : (GYcoord)(TV_MARK_SZ - 1 - i);
			GYcoord y2 = cy + dist;
			YMGUI_Draw_Line(s, cx + i, cy, cx + i, y2, color);
		}
	}
}

/**
  * @brief 树绘制:背景 → 只画可见行(缩进 + 目录三角标记 + 名称;选中行高亮)
  */
static void treeDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYtree_data* d = (GYtree_data*)obj->user_data;
	GYFONT font = &YMGUI_Font_Default;

	//整体裁到自身区域(自绘型控件手动做 ClipChildren 的事)
	GYrect self_clip;
	if (!GY_Rect_Intersect(&self_clip, abs, &s->clip))
		return;
	GYrect saved_clip = s->clip;
	s->clip = self_clip;

	//背景
	YMGUI_Draw_Fill(s, abs, d->bg, GY_OPA_COVER);

	//只画可见行:首个可见行下标 = scroll_y / row_h
	uint16 first = (d->row_h > 0) ? (uint16)(d->scroll_y / d->row_h) : 0;
	for (uint16 ri = first; ri < d->vis_count; ri++)
	{
		GYcoord ry = abs->y + (GYcoord)((int32)ri * d->row_h - d->scroll_y);
		if (ry >= abs->y + abs->h)
			break;//滚出下沿
		GYtree_node* n = d->vis[ri];

		//选中行高亮(整行宽)
		if (n == d->selected)
		{
			GYrect selr = {abs->x, ry, abs->w, d->row_h};
			YMGUI_Draw_Fill(s, &selr, d->sel_bg, GY_OPA_COVER);
		}

		GYcoord indent_x = abs->x + 4 + (GYcoord)((int32)n->depth * d->indent);
		//目录三角标记(在缩进处,行内垂直居中)
		if (n->is_dir)
		{
			GYcoord my = ry + (d->row_h - TV_MARK_SZ) / 2;
			drawTriangle(s, indent_x, my, n->expanded, d->mark);
		}
		//名称(标记右侧留一个标记宽的空位,文件也对齐到该位置)
		GYcoord tx = indent_x + TV_MARK_SZ + 6;
		GYcoord ty = ry + ((d->row_h > font->cell_h) ? (d->row_h - font->cell_h) / 2 : 0);
		GYcolor col = n->is_dir ? d->dir_txt : d->txt;
		//名称各自裁到行内(防长名越界到下一行区域外——纵向已由 self_clip 保证)
		YMGUI_Draw_Text(s, font, tx, ty, n->name, col);
	}

	s->clip = saved_clip;
}

static void treeFreeCb(GYOBJ obj)
{
	GYtree_data* d = (GYtree_data*)obj->user_data;
	if (d != NULL)
	{
		GYtree_node* c = d->root_head;
		while (c != NULL)
		{
			GYtree_node* next = c->sibling;
			freeSubtree(c);
			c = next;
		}
		if (d->vis != NULL)
			GY_free0(d->vis);
		GY_free0(d);
		obj->user_data = NULL;
	}
}

/**
  * @brief 切换/设置某节点展开态:展开且未加载先触发懒加载,再重建可见数组
  */
static void doSetExpanded(GYOBJ tree, GYtree_node* n, uint8 expanded)
{
	if (n == NULL || !n->is_dir)
		return;
	GYtree_data* d = (GYtree_data*)tree->user_data;
	n->expanded = expanded ? 1 : 0;
	if (n->expanded && !n->loaded)
	{
		n->loaded = 1;//先置位:回调里 AddNode 走的是"已加载"路径,避免重入再触发
		if (d->expand_cb != NULL)
			d->expand_cb(tree, n);
	}
	rebuildVisible(tree);
	clampScroll(tree);
	YMGUI_Obj_Invalidate(tree);
}

/**
  * @brief 指针 y(屏幕)落在第几个可见行;不在内容/越界返回 -1
  */
static int32 rowAtPointer(GYOBJ tree, GYtree_data* d, GYcoord py)
{
	GYrect abs;
	YMGUI_Obj_GetAbsArea(tree, &abs);
	if (py < abs.y || py >= abs.y + abs.h)
		return -1;
	if (d->row_h <= 0)
		return -1;
	int32 ri = (int32)(((int32)(py - abs.y) + d->scroll_y) / d->row_h);
	if (ri < 0 || ri >= (int32)d->vis_count)
		return -1;
	return ri;
}

/**
  * @brief 判断某行内指针 x 是否落在"展开三角标记"区域(用于:点标记只切展开不选中)
  */
static uint8 hitMarker(GYOBJ tree, GYtree_data* d, GYtree_node* n, GYcoord px)
{
	if (!n->is_dir)
		return 0;
	GYrect abs;
	YMGUI_Obj_GetAbsArea(tree, &abs);
	GYcoord indent_x = abs.x + 4 + (GYcoord)((int32)n->depth * d->indent);
	//标记命中区放宽一点(标记宽 + 左右各 3px),好点
	return (px >= indent_x - 3 && px < indent_x + TV_MARK_SZ + 3) ? 1 : 0;
}

/**
  * @brief 树事件:按下记锚点;按住移动改 scroll(累计位移);
  *        单击(未超阈值)→ 点标记切展开,否则选中并回调;双击 → 目录切展开/文件激活
  */
static void treeEventCb(GYOBJ obj, GYEvent e)
{
	GYtree_data* d = (GYtree_data*)obj->user_data;
	GYcoord px = obj->ctx->point_x;
	GYcoord py = obj->ctx->point_y;
	switch (e)
	{
	case GY_EVENT_Pressed:
		d->drag_start_y = py;
		d->drag_start_scr = d->scroll_y;
		d->drag_moved = 0;
		break;
	case GY_EVENT_Pressing:
	{
		GYcoord delta = (GYcoord)(d->drag_start_y - py);
		GYcoord ad = (delta < 0) ? (GYcoord)(-delta) : delta;
		if (ad > d->drag_moved)
			d->drag_moved = ad;
		d->scroll_y = d->drag_start_scr + delta;
		clampScroll(obj);
		YMGUI_Obj_Invalidate(obj);
		break;
	}
	case GY_EVENT_Clicked:
		if (d->drag_moved <= DRAG_THRESH)
		{
			int32 ri = rowAtPointer(obj, d, py);
			if (ri >= 0)
			{
				GYtree_node* n = d->vis[(uint16)ri];
				if (hitMarker(obj, d, n, px))
				{
					doSetExpanded(obj, n, (uint8)!n->expanded);//点三角:只切展开
				}
				else
				{
					d->selected = n;//选中
					YMGUI_Obj_Invalidate(obj);
					if (d->select_cb != NULL)
						d->select_cb(obj, n);
				}
			}
		}
		break;
	case GY_EVENT_DoubleClicked:
		if (d->drag_moved <= DRAG_THRESH)
		{
			int32 ri = rowAtPointer(obj, d, py);
			if (ri >= 0)
			{
				GYtree_node* n = d->vis[(uint16)ri];
				d->selected = n;
				if (n->is_dir)
					doSetExpanded(obj, n, (uint8)!n->expanded);//双击目录:切展开
				else if (d->activate_cb != NULL)
					d->activate_cb(obj, n);//双击文件:激活(打开)
			}
		}
		break;
	default:
		break;
	}
}

//---- 公共 API ----

GYOBJ YMGUI_Creat_TreeView_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ tree = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(tree);
	if (tree == NULL)
		return NULL;
	GYtree_data* d = (GYtree_data*)GY_malloc0(sizeof(GYtree_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "树形视图数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(tree); return NULL; }
	d->root_head = NULL;
	d->root_tail = NULL;
	d->vis = NULL;
	d->vis_count = 0;
	d->vis_cap = 0;
	d->selected = NULL;
	d->row_h = (GYcoord)(YMGUI_Font_Default.cell_h + 6);
	d->indent = 16;
	d->scroll_y = 0;
	d->drag_moved = 0;
	d->expand_cb = NULL;
	d->select_cb = NULL;
	d->activate_cb = NULL;
	d->bg      = GY_ARGB(0xFF, 0x1C, 0x1C, 0x24);
	d->sel_bg  = GY_ARGB(0xFF, 0x35, 0x5A, 0x8A);
	d->txt     = GY_ARGB(0xFF, 0xE8, 0xE8, 0xE8);
	d->dir_txt = GY_ARGB(0xFF, 0xF0, 0xC8, 0x60);//目录名暖黄
	d->mark    = GY_ARGB(0xFF, 0xB0, 0xB0, 0xC0);

	tree->type = GY_OBJ_Base;
	tree->user_data = d;
	tree->draw_cb = treeDrawCb;
	tree->event_cb = treeEventCb;
	tree->free_cb = treeFreeCb;
	tree->bg_color = d->bg;
	YMGUI_Obj_Invalidate(tree);
	return tree;
}

GYTREENODE YMGUI_TreeView_AddNode(GYOBJ tree, GYTREENODE parent_node, const char* name, uint8 is_dir)
{
	gy_assert(tree && tree->user_data && name);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL) || (name == NULL), GY_LOG_PtrI, "树/数据/名称不存在");
	GYtree_data* d = (GYtree_data*)tree->user_data;

	GYtree_node* n = (GYtree_node*)GY_malloc0(sizeof(GYtree_node));
	gy_assert(n);
	gy_log_explain(n == NULL, GY_LOG_Mem0, "树节点内存申请失败");
	if (n == NULL)
		return NULL;
	uint16 i = 0;
	while (name[i] != '\0' && i < TV_NAME_MAX - 1) { n->name[i] = name[i]; i++; }
	n->name[i] = '\0';
	n->is_dir = is_dir ? 1 : 0;
	n->expanded = 0;
	n->loaded = 0;
	n->parent = parent_node;
	n->child_head = NULL;
	n->child_tail = NULL;
	n->sibling = NULL;
	n->user_ptr = NULL;
	n->depth = (parent_node != NULL) ? (uint16)(parent_node->depth + 1) : 0;

	//尾插到 parent_node 的子链(或根层级)
	if (parent_node != NULL)
	{
		if (parent_node->child_tail == NULL)
			parent_node->child_head = n;
		else
			parent_node->child_tail->sibling = n;
		parent_node->child_tail = n;
	}
	else
	{
		if (d->root_tail == NULL)
			d->root_head = n;
		else
			d->root_tail->sibling = n;
		d->root_tail = n;
	}

	rebuildVisible(tree);
	clampScroll(tree);
	YMGUI_Obj_Invalidate(tree);
	return n;
}

void YMGUI_TreeView_ClearChildren(GYOBJ tree, GYTREENODE node)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	GYtree_data* d = (GYtree_data*)tree->user_data;

	if (node != NULL)
	{
		freeChildren(node);
		node->loaded = 0;
	}
	else
	{
		GYtree_node* c = d->root_head;
		while (c != NULL)
		{
			GYtree_node* next = c->sibling;
			freeSubtree(c);
			c = next;
		}
		d->root_head = NULL;
		d->root_tail = NULL;
	}
	//选中节点可能已被释放:重建可见数组后若 selected 不在其中则清掉
	d->selected = NULL;//保守:清子树后一律清选中(选中失去意义,避免悬空)
	rebuildVisible(tree);
	clampScroll(tree);
	YMGUI_Obj_Invalidate(tree);
}

void YMGUI_TreeView_Clear(GYOBJ tree)
{
	YMGUI_TreeView_ClearChildren(tree, NULL);
}

void YMGUI_TreeView_SetExpanded(GYOBJ tree, GYTREENODE node, uint8 expanded)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	doSetExpanded(tree, node, expanded);
}

uint8 YMGUI_TreeView_IsExpanded(GYTREENODE node)
{
	return (node != NULL) ? node->expanded : 0;
}

const char* YMGUI_TreeView_NodeName(GYTREENODE node)
{
	return (node != NULL) ? node->name : "";
}

uint8 YMGUI_TreeView_NodeIsDir(GYTREENODE node)
{
	return (node != NULL) ? node->is_dir : 0;
}

GYTREENODE YMGUI_TreeView_NodeParent(GYTREENODE node)
{
	return (node != NULL) ? node->parent : NULL;
}

void* YMGUI_TreeView_NodeUserPtr(GYTREENODE node)
{
	return (node != NULL) ? node->user_ptr : NULL;
}

void YMGUI_TreeView_SetNodeUserPtr(GYTREENODE node, void* p)
{
	if (node != NULL)
		node->user_ptr = p;
}

GYTREENODE YMGUI_TreeView_GetSelectedNode(GYOBJ tree)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	return ((GYtree_data*)tree->user_data)->selected;
}

void YMGUI_TreeView_SetSelectedNode(GYOBJ tree, GYTREENODE node)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	((GYtree_data*)tree->user_data)->selected = node;
	YMGUI_Obj_Invalidate(tree);
}

void YMGUI_TreeView_SetExpandCb(GYOBJ tree, GYtree_expand_cb cb)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	((GYtree_data*)tree->user_data)->expand_cb = cb;
}

void YMGUI_TreeView_SetSelectCb(GYOBJ tree, GYtree_select_cb cb)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	((GYtree_data*)tree->user_data)->select_cb = cb;
}

void YMGUI_TreeView_SetActivateCb(GYOBJ tree, GYtree_activate_cb cb)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	((GYtree_data*)tree->user_data)->activate_cb = cb;
}

void YMGUI_TreeView_SetRowHeight(GYOBJ tree, GYcoord row_h)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	GYtree_data* d = (GYtree_data*)tree->user_data;
	if (row_h > 0)
		d->row_h = row_h;
	clampScroll(tree);
	YMGUI_Obj_Invalidate(tree);
}

void YMGUI_TreeView_SetIndent(GYOBJ tree, GYcoord indent)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	if (indent > 0)
		((GYtree_data*)tree->user_data)->indent = indent;
	YMGUI_Obj_Invalidate(tree);
}

void YMGUI_TreeView_SetScroll(GYOBJ tree, GYcoord scroll_y)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	((GYtree_data*)tree->user_data)->scroll_y = scroll_y;
	clampScroll(tree);
	YMGUI_Obj_Invalidate(tree);
}

GYcoord YMGUI_TreeView_GetScroll(GYOBJ tree)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	return ((GYtree_data*)tree->user_data)->scroll_y;
}

uint16 YMGUI_TreeView_GetVisibleCount(GYOBJ tree)
{
	gy_assert(tree && tree->user_data);
	gy_log_explain((tree == NULL) || (tree->user_data == NULL), GY_LOG_PtrI, "树或数据不存在");
	return ((GYtree_data*)tree->user_data)->vis_count;
}

#endif // YMGUI_TREEVIEW
