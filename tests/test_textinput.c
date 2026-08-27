#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_Button.h"
#include "YMGUI_Mem.h"
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    test_textinput.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 文本输入框单测:点击聚焦、字符插入、退格、光标左移插入、Del、焦点切换、非聚焦不吃键
  ***************************************************************************************************************************/

#define SCR_W 320
#define SCR_H 240
#define BAND_H 40

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static void dummyFlush(GYdisp* d, const GYrect* a, const GYpx* b) { (void)d; (void)a; (void)b; }

//输入一个字符串(逐字符注入按下)
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

	GYOBJ ti = YMGUI_Creat_TextInput_Creat(ctx->root, 20, 40, 200, 26, 12);//y=40..66

	//---- 未聚焦时按键不应改变文本 ----
	typeStr("no");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "") == 0, "no focus → keys ignored");

	//---- 点击聚焦 ----
	YMGUI_Inject_Pointer(100, 52, 1);
	YMGUI_Inject_Pointer(100, 52, 0);
	CHECK(ctx->focus_obj == ti, "click focuses textinput");
	CHECK(ti->state & GY_STATE_Focused, "focused state bit set");

	//---- 输入文字 ----
	typeStr("Hello");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "Hello") == 0, "typed 'Hello'");

	//---- 退格 ----
	YMGUI_Inject_Key(GY_KEY_BACKSPACE, 1);
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "Hell") == 0, "backspace → 'Hell'");

	//---- 光标左移两位,在中间插入 ----
	YMGUI_Inject_Key(GY_KEY_LEFT, 1);
	YMGUI_Inject_Key(GY_KEY_LEFT, 1);//光标在 'e' 后 'l' 前:He|ll
	typeStr("X");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "HeXll") == 0, "insert at cursor → 'HeXll'");

	//---- Del 删除光标处('l') ----
	YMGUI_Inject_Key(GY_KEY_DEL, 1);//He X | ll → 删掉光标处第一个 l
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "HeXl") == 0, "del at cursor → 'HeXl'");

	//---- 焦点切到别处后,原输入框不再吃键 ----
	GYOBJ btn = YMGUI_Creat_Button_Creat(ctx->root, 20, 100, 80, 30);//非 focusable
	YMGUI_Inject_Pointer(50, 115, 1);//点按钮 → 清焦点
	YMGUI_Inject_Pointer(50, 115, 0);
	CHECK(ctx->focus_obj == NULL, "clicking non-focusable clears focus");
	CHECK(!(ti->state & GY_STATE_Focused), "textinput lost focus");
	typeStr("Z");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "HeXl") == 0, "after focus lost → keys ignored");

	//---- SetText 编程设值 ----
	YMGUI_TextInput_SetText(ti, "abc");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "abc") == 0, "SetText works");
	//再聚焦继续追加(光标应在末尾)
	YMGUI_Inject_Pointer(100, 52, 1);
	YMGUI_Inject_Pointer(100, 52, 0);
	typeStr("d");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "abcd") == 0, "SetText leaves cursor at end");

	//---- 中文(UTF-8 多字节)输入:逐字节注入应拼回整码点 ----
	YMGUI_TextInput_SetText(ti, "");
	YMGUI_Inject_Pointer(100, 52, 1);
	YMGUI_Inject_Pointer(100, 52, 0);
	typeStr("你好");//每个汉字 3 字节,共 6 字节
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "你好") == 0, "typed CJK '你好'");
	//退格删整字(不劈半个汉字):应剩 '你'
	YMGUI_Inject_Key(GY_KEY_BACKSPACE, 1);
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "你") == 0, "backspace deletes whole CJK codepoint");
	//中英混排 + 光标按码点左移后插入
	typeStr("好A");//你好A
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "你好A") == 0, "mixed CJK+ASCII");
	YMGUI_Inject_Key(GY_KEY_LEFT, 1);//跳过 'A'
	YMGUI_Inject_Key(GY_KEY_LEFT, 1);//跳过 '好'(整码点)
	typeStr("们");//你们好A
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "你们好A") == 0, "LEFT moves by codepoint, insert mid-string");
	//Del 删光标处整码点('好')
	YMGUI_Inject_Key(GY_KEY_DEL, 1);
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "你们A") == 0, "DEL deletes whole CJK codepoint");

	//---- 创建期容量:最多保存 capacity 字节,内部另留 '\0' ----
	YMGUI_TextInput_SetText(ti, "123456789012345");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "123456789012") == 0, "SetText truncates to configured capacity");
	typeStr("X");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "123456789012") == 0, "typing stops at configured capacity");

	//---- 水平视口:长文本光标在尾部时自动前移文字 ----
	GYOBJ narrow = YMGUI_Creat_TextInput_Creat(ctx->root, 130, 100, 48, 24, 32);
	YMGUI_TextInput_SetText(narrow, "abcdefghijklmnop");
	CHECK(YMGUI_TextInput_GetScrollX(narrow) > 0, "long single-line text scrolls to keep end cursor visible");

	//---- Shift 选区 + 剪贴板动作 ----
	YMGUI_TextInput_SetText(ti, "abcdef");
	YMGUI_SetFocus(ctx, ti);
	ti->state |= GY_STATE_Editing;
	YMGUI_Inject_Key(GY_KEY_SHIFT_LEFT, 1);
	YMGUI_Inject_Key(GY_KEY_SHIFT_LEFT, 1);
	char selected[8];
	CHECK(YMGUI_TextInput_HasSelection(ti), "Shift+Left creates selection");
	CHECK(YMGUI_TextInput_GetSelectionText(ti, selected, sizeof(selected)) == 2 && strcmp(selected, "ef") == 0,
	      "selection text is reported");
	YMGUI_Inject_Key(GY_KEY_COPY, 1);
	CHECK(strcmp(YMGUI_Clipboard_GetText(), "ef") == 0, "Ctrl+C copies textinput selection");
	YMGUI_Inject_Key(GY_KEY_CUT, 1);
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "abcd") == 0, "Ctrl+X removes selected text");
	YMGUI_Clipboard_SetText("X\nY\rZ");
	YMGUI_Inject_Key(GY_KEY_PASTE, 1);
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "abcdXYZ") == 0, "single-line paste filters CR/LF");
	YMGUI_Inject_Key(GY_KEY_SEL_ALL, 1);
	typeStr("Q");
	CHECK(strcmp(YMGUI_TextInput_GetText(ti), "Q") == 0, "typing replaces selected text");

	//---- 鼠标拖动选择 ----
	YMGUI_TextInput_SetText(ti, "abcdef");
	YMGUI_Inject_Pointer(25, 52, 1);
	YMGUI_Inject_Pointer(60, 52, 1);
	YMGUI_Inject_Pointer(60, 52, 0);
	CHECK(YMGUI_TextInput_HasSelection(ti), "mouse drag creates selection");

	YMGUI_Free_CtxFree(ctx);
	GY_free1(disp.buf1);

	if (fails == 0)
		printf("test_textinput: ALL PASS\n");
	else
		printf("test_textinput: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
