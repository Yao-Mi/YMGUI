#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_TreeView.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    demo_treeview.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-03
  *	@Description: 树形视图 demo:单击文件夹标记(▶/▼)就地展开/收起,首次展开触发懒加载填子节点;
  *	              单击选中(下方状态栏显路径),双击目录=切展开、双击文件="打开"(状态栏提示)。
  *	              拖动即纵向滚动。目录名暖黄、文件浅灰。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 360
#define SCR_H 300
#define BAND_H 60

static GYOBJ g_tree;
static GYOBJ g_status;

/* 懒加载:首次展开某个目录时,按其名字填入一层虚拟子节点(模拟 readdir)。
   真实文件管理器在这里换成 opendir/readdir/stat —— 见 project_Demo/files_manager。 */
static void onExpand(GYOBJ tree, GYTREENODE node)
{
	const char* name = YMGUI_TreeView_NodeName(node);

	if (strcmp(name, "src") == 0)
	{
		YMGUI_TreeView_AddNode(tree, node, "main.c", 0);
		YMGUI_TreeView_AddNode(tree, node, "util.c", 0);
		YMGUI_TreeView_AddNode(tree, node, "util.h", 0);
		GYTREENODE nested = YMGUI_TreeView_AddNode(tree, node, "core", 1);
		(void)nested; /* 展开 core 时会再次进本回调 */
	}
	else if (strcmp(name, "core") == 0)
	{
		YMGUI_TreeView_AddNode(tree, node, "engine.c", 0);
		YMGUI_TreeView_AddNode(tree, node, "engine.h", 0);
	}
	else if (strcmp(name, "docs") == 0)
	{
		YMGUI_TreeView_AddNode(tree, node, "guide.md", 0);
		YMGUI_TreeView_AddNode(tree, node, "api.md", 0);
	}
	else if (strcmp(name, "empty") == 0)
	{
		/* 空目录:展开后无子节点,标记从 ▶ 变 ▼ 但不长出行 */
	}
}

static void onSelect(GYOBJ tree, GYTREENODE node)
{
	char buf[128];
	const char* name = YMGUI_TreeView_NodeName(node);
	(void)tree;
	snprintf(buf, sizeof(buf), "Selected: %s%s",
	         name, YMGUI_TreeView_NodeIsDir(node) ? "/" : "");
	YMGUI_Label_SetText(g_status, buf);
}

static void onActivate(GYOBJ tree, GYTREENODE node)
{
	char buf[128];
	const char* name = YMGUI_TreeView_NodeName(node);
	(void)tree;
	/* 目录的双击已被控件转成切展开;这里只会收到文件的双击 */
	snprintf(buf, sizeof(buf), "Open file: %s", name);
	YMGUI_Label_SetText(g_status, buf);
}

int main(int argc, char** argv)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	int max_frames = (argc > 1) ? atoi(argv[1]) : -1;
	int frame = 0;

	disp.hor_res = SCR_W; disp.ver_res = SCR_H; disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.user_data = NULL;

	SDL_LCD_Init(&disp, 2);
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));

	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 8, 6, SCR_W - 16, 16);
	YMGUI_Label_SetText(title, "TreeView: click arrow to expand, drag to scroll");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));

	g_tree = YMGUI_Creat_TreeView_Creat(ctx->root, 8, 28, SCR_W - 16, SCR_H - 28 - 26);
	YMGUI_TreeView_SetExpandCb(g_tree, onExpand);
	YMGUI_TreeView_SetSelectCb(g_tree, onSelect);
	YMGUI_TreeView_SetActivateCb(g_tree, onActivate);

	/* 顶层:两个目录(可懒加载)+ 两个文件 + 一个空目录 */
	GYTREENODE src  = YMGUI_TreeView_AddNode(g_tree, NULL, "src", 1);
	YMGUI_TreeView_AddNode(g_tree, NULL, "docs", 1);
	YMGUI_TreeView_AddNode(g_tree, NULL, "empty", 1);
	YMGUI_TreeView_AddNode(g_tree, NULL, "README.md", 0);
	YMGUI_TreeView_AddNode(g_tree, NULL, "LICENSE", 0);

	/* 默认把 src 展开一层,让首屏就有层级观感 */
	YMGUI_TreeView_SetExpanded(g_tree, src, 1);

	g_status = YMGUI_Creat_Label_Creat(ctx->root, 8, SCR_H - 20, SCR_W - 16, 16);
	YMGUI_Label_SetText(g_status, "Selected: (none)");
	YMGUI_Label_SetTextColor(g_status, GY_ARGB(0xFF, 0xA0, 0xE0, 0xA0));

	YMGUI_Inject_SetCtx(ctx);

	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	gy_log_print("demo_treeview exit ok\n");
	return 0;
}
