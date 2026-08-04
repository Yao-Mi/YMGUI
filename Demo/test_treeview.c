#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_TreeView.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_treeview.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-03
  *	@Description: 树形视图单测:节点增删、展开/收起重建可见数组、懒加载回调只触发一次、
  *	              缩进 depth、点击选中+回调、点三角只切展开不选中、双击目录切展开/双击文件激活、
  *	              滚动钳制、ClearChildren 释放不漏、析构级联不崩。判成败以 exit code 为准。
  ***************************************************************************************************************************/

#define SCR_W 240
#define SCR_H 160
#define BAND_H 160

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static GYpx g_fb[SCR_W * SCR_H];
static void fbFlush(GYdisp* d, const GYrect* a, const GYpx* b)
{
	(void)d;
	for (GYcoord yy = 0; yy < a->h; yy++)
		for (GYcoord xx = 0; xx < a->w; xx++)
		{
			GYcoord sx = a->x + xx, sy = a->y + yy;
			if (sx >= 0 && sx < SCR_W && sy >= 0 && sy < SCR_H)
				g_fb[sy * SCR_W + sx] = b[yy * a->w + xx];
		}
}

//---- 懒加载回调:记录触发次数,给被展开的目录填几个子节点 ----
static int g_expand_calls = 0;
static GYTREENODE g_last_expanded = NULL;
static void onExpand(GYOBJ tree, GYTREENODE node)
{
	g_expand_calls++;
	g_last_expanded = node;
	//给该目录填两个子文件(模拟 readdir)
	YMGUI_TreeView_AddNode(tree, node, "child_a.txt", 0);
	YMGUI_TreeView_AddNode(tree, node, "child_b.txt", 0);
}

static int g_select_calls = 0;
static GYTREENODE g_selected = NULL;
static void onSelect(GYOBJ tree, GYTREENODE node) { (void)tree; g_select_calls++; g_selected = node; }

static int g_activate_calls = 0;
static GYTREENODE g_activated = NULL;
static void onActivate(GYOBJ tree, GYTREENODE node) { (void)tree; g_activate_calls++; g_activated = node; }

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = fbFlush; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0, 0, 0));
	YMGUI_Inject_SetCtx(ctx);

	//树 (10,10) 200x120;行高 20 → 视口 6 行可见
	GYOBJ tree = YMGUI_Creat_TreeView_Creat(ctx->root, 10, 10, 200, 120);
	YMGUI_TreeView_SetRowHeight(tree, 20);
	YMGUI_TreeView_SetExpandCb(tree, onExpand);
	YMGUI_TreeView_SetSelectCb(tree, onSelect);
	YMGUI_TreeView_SetActivateCb(tree, onActivate);

	//---- 节点增删 + 可见数组 ----
	GYTREENODE root_dir = YMGUI_TreeView_AddNode(tree, NULL, "src", 1);
	GYTREENODE file1    = YMGUI_TreeView_AddNode(tree, NULL, "readme.txt", 0);
	CHECK(root_dir != NULL && file1 != NULL, "AddNode 返回非空");
	CHECK(YMGUI_TreeView_GetVisibleCount(tree) == 2, "根层级 2 个可见");
	CHECK(YMGUI_TreeView_NodeIsDir(root_dir) == 1, "src 是目录");
	CHECK(YMGUI_TreeView_NodeIsDir(file1) == 0, "readme.txt 是文件");
	CHECK(strcmp(YMGUI_TreeView_NodeName(root_dir), "src") == 0, "节点名 src");
	CHECK(YMGUI_TreeView_NodeParent(root_dir) == NULL, "根层级 parent 为 NULL");

	//---- 手动加子节点(不经懒加载),深度递增 ----
	GYTREENODE sub = YMGUI_TreeView_AddNode(tree, root_dir, "sub", 1);
	CHECK(YMGUI_TreeView_NodeParent(sub) == root_dir, "sub 的 parent 是 src");
	//src 未展开,sub 不应出现在可见数组
	CHECK(YMGUI_TreeView_GetVisibleCount(tree) == 2, "src 收起时子节点不可见");

	//---- 展开:sub 出现,可见数变 3 ----
	YMGUI_TreeView_SetExpanded(tree, root_dir, 1);
	CHECK(YMGUI_TreeView_IsExpanded(root_dir) == 1, "src 已展开");
	//src 已有手动加的子节点 sub;但 loaded 首次由 SetExpanded 置位 → 触发一次懒加载又加了 a/b
	//故 src 子 = sub + child_a + child_b = 3;总可见 = src + 3 + readme = 5
	CHECK(g_expand_calls == 1, "懒加载回调触发一次");
	CHECK(g_last_expanded == root_dir, "懒加载的是 src");
	CHECK(YMGUI_TreeView_GetVisibleCount(tree) == 5, "展开后可见 5(src+sub+a+b+readme)");

	//---- 再收起再展开:不应重复触发懒加载 ----
	YMGUI_TreeView_SetExpanded(tree, root_dir, 0);
	CHECK(YMGUI_TreeView_GetVisibleCount(tree) == 2, "收起回到 2");
	YMGUI_TreeView_SetExpanded(tree, root_dir, 1);
	CHECK(g_expand_calls == 1, "再展开不重复懒加载");
	CHECK(YMGUI_TreeView_GetVisibleCount(tree) == 5, "再展开仍 5");

	//---- 对文件调 SetExpanded 应被忽略 ----
	YMGUI_TreeView_SetExpanded(tree, file1, 1);
	CHECK(YMGUI_TreeView_IsExpanded(file1) == 0, "文件不可展开");

	//---- 用户指针 ----
	int marker = 42;
	YMGUI_TreeView_SetNodeUserPtr(file1, &marker);
	CHECK(YMGUI_TreeView_NodeUserPtr(file1) == &marker, "user_ptr 存取");

	//---- 点击选中 + 回调:第 0 行是 src(目录),点其名称区(避开三角)应选中 ----
	//行 0 屏幕 y = 10..30;名称在缩进+标记右侧,x≈40 稳落在名称区
	YMGUI_Inject_Pointer(120, 20, 1);//按下
	YMGUI_Inject_Pointer(120, 20, 0);//抬起 → Clicked
	CHECK(g_select_calls == 1, "点击触发选中回调一次");
	CHECK(YMGUI_TreeView_GetSelectedNode(tree) == root_dir, "选中的是第 0 行 src");

	//---- 点三角只切展开不选中:src 当前展开,点其三角(x≈14)应收起 ----
	int sel_before = g_select_calls;
	YMGUI_Inject_Pointer(15, 20, 1);
	YMGUI_Inject_Pointer(15, 20, 0);
	CHECK(YMGUI_TreeView_IsExpanded(root_dir) == 0, "点三角收起了 src");
	CHECK(g_select_calls == sel_before, "点三角不触发选中回调");

	//---- 双击文件激活:重新展开 src,双击其子文件 child_a(第 2 行,y≈50)----
	YMGUI_TreeView_SetExpanded(tree, root_dir, 1);
	//可见:0=src 1=sub 2=child_a 3=child_b 4=readme;行 2 屏幕 y=10+2*20=50..70
	YMGUI_Inject_DoubleClick(120, 55);
	CHECK(g_activate_calls == 1, "双击文件触发激活一次");
	CHECK(g_activated != NULL && strcmp(YMGUI_TreeView_NodeName(g_activated), "child_a.txt") == 0, "激活的是 child_a.txt");

	//---- 双击目录切展开(不激活):sub 是行 1(y≈30..50),双击应展开 sub(触发懒加载 +2)----
	int act_before = g_activate_calls;
	int exp_before = g_expand_calls;
	YMGUI_Inject_DoubleClick(120, 35);
	CHECK(g_activate_calls == act_before, "双击目录不触发激活");
	CHECK(g_expand_calls == exp_before + 1, "双击目录首展开触发懒加载");
	CHECK(YMGUI_TreeView_IsExpanded(sub) == 1, "双击展开了 sub");

	//---- 滚动钳制:内容够高时 scroll 上下限 ----
	//造更多节点撑高内容
	for (int i = 0; i < 20; i++)
	{
		char nm[16];
		snprintf(nm, sizeof(nm), "f%d.txt", i);
		YMGUI_TreeView_AddNode(tree, NULL, nm, 0);
	}
	YMGUI_TreeView_SetScroll(tree, -50);
	CHECK(YMGUI_TreeView_GetScroll(tree) == 0, "scroll 下限钳 0");
	YMGUI_TreeView_SetScroll(tree, 30000);
	{
		GYcoord maxs = (GYcoord)((int32)YMGUI_TreeView_GetVisibleCount(tree) * 20 - 120);
		if (maxs < 0) maxs = 0;
		CHECK(YMGUI_TreeView_GetScroll(tree) == maxs, "scroll 上限钳到内容高-视口高");
	}

	//---- ClearChildren:清 src 的子节点,src 变未加载,可见数减少 ----
	uint16 before = YMGUI_TreeView_GetVisibleCount(tree);
	YMGUI_TreeView_ClearChildren(tree, root_dir);
	CHECK(YMGUI_TreeView_GetVisibleCount(tree) < before, "ClearChildren 后可见减少");
	CHECK(YMGUI_TreeView_GetSelectedNode(tree) == NULL, "ClearChildren 保守清选中");
	//清后再展开 src 应重新懒加载(loaded 被复位)
	int ec = g_expand_calls;
	YMGUI_TreeView_SetExpanded(tree, root_dir, 1);
	CHECK(g_expand_calls == ec + 1, "ClearChildren 后再展开重新懒加载");

	//---- 渲染一帧(不崩)+ Clear 全清 ----
	YMGUI_Refresh(ctx);
	YMGUI_TreeView_Clear(tree);
	CHECK(YMGUI_TreeView_GetVisibleCount(tree) == 0, "Clear 后可见 0");
	YMGUI_Refresh(ctx);

	//---- 析构级联不崩(还剩一个非空树的场景:重建再交给 CtxFree)----
	YMGUI_TreeView_AddNode(tree, NULL, "leftover_dir", 1);
	GYTREENODE lo = YMGUI_TreeView_GetSelectedNode(tree);//NULL
	(void)lo;

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_treeview: ALL PASS\n");
	else
		printf("test_treeview: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
