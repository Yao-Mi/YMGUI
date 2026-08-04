#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_Dropdown.h"
#include "YMGUI_EditView.h"
#include "YMGUI_Font.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    txt_edit.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-02
  *	@Description: project_Demo 第一个基础验证项目 —— 多行文本编辑器(v3:菜单栏版)。
  *	              800x600 窗口。顶部菜单栏用 4 个 Dropdown 当下拉菜单(文件/编辑/查找/视图),
  *	              选项 0 是菜单名(合起时常显),选项 1+ 是动作,选后跑动作再复位回菜单名。
  *	              文件=新建/清空;编辑=撤销/剪切/复制/粘贴/全选;查找=打开查找条;视图=切换折行/字数统计。
  *	              主体全屏 EditView 多行编辑(点击定位/双击选词/Shift 选区/Ctrl+C/X/V/Z/A/Home/End)。
  *	              Ctrl+F 弹可折叠查找/替换条;底部状态栏显示行列 + 选中字节 + 折行状态。
  *	              剪贴板走 HAL 回调缝(SDL 挂系统剪贴板);复用 Demo/ 下 GB2312 全字库外部 blob。
  *	@Version:     3.0
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 800
#define SCR_H 600
#define BAND_H 60

//GB2312 全字库回退:码点索引在固件(YMGUI_GB2312_cps),字形位图在外部 blob。
//没有 blob 时不挂 fallback,内置 CJK 只有 75 字,中文/标点会显示为占位空格。
#ifndef GB2312_BIN_PATH
#define GB2312_BIN_PATH "gb2312_glyphs.bin"
#endif
extern const uint16 YMGUI_GB2312_cps[];
extern const uint16 YMGUI_GB2312_glyph_count;
static FILE* s_blob = NULL;
//外部 flash 读回调:第 i 个字形在 off=i*128 处(16x16 4bpp = 128 字节)
static uint32 flashRead(const GYfont* font, uint32 off, uint32 len, uint8* buf)
{
	(void)font;
	if (s_blob == NULL) return 0;
	if (fseek(s_blob, (long)off, SEEK_SET) != 0) return 0;
	return (uint32)fread(buf, 1, len, s_blob);
}
//GYfont 位置初始化:bitmap,codepoints,glyph_count,first,last,cell_w,cell_h,bpr,bpp,fallback,glyph_read
static GYfont s_gb_font = { NULL, YMGUI_GB2312_cps, 0, 0, 0, 16, 16, 8, 4, NULL, flashRead };

static GYCTX  g_ctx;
static GYOBJ  g_edit;       //主编辑区(EditView,全屏)
static GYOBJ  g_status;     //状态栏(行:列 + 选中字节 + 折行状态)
static GYOBJ  g_find_in;    //查找输入框
static GYOBJ  g_repl_in;    //替换输入框
static GYOBJ  g_path_in;    //文件路径输入框(菜单栏右侧,供 打开/保存 用)
static GYOBJ  g_bar[8];     //查找/替换条上的对象(整条一起显隐)
static int    g_bar_cnt = 0;
static uint8  g_bar_open = 0;
static uint8  g_wrap = 0;    //视图:自动折行开关

//菜单里各下拉的选项下标(选项 0 恒为菜单名占位)
enum { FILE_TITLE = 0, FILE_NEW, FILE_OPEN, FILE_SAVE, FILE_CLEAR };
enum { EDIT_TITLE = 0, EDIT_UNDO, EDIT_CUT, EDIT_COPY, EDIT_PASTE, EDIT_SELALL };
enum { FIND_TITLE = 0, FIND_OPEN, FIND_CLOSE };
enum { VIEW_TITLE = 0, VIEW_WRAP, VIEW_COUNT };

//---- 查找/替换条:整条显隐 ----
static void barSetHidden(uint8 hidden)
{
	int i;
	for (i = 0; i < g_bar_cnt; i++)
		YMGUI_Obj_SetHidden(g_bar[i], hidden);
	g_bar_open = (uint8)!hidden;
}
static void barOpen(void)
{
	barSetHidden(0);
	YMGUI_EditView_SetFindNeedle(g_edit, YMGUI_TextInput_GetText(g_find_in));
	YMGUI_SetFocus(g_ctx, g_find_in);
}
static void barClose(void)
{
	barSetHidden(1);
	YMGUI_EditView_SetFindNeedle(g_edit, NULL);
	YMGUI_SetFocus(g_ctx, g_edit);
}

//Ctrl+F(EditView 转发):切换查找条
static void onFind(GYOBJ ev)
{
	(void)ev;
	if (g_bar_open) barClose();
	else            barOpen();
}

//查找框内容变更:实时刷新高亮(不移动光标)
static void onFindChanged(GYOBJ ti, const char* text)
{
	(void)ti;
	YMGUI_EditView_SetFindNeedle(g_edit, text);
}
static void onNext(GYOBJ btn)    { (void)btn; YMGUI_EditView_FindNext(g_edit, YMGUI_TextInput_GetText(g_find_in)); }
static void onPrev(GYOBJ btn)    { (void)btn; YMGUI_EditView_FindPrev(g_edit, YMGUI_TextInput_GetText(g_find_in)); }
static void onRepl(GYOBJ btn)    { (void)btn; YMGUI_EditView_Replace(g_edit, YMGUI_TextInput_GetText(g_find_in), YMGUI_TextInput_GetText(g_repl_in)); }
static void onReplAll(GYOBJ btn) { (void)btn; YMGUI_EditView_ReplaceAll(g_edit, YMGUI_TextInput_GetText(g_find_in), YMGUI_TextInput_GetText(g_repl_in)); }

//状态栏:每帧算一次,内容变了才 SetText(避免无谓标脏)
static void updateStatus(void)
{
	static char last[80] = {0};
	char buf[80];
	size_t row = 0, col = 0, s = 0, e = 0;
	YMGUI_EditView_GetCursorRowCol(g_edit, &row, &col);
	YMGUI_EditView_GetSelection(g_edit, &s, &e);
	const char* wrap = g_wrap ? "Wrap:on" : "Wrap:off";
	if (e > s)
		snprintf(buf, sizeof(buf), "Ln %u, Col %u  |  sel %u  |  %s", (unsigned)row, (unsigned)col, (unsigned)(e - s), wrap);
	else
		snprintf(buf, sizeof(buf), "Ln %u, Col %u  |  %s", (unsigned)row, (unsigned)col, wrap);
	if (strcmp(buf, last) != 0)
	{
		strncpy(last, buf, sizeof(last) - 1);
		YMGUI_Label_SetText(g_status, buf);
	}
}

//在状态栏临时报一条消息(下一帧光标动了会被 updateStatus 覆盖,够用)
static void statusMsg(const char* msg)
{
	YMGUI_Label_SetText(g_status, msg);
}

//======== 文件 打开/保存 ========
//路径取自菜单栏右侧路径框;读满一个文件缓冲(至多 GY_EV_TEXT_MAX-1 字节)灌进 EditView。
//缓冲设为 static:128KB 放栈上会爆,和 EditView 内部 buffer 同量级。
static char g_io_buf[GY_EV_TEXT_MAX];

static void fileOpen(void)
{
	const char* path = YMGUI_TextInput_GetText(g_path_in);
	char msg[96];
	FILE* f;
	size_t n;
	if (path == NULL || path[0] == '\0') { statusMsg("Open: set a path first"); return; }
	f = fopen(path, "rb");
	if (f == NULL) { snprintf(msg, sizeof(msg), "Open failed: %s", path); statusMsg(msg); return; }
	n = fread(g_io_buf, 1, sizeof(g_io_buf) - 1, f);
	g_io_buf[n] = '\0';
	//若文件比容量大,fread 只读了前一截;告知被截断
	{
		int truncated = (fgetc(f) != EOF);
		fclose(f);
		YMGUI_EditView_SetText(g_edit, g_io_buf);
		YMGUI_SetFocus(g_ctx, g_edit);
		if (truncated)
			snprintf(msg, sizeof(msg), "Opened (truncated to %u bytes): %s", (unsigned)n, path);
		else
			snprintf(msg, sizeof(msg), "Opened %u bytes: %s", (unsigned)n, path);
		statusMsg(msg);
	}
}

static void fileSave(void)
{
	const char* path = YMGUI_TextInput_GetText(g_path_in);
	const char* text = YMGUI_EditView_GetText(g_edit);
	char msg[96];
	FILE* f;
	size_t len, wrote;
	if (path == NULL || path[0] == '\0') { statusMsg("Save: set a path first"); return; }
	f = fopen(path, "wb");
	if (f == NULL) { snprintf(msg, sizeof(msg), "Save failed: %s", path); statusMsg(msg); return; }
	len = strlen(text);
	wrote = fwrite(text, 1, len, f);
	fclose(f);
	if (wrote != len)
		snprintf(msg, sizeof(msg), "Save incomplete (%u/%u): %s", (unsigned)wrote, (unsigned)len, path);
	else
		snprintf(msg, sizeof(msg), "Saved %u bytes: %s", (unsigned)len, path);
	statusMsg(msg);
	YMGUI_SetFocus(g_ctx, g_edit);
}

//======== 菜单动作回调:Dropdown 选中某项 → 跑动作 → 复位回菜单名(选项 0)========
static void onFileMenu(GYOBJ dd, uint16 sel)
{
	switch (sel)
	{
	case FILE_NEW:
	case FILE_CLEAR:
		YMGUI_EditView_SetText(g_edit, "");
		YMGUI_SetFocus(g_ctx, g_edit);
		statusMsg("File: new/clear done");
		break;
	case FILE_OPEN: fileOpen(); break;
	case FILE_SAVE: fileSave(); break;
	default: break;
	}
	YMGUI_Dropdown_SetSelected(dd, FILE_TITLE);//复位显示菜单名
}
static void onEditMenu(GYOBJ dd, uint16 sel)
{
	switch (sel)
	{
	case EDIT_UNDO:   YMGUI_EditView_Undo(g_edit);  break;
	case EDIT_CUT:    YMGUI_EditView_Cut(g_edit);   break;
	case EDIT_COPY:   YMGUI_EditView_Copy(g_edit);  break;
	case EDIT_PASTE:  YMGUI_EditView_Paste(g_edit); break;
	case EDIT_SELALL: YMGUI_EditView_SelectAll(g_edit); break;
	default: break;
	}
	YMGUI_SetFocus(g_ctx, g_edit);
	YMGUI_Dropdown_SetSelected(dd, EDIT_TITLE);
}
static void onFindMenu(GYOBJ dd, uint16 sel)
{
	switch (sel)
	{
	case FIND_OPEN:  barOpen();  break;
	case FIND_CLOSE: barClose(); break;
	default: break;
	}
	YMGUI_Dropdown_SetSelected(dd, FIND_TITLE);
}
static void onViewMenu(GYOBJ dd, uint16 sel)
{
	switch (sel)
	{
	case VIEW_WRAP:
		g_wrap = (uint8)!g_wrap;
		YMGUI_EditView_SetWrap(g_edit, g_wrap);
		statusMsg(g_wrap ? "View: wrap ON" : "View: wrap OFF");
		break;
	case VIEW_COUNT:
	{
		//字数统计:字节数(UTF-8 下中文占多字节,此处报字节 + 行数,够验证)
		char buf[80];
		const char* t = YMGUI_EditView_GetText(g_edit);
		unsigned bytes = (unsigned)strlen(t);
		unsigned lines = (unsigned)YMGUI_EditView_GetLineCount(g_edit);
		snprintf(buf, sizeof(buf), "Count: %u bytes, %u display-lines", bytes, lines);
		statusMsg(buf);
		break;
	}
	default: break;
	}
	YMGUI_Dropdown_SetSelected(dd, VIEW_TITLE);
}

//建一个菜单下拉:选项 0 = 菜单名(合起时常显),其余为动作。返回对象
static GYOBJ makeMenu(GYOBJ root, GYcoord x, GYcoord w, const char* title,
                      const char* const* items, int n, GYdropdown_sel_cb cb)
{
	int i;
	GYOBJ dd = YMGUI_Creat_Dropdown_Creat(root, x, 4, w, 24);
	YMGUI_Dropdown_AddOption(dd, title);//选项 0:菜单名
	for (i = 0; i < n; i++)
		YMGUI_Dropdown_AddOption(dd, items[i]);
	YMGUI_Dropdown_SetSelected(dd, 0);
	YMGUI_Dropdown_SetSelectedCb(dd, cb);
	return dd;
}

//顶部菜单栏:文件/编辑/查找/视图 四个下拉横排
static void buildMenuBar(GYOBJ root)
{
	static const char* const file_items[] = { "New", "Open", "Save", "Clear" };
	static const char* const edit_items[] = { "Undo", "Cut", "Copy", "Paste", "Select All" };
	static const char* const find_items[] = { "Find / Replace...", "Close find bar" };
	static const char* const view_items[] = { "Toggle Wrap", "Word Count" };
	makeMenu(root,   8, 90, "File", file_items, 4, onFileMenu);
	makeMenu(root, 104, 90, "Edit", edit_items, 5, onEditMenu);
	makeMenu(root, 200, 90, "Find", find_items, 2, onFindMenu);
	makeMenu(root, 296, 90, "View", view_items, 2, onViewMenu);
	//菜单栏右侧:文件路径框(供 File→Open/Save 用)。前置一个 "Path:" 标签。
	{
		GYOBJ lbl = YMGUI_Creat_Label_Creat(root, 396, 8, 44, 16);
		YMGUI_Label_SetText(lbl, "Path:");
		YMGUI_Label_SetTextColor(lbl, GY_ARGB(0xFF, 0xC0, 0xC0, 0xC8));
		g_path_in = YMGUI_Creat_TextInput_Creat(root, 440, 4, SCR_W - 440 - 8, 24);
		YMGUI_TextInput_SetText(g_path_in, "out.txt");
	}
}

//查找/替换条:两行——[查找框][Prev][Next] / [替换框][Repl][All]。整条登记进 g_bar 一起显隐
static void buildFindBar(GYOBJ root)
{
	GYOBJ b;
	GYcoord y1 = SCR_H - 78, y2 = SCR_H - 56;
	//第 1 行:查找
	g_find_in = YMGUI_Creat_TextInput_Creat(root, 8, y1, 360, 20);
	YMGUI_TextInput_SetChanged(g_find_in, onFindChanged);
	g_bar[g_bar_cnt++] = g_find_in;
	b = YMGUI_Creat_Button_Creat(root, 376, y1, 80, 20);
	YMGUI_Button_SetText(b, "Prev"); YMGUI_Button_SetClicked(b, onPrev); g_bar[g_bar_cnt++] = b;
	b = YMGUI_Creat_Button_Creat(root, 462, y1, 80, 20);
	YMGUI_Button_SetText(b, "Next"); YMGUI_Button_SetClicked(b, onNext); g_bar[g_bar_cnt++] = b;
	//第 2 行:替换
	g_repl_in = YMGUI_Creat_TextInput_Creat(root, 8, y2, 360, 20);
	g_bar[g_bar_cnt++] = g_repl_in;
	b = YMGUI_Creat_Button_Creat(root, 376, y2, 80, 20);
	YMGUI_Button_SetText(b, "Repl"); YMGUI_Button_SetClicked(b, onRepl); g_bar[g_bar_cnt++] = b;
	b = YMGUI_Creat_Button_Creat(root, 462, y2, 80, 20);
	YMGUI_Button_SetText(b, "All"); YMGUI_Button_SetClicked(b, onReplAll); g_bar[g_bar_cnt++] = b;
	barSetHidden(1);//默认收起
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

	SDL_LCD_Init(&disp, 1);//800x600 已够大,scale=1
	g_ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	//窗口底色用很暗的近黑,和编辑区底色(偏亮一档)拉开对比,一眼能看出编辑区范围
	YMGUI_Obj_SetBgColor(g_ctx->root, GY_ARGB(0xFF, 0x0E, 0x0E, 0x12));

	//挂 GB2312 全字库回退(有 blob 才挂),让编辑的任意中文/标点都能显示
	s_blob = fopen(GB2312_BIN_PATH, "rb");
	if (s_blob != NULL)
	{
		s_gb_font.glyph_count = YMGUI_GB2312_glyph_count;
		YMGUI_Font_SetFallback(&s_gb_font);
	}
	else
	{
		gy_log_print("warn: gb2312 blob not found, CJK limited to built-in glyphs\n");
	}

	buildMenuBar(g_ctx->root);

	//主编辑区:菜单栏(y<28)下方到查找条上方一大片。查找条收起时其下方是空背景
	g_edit = YMGUI_Creat_EditView_Creat(g_ctx->root, 8, 32, SCR_W - 16, SCR_H - 110, GY_EV_TEXT_MAX);//PC 编辑器:128K 容量
	YMGUI_EditView_SetBgColor(g_edit, GY_ARGB(0xFF, 0x24, 0x24, 0x30));   //编辑区偏亮一档
	YMGUI_EditView_SetBorderColor(g_edit, GY_ARGB(0xFF, 0x60, 0x60, 0x78));//浅灰边框勾出范围
	YMGUI_EditView_SetText(g_edit, "");
	YMGUI_EditView_SetFindCb(g_edit, onFind);

	buildFindBar(g_ctx->root);

	g_status = YMGUI_Creat_Label_Creat(g_ctx->root, 8, SCR_H - 20, SCR_W - 16, 16);
	YMGUI_Label_SetTextColor(g_status, GY_ARGB(0xFF, 0xA0, 0xE0, 0xA0));

	YMGUI_SetFocus(g_ctx, g_edit);
	YMGUI_Inject_SetCtx(g_ctx);
	updateStatus();

	//无头验证:预置多行文本 + 跑一遍菜单动作/查找替换/选区这条链不崩
	if (max_frames > 0)
	{
		YMGUI_EditView_SetText(g_edit,
			"hello world\nsecond line here\nthird: hello again\nfoo bar baz\n");
		YMGUI_EditView_FindNext(g_edit, "hello");
		YMGUI_EditView_Copy(g_edit);       //复制当前选中的匹配
		YMGUI_EditView_SelectAll(g_edit);
		YMGUI_EditView_ReplaceAll(g_edit, "hello", "HI");
		YMGUI_EditView_Undo(g_edit);       //撤销一次
		g_wrap = 1; YMGUI_EditView_SetWrap(g_edit, 1);//视图:折行
		barOpen(); barClose();             //查找条弹/收

		//文件 保存→清空→打开 往返:写盘再读回,验内容一致
		YMGUI_EditView_SetText(g_edit, "file io round trip\nline two\n");
		YMGUI_TextInput_SetText(g_path_in, "txt_edit_selftest.txt");
		fileSave();
		YMGUI_EditView_SetText(g_edit, "");
		fileOpen();
		if (strcmp(YMGUI_EditView_GetText(g_edit), "file io round trip\nline two\n") == 0)
			gy_log_print("selftest: file save/open round-trip OK\n");
		else
			gy_log_print("selftest: file save/open round-trip FAILED\n");
		remove("txt_edit_selftest.txt");

		updateStatus();
	}

	while (SDL_LCD_PumpEvents())
	{
		updateStatus();
		YMGUI_Refresh(g_ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(g_ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	if (s_blob != NULL) fclose(s_blob);
	gy_log_print("txt_edit exit ok\n");
	return 0;
}
