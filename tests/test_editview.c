#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_EditView.h"
#include "YMGUI_Button.h"
#include "YMGUI_Font.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_editview.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-01
  *	@Description: 可编辑多行文本框单测:聚焦、UTF-8 插入、ENTER 换行、退格跨行合并、Del、方向键移光标、
  *	              中文整码点、Wrap 折行、滚动钳制。判成败以 exit code 为准(main 返回 fails?1:0)。
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static void dummyFlush(GYdisp* d, const GYrect* a, const GYpx* b) { (void)d; (void)a; (void)b; }

//逐字节注入(与 SDL_TEXTINPUT 一致:UTF-8 多字节逐字节来)
static void typeStr(const char* s)
{
	for (; *s; s++)
		YMGUI_Inject_Key((uint32)(uint8)*s, 1);
}

int main(void)
{
	GYdisp disp;
	uint32 buf_px = SCR_W * BAND_H;
	disp.hor_res = SCR_W; disp.ver_res = SCR_H;
	disp.buf_px_cnt = buf_px;
	disp.buf1 = (GYpx*)GY_malloc1(buf_px * sizeof(GYpx));
	disp.buf2 = NULL; disp.flush_cb = dummyFlush; disp.user_data = NULL;

	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Inject_SetCtx(ctx);

	GYOBJ ev = YMGUI_Creat_EditView_Creat(ctx->root, 10, 10, 200, 80, 128*1024);//80/20=4 行可见;容量 128K 供 100KB 回归用

	//---- 未聚焦不吃键 ----
	typeStr("no");
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "") == 0, "no focus → keys ignored");
	CHECK(YMGUI_EditView_GetLineCount(ev) == 1, "empty → one placeholder line");

	//---- 点击聚焦 ----
	YMGUI_Inject_Pointer(50, 30, 1);
	YMGUI_Inject_Pointer(50, 30, 0);
	CHECK(ctx->focus_obj == ev, "click focuses editview");

	//---- 输入 + ENTER 换行 ----
	typeStr("ab");
	YMGUI_Inject_Key(GY_KEY_ENTER, 1);
	typeStr("cd");
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "ab\ncd") == 0, "typed with newline → 'ab\\ncd'");
	CHECK(YMGUI_EditView_GetLineCount(ev) == 2, "two display lines");
	CHECK(YMGUI_EditView_GetCursor(ev) == 5, "cursor at end (byte 5)");

	//---- 退格删 'd' ----
	YMGUI_Inject_Key(GY_KEY_BACKSPACE, 1);
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "ab\nc") == 0, "backspace → 'ab\\nc'");

	//---- 光标到行首,退格跨行合并 ----
	YMGUI_Inject_Key(GY_KEY_LEFT, 1);//c 前
	CHECK(YMGUI_EditView_GetCursor(ev) == 3, "LEFT → cursor before 'c' (byte 3)");
	YMGUI_Inject_Key(GY_KEY_BACKSPACE, 1);//删 '\n' 合并
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "abc") == 0, "backspace across line merges → 'abc'");
	CHECK(YMGUI_EditView_GetLineCount(ev) == 1, "merged to one line");

	//---- Del(合并后光标已在 'c' 前 byte 2,直接删 'c') ----
	CHECK(YMGUI_EditView_GetCursor(ev) == 2, "after merge cursor before 'c' (byte 2)");
	YMGUI_Inject_Key(GY_KEY_DEL, 1); //删 'c'
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "ab") == 0, "del → 'ab'");

	//---- 中文:整码点插入/退格 ----
	YMGUI_EditView_SetText(ev, "");
	typeStr("你好");//每字 3 字节
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "你好") == 0, "CJK typed '你好'");
	CHECK(YMGUI_EditView_GetCursor(ev) == 6, "CJK cursor at byte 6");
	YMGUI_Inject_Key(GY_KEY_LEFT, 1);//跳过 '好'(整码点 3 字节)
	CHECK(YMGUI_EditView_GetCursor(ev) == 3, "LEFT skips whole CJK codepoint");
	YMGUI_Inject_Key(GY_KEY_BACKSPACE, 1);//删 '你'
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "好") == 0, "backspace deletes whole CJK codepoint");

	//---- 中英混排换行 + UP/DOWN 跨行 ----
	YMGUI_EditView_SetText(ev, "hello\n世界x");
	CHECK(YMGUI_EditView_GetLineCount(ev) == 2, "two lines mixed");
	//光标在末尾(byte len). DOWN 无下一行不动;UP 到上一行
	size_t cur_end = YMGUI_EditView_GetCursor(ev);
	YMGUI_Inject_Pointer(50, 30, 1); YMGUI_Inject_Pointer(50, 30, 0);//确保聚焦
	YMGUI_EditView_SetText(ev, "hello\n世界x");//重置(SetText 后光标在末尾)
	YMGUI_Inject_Key(GY_KEY_UP, 1);//到第 0 行
	CHECK(YMGUI_EditView_GetCursor(ev) <= 5, "UP moves cursor into first line");
	(void)cur_end;

	//---- Wrap:窄框长行折多行 ----
	YMGUI_EditView_SetWrap(ev, 1);
	YMGUI_EditView_SetText(ev, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");//40 个 A,8px 宽,内容宽 192
	CHECK(YMGUI_EditView_GetLineCount(ev) > 1, "wrap splits long line");
	YMGUI_EditView_SetWrap(ev, 0);
	CHECK(YMGUI_EditView_GetLineCount(ev) == 1, "no-wrap → single line");

	//---- 滚动钳制:多行,scroll 不越界 ----
	YMGUI_EditView_SetText(ev, "l0\nl1\nl2\nl3\nl4\nl5\nl6\nl7");//8 行,视口 4 行
	YMGUI_EditView_SetScroll(ev, 30000);
	GYcoord maxs = (GYcoord)(8 * (YMGUI_Font_Default.cell_h + 4)) - 80;
	CHECK(YMGUI_EditView_GetScroll(ev) == maxs, "scroll clamped to max");
	YMGUI_EditView_SetScroll(ev, -100);
	CHECK(YMGUI_EditView_GetScroll(ev) == 0, "scroll clamped to 0");

	//---- 可配置内边距:参与折行、滚动视口和点击定位 ----
	YMGUI_EditView_SetPadding(ev, 12, 8, 6, 4);
	YMGUI_EditView_SetWrap(ev, 1);
	YMGUI_EditView_SetText(ev, "AAAAAAAAAAAAAAAAAAAAAAAA");//24*8=192px,内容宽 182px
	CHECK(YMGUI_EditView_GetLineCount(ev) == 2, "padding reduces wrap content width");
	YMGUI_EditView_SetWrap(ev, 0);
	YMGUI_EditView_SetText(ev, "l0\nl1\nl2\nl3\nl4");
	YMGUI_EditView_SetScroll(ev, 30000);
	maxs = (GYcoord)(5 * (YMGUI_Font_Default.cell_h + 4)) - (80 - 8 - 4);
	CHECK(YMGUI_EditView_GetScroll(ev) == maxs, "padding reduces vertical scroll viewport");
	YMGUI_EditView_SetText(ev, "hello");
	YMGUI_Inject_Pointer(10 + 12, 10 + 8, 1); YMGUI_Inject_Pointer(10 + 12, 10 + 8, 0);
	CHECK(YMGUI_EditView_GetCursor(ev) == 0, "click at padded content origin positions cursor at start");
	YMGUI_EditView_SetPadding(ev, 4, 0, 4, 0);//后续用默认点击坐标

	//======================================================================
	// 选区 / 剪贴板 / 查找替换 / 撤销 / 点击定位 / 双击选词(第 23 轮新增)
	//   注:未注册 SDL 剪贴板后端 → YMGUI_Clipboard 走库内静态缓冲(正是要测的裸机回退路径)
	//======================================================================
	YMGUI_EditView_SetWrap(ev, 0);

	//---- 点击定位光标(此前缺口:点击只聚焦不定位)----
	//默认 ASCII 字体 8px 宽;editview 在 (10,10) 200x80,内边距 4。line_h=cell_h+4=20
	YMGUI_EditView_SetText(ev, "hello world");//单行 11 字节
	YMGUI_Inject_Pointer(14, 15, 1); YMGUI_Inject_Pointer(14, 15, 0);//行首附近
	CHECK(YMGUI_EditView_GetCursor(ev) == 0, "click at left edge → cursor 0");
	YMGUI_Inject_Pointer(14 + 8 * 3, 15, 1); YMGUI_Inject_Pointer(14 + 8 * 3, 15, 0);//约第 3 字符
	CHECK(YMGUI_EditView_GetCursor(ev) == 3, "click positions cursor mid-line");

	//---- 双击选词 ----
	YMGUI_Inject_Pointer(14 + 8, 15, 1); YMGUI_Inject_Pointer(14 + 8, 15, 0);//落在 'hello' 内
	YMGUI_Inject_DoubleClick(14 + 8, 15);
	CHECK(YMGUI_EditView_HasSelection(ev), "double-click makes a selection");
	{
		char w[32];
		YMGUI_EditView_GetSelectionText(ev, w, sizeof(w));
		CHECK(strcmp(w, "hello") == 0, "double-click selects word 'hello'");
	}

	//---- 全选 API ----
	YMGUI_EditView_SelectAll(ev);
	{
		size_t s, e;
		YMGUI_EditView_GetSelection(ev, &s, &e);
		CHECK(s == 0 && e == 11, "SelectAll spans whole text");
	}

	//---- Ctrl+A(键路径)+ Ctrl+C 复制 + Ctrl+V 粘贴 ----
	YMGUI_Inject_Pointer(14, 15, 1); YMGUI_Inject_Pointer(14, 15, 0);//聚焦进编辑,光标 0,清选区
	YMGUI_Inject_Key(GY_KEY_SEL_ALL, 1);
	CHECK(YMGUI_EditView_HasSelection(ev), "Ctrl+A selects all");
	YMGUI_Inject_Key(GY_KEY_COPY, 1);
	CHECK(strcmp(YMGUI_Clipboard_GetText(), "hello world") == 0, "Ctrl+C copies selection to clipboard");
	//光标到文尾,粘贴一次 → 文本翻倍
	YMGUI_Inject_Key(GY_KEY_DOC_END, 1);
	YMGUI_Inject_Key(GY_KEY_PASTE, 1);
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "hello worldhello world") == 0, "Ctrl+V pastes clipboard at cursor");

	//---- 有选区时打字替换选区 ----
	YMGUI_EditView_SetText(ev, "abcdef");
	YMGUI_Inject_Pointer(14, 15, 1); YMGUI_Inject_Pointer(14, 15, 0);//聚焦,光标 0
	YMGUI_Inject_Key(GY_KEY_SHIFT_RIGHT, 1);//选 'a'
	YMGUI_Inject_Key(GY_KEY_SHIFT_RIGHT, 1);//选 'ab'
	{
		size_t s, e; YMGUI_EditView_GetSelection(ev, &s, &e);
		CHECK(s == 0 && e == 2, "Shift+Right extends selection to 'ab'");
	}
	typeStr("X");//替换 'ab' → 'X'
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "Xcdef") == 0, "typing replaces selection");

	//---- Ctrl+X 剪切 ----
	YMGUI_EditView_SetText(ev, "cut me");
	YMGUI_Inject_Pointer(14, 15, 1); YMGUI_Inject_Pointer(14, 15, 0);
	YMGUI_Inject_Key(GY_KEY_SEL_ALL, 1);
	YMGUI_Inject_Key(GY_KEY_CUT, 1);
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "") == 0, "Ctrl+X removes selection from text");
	CHECK(strcmp(YMGUI_Clipboard_GetText(), "cut me") == 0, "Ctrl+X puts selection on clipboard");

	//---- Ctrl+Z 单级撤销(撤销上一步打字)----
	YMGUI_EditView_SetText(ev, "");
	YMGUI_Inject_Pointer(14, 15, 1); YMGUI_Inject_Pointer(14, 15, 0);
	typeStr("hi");
	YMGUI_Inject_Key(GY_KEY_ENTER, 1);//这一步改动:插 '\n'(快照 = "hi")
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "hi\n") == 0, "before undo: 'hi\\n'");
	YMGUI_Inject_Key(GY_KEY_UNDO, 1);//回滚到 "hi"
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "hi") == 0, "Ctrl+Z reverts last edit");

	//---- Home/End 行首尾 ----
	YMGUI_EditView_SetText(ev, "line one\nline two");
	YMGUI_Inject_Pointer(14, 15, 1); YMGUI_Inject_Pointer(14, 15, 0);//聚焦到第 0 行
	YMGUI_Inject_Key(GY_KEY_END, 1);
	CHECK(YMGUI_EditView_GetCursor(ev) == 8, "End → end of first line (byte 8)");
	YMGUI_Inject_Key(GY_KEY_HOME, 1);
	CHECK(YMGUI_EditView_GetCursor(ev) == 0, "Home → start of first line");
	YMGUI_Inject_Key(GY_KEY_DOC_END, 1);
	CHECK(YMGUI_EditView_GetCursor(ev) == 17, "Ctrl+End → end of document");
	YMGUI_Inject_Key(GY_KEY_DOC_HOME, 1);
	CHECK(YMGUI_EditView_GetCursor(ev) == 0, "Ctrl+Home → start of document");

	//---- 查找 / 替换 ----
	YMGUI_EditView_SetText(ev, "foo bar foo baz foo");
	CHECK(YMGUI_EditView_FindNext(ev, "foo"), "FindNext finds first 'foo'");
	{
		size_t s, e; YMGUI_EditView_GetSelection(ev, &s, &e);
		CHECK(s == 0 && e == 3, "first match selected at 0..3");
	}
	YMGUI_EditView_FindNext(ev, "foo");//下一个
	{
		size_t s, e; YMGUI_EditView_GetSelection(ev, &s, &e);
		CHECK(s == 8 && e == 11, "FindNext advances to second match at 8..11");
	}
	//Replace 当前匹配为 'X'
	YMGUI_EditView_Replace(ev, "foo", "X");
	CHECK(strstr(YMGUI_EditView_GetText(ev), "bar X baz") != NULL, "Replace swaps current match");
	//ReplaceAll 剩余
	YMGUI_EditView_SetText(ev, "foo bar foo baz foo");
	{
		size_t n = YMGUI_EditView_ReplaceAll(ev, "foo", "QQ");
		CHECK(n == 3, "ReplaceAll replaced 3 occurrences");
		CHECK(strcmp(YMGUI_EditView_GetText(ev), "QQ bar QQ baz QQ") == 0, "ReplaceAll result correct");
	}
	//替换成更长串不越界(缓冲兜底)
	YMGUI_EditView_SetText(ev, "aaa");
	YMGUI_EditView_ReplaceAll(ev, "a", "bb");
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "bbbbbb") == 0, "ReplaceAll grows text correctly");

	//---- 光标行列(状态栏用)----
	YMGUI_EditView_SetText(ev, "ab\ncde");
	YMGUI_Inject_Pointer(14, 15, 1); YMGUI_Inject_Pointer(14, 15, 0);
	YMGUI_Inject_Key(GY_KEY_DOC_END, 1);
	{
		size_t row, col;
		YMGUI_EditView_GetCursorRowCol(ev, &row, &col);
		CHECK(row == 2 && col == 4, "cursor row/col at doc end = 2,4");
	}

	//---- 程序插入:输入法上屏到当前光标/选区 ----
	YMGUI_EditView_SetText(ev, "ac");
	YMGUI_Inject_Key(GY_KEY_LEFT, 1);//a|c
	YMGUI_EditView_InsertText(ev, "b");
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "abc") == 0, "InsertText inserts at cursor");
	YMGUI_EditView_SelectAll(ev);
	YMGUI_EditView_InsertText(ev, "替换");
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "替换") == 0, "InsertText replaces selection");
	YMGUI_EditView_Undo(ev);
	CHECK(strcmp(YMGUI_EditView_GetText(ev), "abc") == 0, "InsertText participates in undo");

	//---- 剪贴板后端注册可覆盖(移植缝验证)----
	YMGUI_Clipboard_SetText("internal");
	CHECK(strcmp(YMGUI_Clipboard_GetText(), "internal") == 0, "clipboard internal buffer roundtrip");

	//---- 大容量:存入 > 64KB 文本不截断/不溢出(32 位位宽验证)----
	{
		//旧版 len/cursor 为 uint16,>65535 会 SetText 死循环 + 堆越界。此处存 100000 字节。
		static char big[100003];
		uint32 i;
		for (i = 0; i < 100000; i++)
			big[i] = (char)('a' + (i % 26));
		big[100000] = '\0';
		YMGUI_EditView_SetText(ev, big);
		CHECK(strlen(YMGUI_EditView_GetText(ev)) == 100000, "100KB text stored without truncation");
		YMGUI_EditView_SelectAll(ev);
		{
			size_t s, e;
			YMGUI_EditView_GetSelection(ev, &s, &e);
			CHECK(s == 0 && e == 100000, "SelectAll spans 100KB (offset > 65535)");
		}
		YMGUI_EditView_SetText(ev, "");//清回去,避免影响后续
	}

	//---- 撤销开关:默认开可撤销;关掉后 Undo 变空操作;再开恢复能力 ----
	{
		YMGUI_EditView_SetText(ev, "abc");
		YMGUI_Inject_Pointer(20, 15, 1); YMGUI_Inject_Pointer(20, 15, 0);//聚焦
		typeStr("X");//改动:插入 X(触发一次快照)
		YMGUI_EditView_Undo(ev);
		CHECK(strcmp(YMGUI_EditView_GetText(ev), "abc") == 0, "undo enabled by default → restores");
		//关撤销:改动后 Undo 不应回滚
		YMGUI_EditView_SetUndoEnabled(ev, 0);
		YMGUI_EditView_SetText(ev, "abc");
		YMGUI_Inject_Pointer(20, 15, 1); YMGUI_Inject_Pointer(20, 15, 0);
		typeStr("Y");
		{ const char* t = YMGUI_EditView_GetText(ev);
		  YMGUI_EditView_Undo(ev);
		  CHECK(strcmp(YMGUI_EditView_GetText(ev), t) == 0, "undo disabled → Undo is no-op"); }
		//重开撤销:能力恢复
		YMGUI_EditView_SetUndoEnabled(ev, 1);
		YMGUI_EditView_SetText(ev, "abc");
		YMGUI_Inject_Pointer(20, 15, 1); YMGUI_Inject_Pointer(20, 15, 0);
		typeStr("Z");
		YMGUI_EditView_Undo(ev);
		CHECK(strcmp(YMGUI_EditView_GetText(ev), "abc") == 0, "undo re-enabled → restores again");
		YMGUI_EditView_SetText(ev, "");
	}

	//---- 焦点丢失后不吃键 ----
	GYOBJ btn = YMGUI_Creat_Button_Creat(ctx->root, 10, 200, 80, 30);
	YMGUI_Inject_Pointer(40, 215, 1);
	YMGUI_Inject_Pointer(40, 215, 0);
	CHECK(!(ev->state & GY_STATE_Focused), "editview lost focus");
	const char* before = YMGUI_EditView_GetText(ev);
	char snap[64]; strncpy(snap, before, 63); snap[63] = '\0';
	typeStr("Z");
	CHECK(strcmp(YMGUI_EditView_GetText(ev), snap) == 0, "after focus lost → keys ignored");
	(void)btn;

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_editview: ALL PASS\n");
	else
		printf("test_editview: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
