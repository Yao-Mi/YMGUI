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
#include "YMGUI_Grid.h"
#include "YMGUI_Font.h"
#include "SDL_LCD.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    excel_edit.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: project_Demo 第三个基础验证项目 —— 电子表格(催生库控件 Grid)。960x600 窗口。
  *	              库侧 Grid 只管"显示串 + 选中 + 二维滚动 + 编辑意图",不认识公式;电子表格语义
  *	              (A1 地址、公式引擎、重算、定点格式化)全在本 app。公式深度:算术 + 单元格引用 +
  *	              区间函数(=A1+B1*2、括号、SUM/AVG/MIN/MAX(A1:B5))。数值走 16.16 定点(无 FPU),
  *	              显示 2 位小数。编辑两条路径:顶部公式栏 fx + 单元格内双击就地编辑框。复用 GB2312
  *	              全字库外部 blob 回退(同 txt_edit/files_manager)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * 备注信息:
  * 1.重算策略:任一格编辑后全表重算一遍(N=ROWS×COLS 很小,够用;依赖图拓扑排序是过度设计)。
  * 2.循环引用检测:evalCell 用 visiting 标志位,命中 → #REF!(防无限递归/栈溢出)。
  * 3.编辑提交:公式栏/就地框都"逐键实时提交"——每次 changed 写回 raw + 全表重算 + 刷显示。
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 960
#define SCR_H 600
#define BAND_H 60

#define TOOLBAR_H 34   //第一排:公式栏
#define TOOLBAR2_H 30  //第二排:操作按钮
#define STATUS_H  20

//网格规模(<= 库 GY_GRID_MAX_ROWS/COLS)。26 列 A..Z + 30 行:内容宽(26×72=1872)>视口,
//列名行可左右拖观察;插入列有"右推"效果(末列挤出);行号列可上下拖(30×22=660>视口高)。
#define G_ROWS 30
#define G_COLS 26
#define CELL_RAW_MAX 32   //单元格原文上限(> 显示串 GY_GRID_CELL_LEN,原文可比显示长)

//单元格种类
enum { KIND_EMPTY = 0, KIND_NUMBER, KIND_TEXT, KIND_FORMULA };
//错误码(0 = 无错)
enum { ERR_NONE = 0, ERR_REF, ERR_DIV0, ERR_SYNTAX };

//---- GB2312 全字库回退(同 txt_edit/files_manager)----
#ifndef GB2312_BIN_PATH
#define GB2312_BIN_PATH "gb2312_glyphs.bin"
#endif
extern const uint16 YMGUI_GB2312_cps[];
extern const uint16 YMGUI_GB2312_glyph_count;
static FILE* s_blob = NULL;
static uint32 flashRead(const GYfont* font, uint32 off, uint32 len, uint8* buf)
{
	(void)font;
	if (s_blob == NULL) return 0;
	if (fseek(s_blob, (long)off, SEEK_SET) != 0) return 0;
	return (uint32)fread(buf, 1, len, s_blob);
}
static GYfont s_gb_font = { NULL, YMGUI_GB2312_cps, 0, 0, 0, 16, 16, 8, 4, NULL, flashRead };

//---- 单元格模型(app 侧,与 Grid 的显示串分离)----
typedef struct
{
	char    raw[CELL_RAW_MAX];//用户原文:"10" / "3.14" / "=A1+B1" / "hello"
	GYvalue value;            //计算出的定点值(数字/公式)
	uint8   kind;             //KIND_*
	uint8   error;            //ERR_*
	uint8   computed;         //本轮重算是否已求值(记忆化)
	uint8   visiting;         //正在求值(循环引用检测)
}Cell;

//---- 全局 ----
static GYCTX g_ctx;
static GYOBJ g_grid;
static GYOBJ g_fx;         //公式栏输入框(显示/编辑当前选中格原文)
static GYOBJ g_addr_lbl;   //地址标签(如 "C1")
static GYOBJ g_status;     //状态栏(值/错误)
static GYOBJ g_editor;     //就地编辑框(双击时叠到格上;平时 Hidden)
static Cell  g_cells[G_ROWS][G_COLS];
static int32 g_sel_r = -1, g_sel_c = -1;//当前选中格(公式栏提交目标)
static int   g_selftest_fail = 0;       //selftest 失败置 1 → main 返回非 0
static int   g_suppress = 0;            //抑制 changed 回调递归(SetText 不触发,这里防御性留位)

//前置声明
static GYvalue evalCell(int r, int c);
static void recalcAll(void);
static void refreshDisplay(void);

//===========================================================================
// 定点小数(16.16)解析 / 格式化 —— 本地实现,不用 sprintf 浮点(裸机口径)
//===========================================================================

//字符类
static int isDigitC(char c) { return c >= '0' && c <= '9'; }
static int isAlphaC(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static char upC(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c; }

/**
  * @brief 从 *p 读一个十进制数(可带小数点)→ 定点 GYvalue,推进 *p。假定已在数字/'.' 上。
  *        整数部分与小数部分分开定点化(避免溢出);小数最多取 6 位(够 2 位显示 + 余量)。
  */
static GYvalue parseNumberAt(const char** p)
{
	const char* s = *p;
	int64 ipart = 0;
	while (isDigitC(*s)) { ipart = ipart * 10 + (*s - '0'); s++; }
	GYvalue val = (GYvalue)(ipart << GY_FP_SHIFT);
	if (*s == '.')
	{
		s++;
		int64 num = 0, den = 1;
		int cnt = 0;
		while (isDigitC(*s) && cnt < 6) { num = num * 10 + (*s - '0'); den *= 10; s++; cnt++; }
		while (isDigitC(*s)) s++;//多余小数位丢弃
		//小数部分定点:num/den → (num<<16)/den
		if (den > 1)
			val += (GYvalue)((num << GY_FP_SHIFT) / den);
	}
	*p = s;
	return val;
}

/**
  * @brief 整串是否是"纯数字"(可带前导 '-'、一个小数点、首尾空格)。用于分类 NUMBER vs TEXT。
  */
static int isNumberStr(const char* s)
{
	while (*s == ' ') s++;
	if (*s == '-' || *s == '+') s++;
	int digits = 0, dot = 0;
	while (*s != '\0' && *s != ' ')
	{
		if (isDigitC(*s)) { digits++; s++; }
		else if (*s == '.' && !dot) { dot = 1; s++; }
		else return 0;
	}
	while (*s == ' ') s++;
	return (digits > 0 && *s == '\0');
}

/**
  * @brief 解析纯数字串(带符号)→ 定点。调用前须 isNumberStr 为真。
  */
static GYvalue parseSignedNumber(const char* s)
{
	while (*s == ' ') s++;
	int neg = 0;
	if (*s == '-') { neg = 1; s++; }
	else if (*s == '+') s++;
	GYvalue v = parseNumberAt(&s);
	return neg ? -v : v;
}

/**
  * @brief 定点 → "整数.两位小数"字符串(四舍五入到 2 位),写进 out(cap>=16)。
  */
static void formatValue(GYvalue v, char* out, size_t cap)
{
	if (cap < 8) { if (cap) out[0] = '\0'; return; }
	int neg = (v < 0);
	uint32 mag = (uint32)(neg ? -(int64)v : (int64)v);
	uint32 ipart = mag >> GY_FP_SHIFT;
	uint32 frac = mag & ((1u << GY_FP_SHIFT) - 1);
	//小数两位:frac/65536*100,四舍五入
	uint32 dec = (uint32)(((int64)frac * 100 + (1 << (GY_FP_SHIFT - 1))) >> GY_FP_SHIFT);
	if (dec >= 100) { dec -= 100; ipart += 1; }//进位
	if (ipart == 0 && dec == 0) neg = 0;//避免 "-0.00"
	//拼整数部分(倒序)
	char tmp[12]; int n = 0;
	if (ipart == 0) tmp[n++] = '0';
	while (ipart > 0 && n < (int)sizeof(tmp)) { tmp[n++] = (char)('0' + ipart % 10); ipart /= 10; }
	size_t i = 0;
	if (neg && i + 1 < cap) out[i++] = '-';
	while (n > 0 && i + 1 < cap) out[i++] = tmp[--n];
	if (i + 1 < cap) out[i++] = '.';
	if (i + 1 < cap) out[i++] = (char)('0' + dec / 10);
	if (i + 1 < cap) out[i++] = (char)('0' + dec % 10);
	out[i] = '\0';
}

//===========================================================================
// 公式引擎(纯 app 侧,无 libm)。递归下降:
//   expr   = term (('+'|'-') term)*
//   term   = factor (('*'|'/') factor)*
//   factor = ['-'|'+'] primary
//   primary= number | addr | func '(' addr ':' addr ')' | '(' expr ')'
// 引用求值经 evalCell 递归(带 visiting 循环检测)。任何错误置 ps->err 并短路。
//===========================================================================

typedef struct { const char* p; int err; } Parser;

static GYvalue parseExpr(Parser* ps);//前置(递归)

//跳空白
static void skipWs(Parser* ps) { while (*ps->p == ' ' || *ps->p == '\t') ps->p++; }

/**
  * @brief 解析单元格地址(如 "A1"/"j30",列 A..,行 1 基)→ *r/*c(0 基)。
  *        成功推进 ps->p 返回 1;失败(格式不符)返回 0 不推进。
  */
static int parseAddr(Parser* ps, int* r, int* c)
{
	const char* s = ps->p;
	if (!isAlphaC(*s)) return 0;
	int col = 0, letters = 0;
	while (isAlphaC(*s)) { col = col * 26 + (upC(*s) - 'A' + 1); s++; letters++; }
	if (!isDigitC(*s)) return 0;//字母后必须紧跟行号,否则不是地址(可能是函数名)
	int row = 0;
	while (isDigitC(*s)) { row = row * 10 + (*s - '0'); s++; }
	*c = col - 1;//1 基 → 0 基
	*r = row - 1;
	ps->p = s;
	(void)letters;
	return 1;
}

/**
  * @brief 取被引格的值;越界 → ERR_REF;被引格自身有错 → 传播其错。
  */
static GYvalue resolveRef(Parser* ps, int r, int c)
{
	if (r < 0 || c < 0 || r >= G_ROWS || c >= G_COLS)
	{
		ps->err = ERR_REF;
		return 0;
	}
	GYvalue v = evalCell(r, c);
	if (g_cells[r][c].error != ERR_NONE)
		ps->err = g_cells[r][c].error;//传播被引格的错
	return v;
}

/**
  * @brief 区间函数:name 已知(SUM/AVG/MIN/MAX),ps->p 停在 '(' 上。
  *        解析 addr ':' addr,遍历矩形区聚合。空格计 0(SUM/AVG 口径)。
  */
static GYvalue parseFunc(Parser* ps, int fn)
{
	skipWs(ps);
	if (*ps->p != '(') { ps->err = ERR_SYNTAX; return 0; }
	ps->p++;
	skipWs(ps);
	int r0, c0, r1, c1;
	if (!parseAddr(ps, &r0, &c0)) { ps->err = ERR_SYNTAX; return 0; }
	skipWs(ps);
	if (*ps->p != ':') { ps->err = ERR_SYNTAX; return 0; }
	ps->p++;
	skipWs(ps);
	if (!parseAddr(ps, &r1, &c1)) { ps->err = ERR_SYNTAX; return 0; }
	skipWs(ps);
	if (*ps->p != ')') { ps->err = ERR_SYNTAX; return 0; }
	ps->p++;
	//规整区间(允许反向)
	if (r0 > r1) { int t = r0; r0 = r1; r1 = t; }
	if (c0 > c1) { int t = c0; c0 = c1; c1 = t; }
	if (r0 < 0 || c0 < 0 || r1 >= G_ROWS || c1 >= G_COLS) { ps->err = ERR_REF; return 0; }

	GYvalue acc = 0, mn = 0, mx = 0;
	int count = 0;
	for (int r = r0; r <= r1; r++)
		for (int c = c0; c <= c1; c++)
		{
			GYvalue v = evalCell(r, c);
			if (g_cells[r][c].error != ERR_NONE) { ps->err = g_cells[r][c].error; return 0; }
			if (count == 0) { mn = mx = v; }
			else { if (v < mn) mn = v; if (v > mx) mx = v; }
			acc += v;
			count++;
		}
	switch (fn)
	{
	case 0: return acc;                                   //SUM
	case 1: return (count > 0) ? (GYvalue)(acc / count) : 0;//AVG(整型除计数,近似)
	case 2: return mn;                                    //MIN
	case 3: return mx;                                    //MAX
	}
	return 0;
}

/**
  * @brief primary = number | func(...) | addr | '(' expr ')'
  */
static GYvalue parsePrimary(Parser* ps)
{
	skipWs(ps);
	if (*ps->p == '#')//错误 token(如删行/列后残留的 #REF!)→ 直接置错
	{
		ps->err = ERR_REF;
		while (*ps->p != '\0' && *ps->p != ' ' && *ps->p != '\t') ps->p++;//吞掉整个 token
		return 0;
	}
	if (*ps->p == '(')
	{
		ps->p++;
		GYvalue v = parseExpr(ps);
		skipWs(ps);
		if (*ps->p != ')') { ps->err = ERR_SYNTAX; return 0; }
		ps->p++;
		return v;
	}
	if (isDigitC(*ps->p) || (*ps->p == '.' && isDigitC(ps->p[1])))
		return parseNumberAt(&ps->p);
	if (isAlphaC(*ps->p))
	{
		//向前看:字母串后紧跟 '(' → 函数;否则当地址
		const char* s = ps->p;
		char name[8]; int nl = 0;
		while (isAlphaC(*s)) { if (nl < 7) name[nl++] = upC(*s); s++; }
		name[nl] = '\0';
		const char* t = s;
		while (*t == ' ' || *t == '\t') t++;
		if (*t == '(')
		{
			int fn = -1;
			if (strcmp(name, "SUM") == 0) fn = 0;
			else if (strcmp(name, "AVG") == 0) fn = 1;
			else if (strcmp(name, "MIN") == 0) fn = 2;
			else if (strcmp(name, "MAX") == 0) fn = 3;
			if (fn < 0) { ps->err = ERR_SYNTAX; return 0; }
			ps->p = s;//停到 '(' 前(parseFunc 会 skipWs)
			return parseFunc(ps, fn);
		}
		//地址
		int r, c;
		if (parseAddr(ps, &r, &c))
			return resolveRef(ps, r, c);
		ps->err = ERR_SYNTAX;
		return 0;
	}
	ps->err = ERR_SYNTAX;
	return 0;
}

/**
  * @brief factor = ['-'|'+'] primary(一元正负)
  */
static GYvalue parseFactor(Parser* ps)
{
	skipWs(ps);
	if (*ps->p == '-') { ps->p++; return -parseFactor(ps); }
	if (*ps->p == '+') { ps->p++; return parseFactor(ps); }
	return parsePrimary(ps);
}

/**
  * @brief term = factor (('*'|'/') factor)*
  */
static GYvalue parseTerm(Parser* ps)
{
	GYvalue v = parseFactor(ps);
	for (;;)
	{
		skipWs(ps);
		char op = *ps->p;
		if (op != '*' && op != '/') break;
		ps->p++;
		GYvalue rhs = parseFactor(ps);
		if (ps->err) return 0;
		if (op == '*') v = GY_FP_MUL(v, rhs);
		else
		{
			if (rhs == 0) { ps->err = ERR_DIV0; return 0; }
			v = GY_FP_DIV(v, rhs);
		}
	}
	return v;
}

/**
  * @brief expr = term (('+'|'-') term)*
  */
static GYvalue parseExpr(Parser* ps)
{
	GYvalue v = parseTerm(ps);
	for (;;)
	{
		skipWs(ps);
		char op = *ps->p;
		if (op != '+' && op != '-') break;
		ps->p++;
		GYvalue rhs = parseTerm(ps);
		if (ps->err) return 0;
		v = (op == '+') ? (v + rhs) : (v - rhs);
	}
	return v;
}

/**
  * @brief 求 (r,c) 格的值(记忆化 + 循环检测)。数字/文本直接返回;公式解析求值。
  *        循环引用(求值过程中再次进入本格)→ 置 ERR_REF 返回 0(防栈溢出)。
  */
static GYvalue evalCell(int r, int c)
{
	Cell* cell = &g_cells[r][c];
	if (cell->computed)
		return cell->value;
	if (cell->visiting)
	{
		cell->error = ERR_REF;//循环引用
		cell->computed = 1;
		cell->value = 0;
		return 0;
	}
	switch (cell->kind)
	{
	case KIND_EMPTY:
	case KIND_TEXT:
		cell->value = 0;
		cell->computed = 1;
		return 0;
	case KIND_NUMBER:
		//value 已在 setCellRaw 里定点化
		cell->computed = 1;
		return cell->value;
	case KIND_FORMULA:
	default:
		break;
	}
	cell->visiting = 1;
	Parser ps;
	ps.p = cell->raw + 1;//跳过 '='
	ps.err = ERR_NONE;
	GYvalue v = parseExpr(&ps);
	skipWs(&ps);
	if (ps.err == ERR_NONE && *ps.p != '\0')
		ps.err = ERR_SYNTAX;//有尾巴没吃完
	cell->visiting = 0;
	//循环检测可能已在递归中把本格标错;别覆盖
	if (cell->error != ERR_REF)
		cell->error = ps.err;
	cell->value = (cell->error == ERR_NONE) ? v : 0;
	cell->computed = 1;
	return cell->value;
}

/**
  * @brief 写回某格原文并分类(EMPTY/NUMBER/TEXT/FORMULA);数字顺带定点化。不重算。
  */
static void setCellRaw(int r, int c, const char* raw)
{
	if (r < 0 || c < 0 || r >= G_ROWS || c >= G_COLS) return;
	Cell* cell = &g_cells[r][c];
	uint16 i = 0;
	while (raw[i] != '\0' && i < CELL_RAW_MAX - 1) { cell->raw[i] = raw[i]; i++; }
	cell->raw[i] = '\0';
	cell->error = ERR_NONE;
	cell->value = 0;
	const char* s = cell->raw;
	while (*s == ' ') s++;
	if (*s == '\0')
		cell->kind = KIND_EMPTY;
	else if (*s == '=')
		cell->kind = KIND_FORMULA;
	else if (isNumberStr(cell->raw))
	{
		cell->kind = KIND_NUMBER;
		cell->value = parseSignedNumber(cell->raw);
	}
	else
		cell->kind = KIND_TEXT;
}

//(r,c) → "A1" 地址串(前置声明,rewriteFormulaRefs 复用)
static void cellAddr(int r, int c, char* out, size_t cap);

/**
  * @brief 重写公式串里的地址引用:插入行/列后调整。行 >= ins_row 的引用行号 +1(ins_row<0 不调行);
  *        列 >= ins_col 的引用列号 +1(ins_col<0 不调列)。函数名(字母后跟非数字)与数字原样保留。
  *        覆盖 SUM(A1:B5) 两端点(都是独立地址 token)。写入 out(cap);超界则截断。
  */
static void rewriteFormulaRefs(const char* src, int ins_row, int ins_col, char* out, size_t cap)
{
	size_t o = 0;
	const char* s = src;
	while (*s != '\0')
	{
		if (isAlphaC(*s))
		{
			//试解析地址:字母串 + 数字串
			const char* p = s;
			int col = 0;
			while (isAlphaC(*p)) { col = col * 26 + (upC(*p) - 'A' + 1); p++; }
			if (isDigitC(*p))
			{
				int row = 0;
				while (isDigitC(*p)) { row = row * 10 + (*p - '0'); p++; }
				int c0 = col - 1, r0 = row - 1;//1 基 → 0 基
				if (ins_col >= 0 && c0 >= ins_col) c0++;
				if (ins_row >= 0 && r0 >= ins_row) r0++;
				char addr[12];
				cellAddr(r0, c0, addr, sizeof(addr));
				for (size_t i = 0; addr[i] != '\0' && o + 1 < cap; i++) out[o++] = addr[i];
				s = p;
				continue;
			}
			//非地址(函数名/裸字母):原样复制这段字母
			while (s < p && o + 1 < cap) out[o++] = *s++;
			continue;
		}
		if (o + 1 < cap) out[o++] = *s;
		s++;
	}
	out[o] = '\0';
}

/**
  * @brief 在第 at 行前插入一行:at 及以下行整体下移,末行丢弃,新行清空;
  *        所有公式引用行 >= at 的 +1。之后重算 + 刷显示。
  */
static void insertRowAt(int at)
{
	if (at < 0 || at >= G_ROWS) return;
	for (int r = G_ROWS - 1; r > at; r--)
		for (int c = 0; c < G_COLS; c++)
			g_cells[r][c] = g_cells[r - 1][c];//结构体整体拷贝(含 raw)
	for (int c = 0; c < G_COLS; c++)
		setCellRaw(at, c, "");
	//引用调整(遍历全表公式)
	for (int r = 0; r < G_ROWS; r++)
		for (int c = 0; c < G_COLS; c++)
			if (g_cells[r][c].kind == KIND_FORMULA)
			{
				char nb[CELL_RAW_MAX];
				rewriteFormulaRefs(g_cells[r][c].raw, at, -1, nb, sizeof(nb));
				setCellRaw(r, c, nb);
			}
	if (g_grid != NULL) YMGUI_Grid_InsertRow(g_grid, (uint16)at);//让库搬合并区/行高
	recalcAll();
	refreshDisplay();
}

/**
  * @brief 在第 at 列前插入一列:at 及右侧列整体右移,末列丢弃,新列清空;
  *        所有公式引用列 >= at 的 +1。之后重算 + 刷显示。
  */
static void insertColAt(int at)
{
	if (at < 0 || at >= G_COLS) return;
	for (int c = G_COLS - 1; c > at; c--)
		for (int r = 0; r < G_ROWS; r++)
			g_cells[r][c] = g_cells[r][c - 1];
	for (int r = 0; r < G_ROWS; r++)
		setCellRaw(r, at, "");
	for (int r = 0; r < G_ROWS; r++)
		for (int c = 0; c < G_COLS; c++)
			if (g_cells[r][c].kind == KIND_FORMULA)
			{
				char nb[CELL_RAW_MAX];
				rewriteFormulaRefs(g_cells[r][c].raw, -1, at, nb, sizeof(nb));
				setCellRaw(r, c, nb);
			}
	if (g_grid != NULL) YMGUI_Grid_InsertCol(g_grid, (uint16)at);//让库搬合并区/列宽
	recalcAll();
	refreshDisplay();
}

/**
  * @brief 重写公式串里的地址引用:删行/列后调整。引用正落在被删行(row==del_row)或被删列
  *        (col==del_col)→ 该地址替换成 "#REF!";在其后的(row>del_row / col>del_col)-1。
  *        del_row<0 不处理行;del_col<0 不处理列。函数名原样保留。
  */
static void rewriteFormulaRefsDelete(const char* src, int del_row, int del_col, char* out, size_t cap)
{
	size_t o = 0;
	const char* s = src;
	while (*s != '\0')
	{
		if (isAlphaC(*s))
		{
			const char* p = s;
			int col = 0;
			while (isAlphaC(*p)) { col = col * 26 + (upC(*p) - 'A' + 1); p++; }
			if (isDigitC(*p))
			{
				int row = 0;
				while (isDigitC(*p)) { row = row * 10 + (*p - '0'); p++; }
				int c0 = col - 1, r0 = row - 1;//1 基 → 0 基
				uint8 gone = ((del_col >= 0 && c0 == del_col) ||
				              (del_row >= 0 && r0 == del_row));
				if (gone)
				{
					const char* ref = "#REF!";
					for (size_t i = 0; ref[i] != '\0' && o + 1 < cap; i++) out[o++] = ref[i];
				}
				else
				{
					if (del_col >= 0 && c0 > del_col) c0--;
					if (del_row >= 0 && r0 > del_row) r0--;
					char addr[12];
					cellAddr(r0, c0, addr, sizeof(addr));
					for (size_t i = 0; addr[i] != '\0' && o + 1 < cap; i++) out[o++] = addr[i];
				}
				s = p;
				continue;
			}
			while (s < p && o + 1 < cap) out[o++] = *s++;
			continue;
		}
		if (o + 1 < cap) out[o++] = *s;
		s++;
	}
	out[o] = '\0';
}

/**
  * @brief 删第 at 行:at 以下行整体上移,末行清空;所有公式引用第 at 行 → #REF!,更后的 -1。
  */
static void deleteRowAt(int at)
{
	if (at < 0 || at >= G_ROWS) return;
	for (int r = at; r < G_ROWS - 1; r++)
		for (int c = 0; c < G_COLS; c++)
			g_cells[r][c] = g_cells[r + 1][c];
	for (int c = 0; c < G_COLS; c++)
		setCellRaw(G_ROWS - 1, c, "");
	for (int r = 0; r < G_ROWS; r++)
		for (int c = 0; c < G_COLS; c++)
			if (g_cells[r][c].kind == KIND_FORMULA)
			{
				char nb[CELL_RAW_MAX];
				rewriteFormulaRefsDelete(g_cells[r][c].raw, at, -1, nb, sizeof(nb));
				setCellRaw(r, c, nb);
			}
	if (g_grid != NULL) YMGUI_Grid_DeleteRow(g_grid, (uint16)at);
	recalcAll();
	refreshDisplay();
}

/**
  * @brief 删第 at 列:at 右侧列整体左移,末列清空;所有公式引用第 at 列 → #REF!,更右的 -1。
  */
static void deleteColAt(int at)
{
	if (at < 0 || at >= G_COLS) return;
	for (int c = at; c < G_COLS - 1; c++)
		for (int r = 0; r < G_ROWS; r++)
			g_cells[r][c] = g_cells[r][c + 1];
	for (int r = 0; r < G_ROWS; r++)
		setCellRaw(r, G_COLS - 1, "");
	for (int r = 0; r < G_ROWS; r++)
		for (int c = 0; c < G_COLS; c++)
			if (g_cells[r][c].kind == KIND_FORMULA)
			{
				char nb[CELL_RAW_MAX];
				rewriteFormulaRefsDelete(g_cells[r][c].raw, -1, at, nb, sizeof(nb));
				setCellRaw(r, c, nb);
			}
	if (g_grid != NULL) YMGUI_Grid_DeleteCol(g_grid, (uint16)at);
	recalcAll();
	refreshDisplay();
}

/**
  * @brief 全表重算:清记忆化标志,逐格 evalCell(公式递归自会带出依赖)。
  */
static void recalcAll(void)
{
	for (int r = 0; r < G_ROWS; r++)
		for (int c = 0; c < G_COLS; c++)
		{
			g_cells[r][c].computed = 0;
			g_cells[r][c].visiting = 0;
			if (g_cells[r][c].kind == KIND_FORMULA)
				g_cells[r][c].error = ERR_NONE;//公式错每轮重判(数字/文本无错)
		}
	for (int r = 0; r < G_ROWS; r++)
		for (int c = 0; c < G_COLS; c++)
			evalCell(r, c);
}

//错误码 → 显示串
static const char* errStr(uint8 e)
{
	switch (e)
	{
	case ERR_REF:  return "#REF!";
	case ERR_DIV0: return "#DIV0!";
	case ERR_SYNTAX: return "#ERR!";
	default: return "";
	}
}

/**
  * @brief 把每格计算结果格式化后灌进 Grid 显示串:错误→错误串;数字/公式→定点 2 位;文本→原文。
  */
static void refreshDisplay(void)
{
	for (int r = 0; r < G_ROWS; r++)
		for (int c = 0; c < G_COLS; c++)
		{
			Cell* cell = &g_cells[r][c];
			char disp[CELL_RAW_MAX];
			if (cell->kind == KIND_EMPTY)
				disp[0] = '\0';
			else if (cell->error != ERR_NONE)
				snprintf(disp, sizeof(disp), "%s", errStr(cell->error));
			else if (cell->kind == KIND_TEXT)
				snprintf(disp, sizeof(disp), "%s", cell->raw);
			else
				formatValue(cell->value, disp, sizeof(disp));
			YMGUI_Grid_SetCellText(g_grid, (uint16)r, (uint16)c, disp);
		}
}

//(r,c) → "A1" 地址串
static void cellAddr(int r, int c, char* out, size_t cap)
{
	if (cap < 4) { if (cap) out[0] = '\0'; return; }
	size_t i = 0;
	if (c < 26) out[i++] = (char)('A' + c);
	else { out[i++] = (char)('A' + c / 26 - 1); out[i++] = (char)('A' + c % 26); }
	//行号(1 基)
	char tmp[8]; int n = 0; uint32 v = (uint32)r + 1;
	if (v == 0) tmp[n++] = '0';
	while (v > 0 && n < (int)sizeof(tmp)) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
	while (n > 0 && i + 1 < cap) out[i++] = tmp[--n];
	out[i] = '\0';
}

//===========================================================================
// UI:提交、状态栏、选中/编辑回调
//===========================================================================

//更新地址标签 + 状态栏(显示选中格地址 + 值/错误/原文)
static void updateStatus(void)
{
	if (g_sel_r < 0 || g_sel_c < 0)
	{
		YMGUI_Label_SetText(g_addr_lbl, "--");
		YMGUI_Label_SetText(g_status, "Ready");
		return;
	}
	char addr[8];
	cellAddr(g_sel_r, g_sel_c, addr, sizeof(addr));
	YMGUI_Label_SetText(g_addr_lbl, addr);
	Cell* cell = &g_cells[g_sel_r][g_sel_c];
	char msg[64];
	if (cell->kind == KIND_EMPTY)
		snprintf(msg, sizeof(msg), "%s: (empty)", addr);
	else if (cell->error != ERR_NONE)
		snprintf(msg, sizeof(msg), "%s = %s", addr, errStr(cell->error));
	else if (cell->kind == KIND_TEXT)
		snprintf(msg, sizeof(msg), "%s: %s", addr, cell->raw);
	else
	{
		char val[24];
		formatValue(cell->value, val, sizeof(val));
		snprintf(msg, sizeof(msg), "%s = %s", addr, val);
	}
	YMGUI_Label_SetText(g_status, msg);
}

//提交某格原文 → 重算全表 → 刷显示 + 状态栏
static void commitCell(int r, int c, const char* raw)
{
	setCellRaw(r, c, raw);
	recalcAll();
	refreshDisplay();
	updateStatus();
}

//Grid 单击选中格:公式栏显示该格原文,更新地址/状态,隐藏就地编辑框
static void onGridSelect(GYOBJ grid, uint16 row, uint16 col)
{
	(void)grid;
	g_sel_r = row;
	g_sel_c = col;
	g_suppress = 1;
	YMGUI_TextInput_SetText(g_fx, g_cells[row][col].raw);//SetText 不触发 changed,防御性抑制
	g_suppress = 0;
	YMGUI_Obj_SetHidden(g_editor, 1);
	updateStatus();
}

//Grid 双击/回车请求编辑:把就地编辑框叠到该格上,载入原文并聚焦
static void onGridEdit(GYOBJ grid, uint16 row, uint16 col)
{
	g_sel_r = row;
	g_sel_c = col;
	GYrect cr;
	if (!YMGUI_Grid_GetCellRect(grid, row, col, &cr))
	{
		//格不可见(理论上刚选中不会发生):退化为聚焦公式栏
		YMGUI_SetFocus(g_ctx, g_fx);
		return;
	}
	//编辑框位置 = 格的屏幕矩形(相对 root;root 在 (0,0) 故绝对=相对)
	g_editor->area.x = cr.x;
	g_editor->area.y = cr.y;
	g_editor->area.w = (cr.w < 90) ? 90 : cr.w;
	g_editor->area.h = cr.h;
	g_suppress = 1;
	YMGUI_TextInput_SetText(g_editor, g_cells[row][col].raw);
	g_suppress = 0;
	YMGUI_Obj_SetHidden(g_editor, 0);
	YMGUI_SetFocus(g_ctx, g_editor);
	YMGUI_Obj_Invalidate(g_ctx->root);//整屏刷,清编辑框移动残影
}

//公式栏内容变化:逐键实时提交到当前选中格
static void onFxChanged(GYOBJ ti, const char* text)
{
	(void)ti;
	if (g_suppress) return;
	if (g_sel_r < 0 || g_sel_c < 0) return;
	commitCell(g_sel_r, g_sel_c, text);
}

//就地编辑框内容变化:提交到选中格,并同步公式栏
static void onEditorChanged(GYOBJ ti, const char* text)
{
	if (g_suppress) return;
	if (g_sel_r < 0 || g_sel_c < 0) return;
	commitCell(g_sel_r, g_sel_c, text);
	g_suppress = 1;
	YMGUI_TextInput_SetText(g_fx, text);
	g_suppress = 0;
}

//===========================================================================
// 工具栏按钮:插入行/列、合并/取消合并、对齐、列宽/行高
//===========================================================================

//刷新公式栏 + 状态栏到当前选中格(插入/编辑后同步显示)
static void syncFxToSel(void)
{
	if (g_sel_r < 0 || g_sel_c < 0) return;
	g_suppress = 1;
	YMGUI_TextInput_SetText(g_fx, g_cells[g_sel_r][g_sel_c].raw);
	g_suppress = 0;
	updateStatus();
}

static void onInsertRow(GYOBJ btn)
{
	(void)btn;
	int at = (g_sel_r >= 0) ? g_sel_r : 0;
	insertRowAt(at);
	syncFxToSel();
	YMGUI_Obj_Invalidate(g_ctx->root);
}

static void onInsertCol(GYOBJ btn)
{
	(void)btn;
	int at = (g_sel_c >= 0) ? g_sel_c : 0;
	insertColAt(at);
	syncFxToSel();
	YMGUI_Obj_Invalidate(g_ctx->root);
}

static void onMerge(GYOBJ btn)
{
	(void)btn;
	int32 r0, c0, r1, c1;
	YMGUI_Grid_GetSelectedRange(g_grid, &r0, &c0, &r1, &c1);
	if (r0 < 0) return;
	YMGUI_Grid_MergeCells(g_grid, r0, c0, r1, c1);
	YMGUI_Obj_Invalidate(g_ctx->root);
}

static void onUnmerge(GYOBJ btn)
{
	(void)btn;
	if (g_sel_r < 0 || g_sel_c < 0) return;
	YMGUI_Grid_UnmergeAt(g_grid, (uint16)g_sel_r, (uint16)g_sel_c);
	YMGUI_Obj_Invalidate(g_ctx->root);
}

//对选区每格设对齐(align: GY_ALIGN_*)
static void applyAlign(uint8 align)
{
	int32 r0, c0, r1, c1;
	YMGUI_Grid_GetSelectedRange(g_grid, &r0, &c0, &r1, &c1);
	if (r0 < 0) return;
	for (int32 r = r0; r <= r1; r++)
		for (int32 c = c0; c <= c1; c++)
			YMGUI_Grid_SetCellAlign(g_grid, (uint16)r, (uint16)c, align);
	YMGUI_Obj_Invalidate(g_ctx->root);
}
static void onAlignLeft(GYOBJ btn)   { (void)btn; applyAlign(GY_ALIGN_LEFT); }
static void onAlignCenter(GYOBJ btn) { (void)btn; applyAlign(GY_ALIGN_CENTER); }
static void onAlignRight(GYOBJ btn)  { (void)btn; applyAlign(GY_ALIGN_RIGHT); }

//删行/列(对活动行/列)
static void onDeleteRow(GYOBJ btn)
{
	(void)btn;
	if (g_sel_r < 0) return;
	deleteRowAt(g_sel_r);
	syncFxToSel();
	YMGUI_Obj_Invalidate(g_ctx->root);
}
static void onDeleteCol(GYOBJ btn)
{
	(void)btn;
	if (g_sel_c < 0) return;
	deleteColAt(g_sel_c);
	syncFxToSel();
	YMGUI_Obj_Invalidate(g_ctx->root);
}

//列宽/行高步进(对活动列/行,尺寸从库读,库为唯一真源)
#define DIM_STEP 12
#define DIM_STEP_H 6
static void onColWider(GYOBJ btn)
{
	(void)btn;
	if (g_sel_c < 0) return;
	GYcoord w = YMGUI_Grid_GetColWidth(g_grid, (uint16)g_sel_c);
	YMGUI_Grid_SetColWidth(g_grid, (uint16)g_sel_c, w + DIM_STEP);
	YMGUI_Obj_Invalidate(g_ctx->root);
}
static void onColNarrower(GYOBJ btn)
{
	(void)btn;
	if (g_sel_c < 0) return;
	GYcoord w = YMGUI_Grid_GetColWidth(g_grid, (uint16)g_sel_c);
	if (w - DIM_STEP >= 24) YMGUI_Grid_SetColWidth(g_grid, (uint16)g_sel_c, w - DIM_STEP);
	YMGUI_Obj_Invalidate(g_ctx->root);
}
static void onRowTaller(GYOBJ btn)
{
	(void)btn;
	if (g_sel_r < 0) return;//只调活动行(修:原先全局设,所有行一起变高)
	GYcoord h = YMGUI_Grid_GetRowHeightAt(g_grid, (uint16)g_sel_r);
	YMGUI_Grid_SetRowHeightAt(g_grid, (uint16)g_sel_r, h + DIM_STEP_H);
	YMGUI_Obj_Invalidate(g_ctx->root);
}
static void onRowShorter(GYOBJ btn)
{
	(void)btn;
	if (g_sel_r < 0) return;
	GYcoord h = YMGUI_Grid_GetRowHeightAt(g_grid, (uint16)g_sel_r);
	if (h - DIM_STEP_H >= 14) YMGUI_Grid_SetRowHeightAt(g_grid, (uint16)g_sel_r, h - DIM_STEP_H);
	YMGUI_Obj_Invalidate(g_ctx->root);
}

//===========================================================================
// 预置示例数据(窗口演示 + selftest 共用)
//===========================================================================
static void loadSample(void)
{
	setCellRaw(0, 0, "10");           //A1 = 10
	setCellRaw(0, 1, "20");           //B1 = 20
	setCellRaw(0, 2, "=A1+B1");       //C1 → 30.00
	setCellRaw(1, 2, "=SUM(A1:B1)");  //C2 → 30.00
	setCellRaw(2, 2, "=AVG(A1:B1)*2");//C3 → 30.00
	setCellRaw(0, 3, "=A1/0");        //D1 → #DIV0!
	setCellRaw(0, 4, "=E1");          //E1 自引 → 循环 #REF!
	setCellRaw(0, 5, "=G1");          //F1 ┐互引循环
	setCellRaw(0, 6, "=F1");          //G1 ┘→ #REF!
	setCellRaw(3, 0, "hello");        //A4 文本
	setCellRaw(0, 7, "=A1*2+B1/2");   //H1 → 30.00
	setCellRaw(1, 0, "3.14");         //A2 小数
	setCellRaw(1, 1, "=A2*2");        //B2 → 6.28
	recalcAll();
}

//===========================================================================
// headless 自检:断言预置数据的计算/格式化/错误正确
//===========================================================================
#define FMT_EQ(r, c, want) do {                                   \
	char _b[24]; formatValue(g_cells[r][c].value, _b, sizeof(_b)); \
	if (strcmp(_b, want) != 0) {                                  \
		gy_log_print("selftest FAIL @(%d,%d): got %s want %s\n", r, c, _b, want); \
		fails++;                                                  \
	} } while (0)
#define ERR_EQ(r, c, want) do {                                   \
	if (g_cells[r][c].error != (want)) {                          \
		gy_log_print("selftest FAIL @(%d,%d): err=%d want %d\n", r, c, g_cells[r][c].error, want); \
		fails++;                                                  \
	} } while (0)

static void selftest(void)
{
	int fails = 0;
	//预置数据已在 loadSample 里 recalc 过
	FMT_EQ(0, 2, "30.00");  //C1 = A1+B1
	FMT_EQ(1, 2, "30.00");  //C2 = SUM(A1:B1)
	FMT_EQ(2, 2, "30.00");  //C3 = AVG(A1:B1)*2
	ERR_EQ(0, 3, ERR_DIV0); //D1 = A1/0
	ERR_EQ(0, 4, ERR_REF);  //E1 = E1 自引
	ERR_EQ(0, 5, ERR_REF);  //F1 = G1 (互引循环)
	ERR_EQ(0, 6, ERR_REF);  //G1 = F1
	FMT_EQ(0, 7, "30.00");  //H1 = A1*2+B1/2 = 20+10
	FMT_EQ(1, 1, "6.28");   //B2 = A2*2 = 3.14*2

	//运行时编辑一格并重算,验证提交链路
	commitCell(4, 4, "=A1+A2");//E5 = 10 + 3.14 = 13.14
	FMT_EQ(4, 4, "13.14");
	commitCell(5, 0, "=SUM(A1:A2)*10");//A6 = (10+3.14)*10 = 131.40
	FMT_EQ(5, 0, "131.40");
	commitCell(0, 3, "=A1/2");//D1 改成合法式 → 清 #DIV0!
	FMT_EQ(0, 3, "5.00");
	ERR_EQ(0, 3, ERR_NONE);

	//---- 插入行 + 引用自动调整 ----
	//清一小片区,布置已知数据:A10=5, A11=7, B11==A10+A11 (=12)
	commitCell(9, 0, "5");
	commitCell(10, 0, "7");
	commitCell(10, 1, "=A10+A11");
	FMT_EQ(10, 1, "12.00");
	//在第 10 行(0 基 9,即 A10 行)前插入一行:原 A10→A11、原 A11→A12、公式 B11→B12 且引用 +1
	insertRowAt(9);
	//原 (9,0)=5 现应在 (10,0);原 (10,1) 公式现在 (11,1) 且引用变 =A11+A12,值不变 12
	if (strcmp(g_cells[10][0].raw, "5") != 0)
		{ gy_log_print("selftest FAIL: insertRow shift raw=%s want 5\n", g_cells[10][0].raw); fails++; }
	if (strcmp(g_cells[11][1].raw, "=A11+A12") != 0)
		{ gy_log_print("selftest FAIL: insertRow ref rewrite raw=%s want =A11+A12\n", g_cells[11][1].raw); fails++; }
	FMT_EQ(11, 1, "12.00");//值不变
	//插入的新空行
	if (g_cells[9][0].kind != KIND_EMPTY)
		{ gy_log_print("selftest FAIL: inserted row not empty\n"); fails++; }

	//---- 插入列 + 引用自动调整 ----
	//布置:C15=3, D15=4, E15==C15+D15 (=7)
	commitCell(14, 2, "3");
	commitCell(14, 3, "4");
	commitCell(14, 4, "=C15+D15");
	FMT_EQ(14, 4, "7.00");
	//在第 C 列(0 基 2)前插入一列:C15→D15、D15→E15、公式 E15→F15 且列引用 +1 → =D15+E15
	insertColAt(2);
	if (strcmp(g_cells[14][3].raw, "3") != 0)
		{ gy_log_print("selftest FAIL: insertCol shift raw=%s want 3\n", g_cells[14][3].raw); fails++; }
	if (strcmp(g_cells[14][5].raw, "=D15+E15") != 0)
		{ gy_log_print("selftest FAIL: insertCol ref rewrite raw=%s want =D15+E15\n", g_cells[14][5].raw); fails++; }
	FMT_EQ(14, 5, "7.00");//值不变

	//---- SUM 区间两端点也随插入调整 ----
	commitCell(20, 0, "=SUM(A22:A24)");//A21 引用 A22:A24
	commitCell(21, 0, "1"); commitCell(22, 0, "2"); commitCell(23, 0, "10");
	FMT_EQ(20, 0, "13.00");
	insertRowAt(21);//在 A22 前插一行 → 区间两端点 +1 → SUM(A23:A25),数据也下移,值不变
	if (strcmp(g_cells[20][0].raw, "=SUM(A23:A25)") != 0)
		{ gy_log_print("selftest FAIL: SUM range rewrite raw=%s want =SUM(A23:A25)\n", g_cells[20][0].raw); fails++; }
	FMT_EQ(20, 0, "13.00");

	//---- 删行 + 引用调整(引用被删行 → #REF!,更后引用 -1) ----
	//布置:A30=100, A31=200, B30==A31(引用下一行), B31==A30(引用被删行本身)
	//(行号须 < G_ROWS=30;此处用 25/26 行,0 基,均在界内)
	commitCell(25, 0, "100");         //A26 = 100(将被删的行)
	commitCell(26, 0, "200");         //A27 = 200
	commitCell(25, 1, "=A27");        //B26 引用 A27(第 27 行,0 基 26)
	commitCell(26, 1, "=A26");        //B27 引用 A26(第 26 行,0 基 25,将被删)
	FMT_EQ(25, 1, "200.00");          //B26 = A27 = 200
	deleteRowAt(25);//删第 26 行(0 基 25):A27→A26,引用第 26 行者变 #REF!,引用第 27 行者 -1→A26
	//原 (26,1)=B27 "=A26" 上移到 (25,1),其引用的 A26 正是被删行 → #REF!
	if (strcmp(g_cells[25][1].raw, "=#REF!") != 0)
		{ gy_log_print("selftest FAIL: deleteRow ref->REF raw=%s want =#REF!\n", g_cells[25][1].raw); fails++; }
	ERR_EQ(25, 1, ERR_REF);

	//---- 删列 + 引用调整(列号 < G_COLS=26;用 D/E 列,行 27 在界内)----
	commitCell(27, 3, "7");           //D28 = 7(将被删的列)
	commitCell(27, 4, "=D28");        //E28 引用 D28
	deleteColAt(3);                   //删第 D 列(0 基 3):引用 D 列者 → #REF!
	//原 E28 "=D28" 左移到 D28(0 基 3),引用被删列 → #REF!
	if (strcmp(g_cells[27][3].raw, "=#REF!") != 0)
		{ gy_log_print("selftest FAIL: deleteCol ref->REF raw=%s want =#REF!\n", g_cells[27][3].raw); fails++; }
	ERR_EQ(27, 3, ERR_REF);

	if (fails == 0)
		gy_log_print("selftest: formula eval OK\n");
	else
	{
		gy_log_print("selftest: %d FAILED\n", fails);
		g_selftest_fail = 1;
	}
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

	SDL_LCD_Init(&disp, 1);
	g_ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	YMGUI_Obj_SetBgColor(g_ctx->root, GY_ARGB(0xFF, 0x0E, 0x0E, 0x12));

	//GB2312 全字库回退
	s_blob = fopen(GB2312_BIN_PATH, "rb");
	if (s_blob != NULL)
	{
		s_gb_font.glyph_count = YMGUI_GB2312_glyph_count;
		YMGUI_Font_SetFallback(&s_gb_font);
	}
	else
		gy_log_print("warn: gb2312 blob not found, CJK limited to built-in glyphs\n");

	//---- 顶部公式栏:地址标签 + fx: + 输入框 ----
	g_addr_lbl = YMGUI_Creat_Label_Creat(g_ctx->root, 8, 8, 52, 18);
	YMGUI_Label_SetTextColor(g_addr_lbl, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));
	YMGUI_Label_SetText(g_addr_lbl, "--");
	GYOBJ fxlbl = YMGUI_Creat_Label_Creat(g_ctx->root, 64, 8, 24, 18);
	YMGUI_Label_SetTextColor(fxlbl, GY_ARGB(0xFF, 0xC0, 0xC0, 0xC8));
	YMGUI_Label_SetText(fxlbl, "fx:");
	g_fx = YMGUI_Creat_TextInput_Creat(g_ctx->root, 92, 6, SCR_W - 100, 24);
	YMGUI_TextInput_SetChanged(g_fx, onFxChanged);

	//---- 第二排工具栏:插入/合并/对齐/尺寸按钮 ----
	{
		GYcoord bx = 8, by = TOOLBAR_H + 2, bh = 24, gap = 4;
		struct { const char* t; GYcoord w; GYbtn_clicked_cb cb; } defs[] = {
			{ "插入行", 56, onInsertRow }, { "插入列", 56, onInsertCol },
			{ "删行", 44, onDeleteRow }, { "删列", 44, onDeleteCol },
			{ "合并",   44, onMerge },     { "取消合并", 68, onUnmerge },
			{ "左", 32, onAlignLeft }, { "中", 32, onAlignCenter }, { "右", 32, onAlignRight },
			{ "列宽+", 52, onColWider }, { "列宽-", 52, onColNarrower },
			{ "行高+", 52, onRowTaller }, { "行高-", 52, onRowShorter },
		};
		for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++)
		{
			GYOBJ b = YMGUI_Creat_Button_Creat(g_ctx->root, bx, by, defs[i].w, bh);
			YMGUI_Button_SetText(b, defs[i].t);
			YMGUI_Button_SetClicked(b, defs[i].cb);
			bx += defs[i].w + gap;
		}
	}

	//---- 中部 Grid ----
	GYcoord grid_y = TOOLBAR_H + TOOLBAR2_H + 2;
	g_grid = YMGUI_Creat_Grid_Creat(g_ctx->root, 8, grid_y, SCR_W - 16,
	                                SCR_H - grid_y - STATUS_H - 4, G_ROWS, G_COLS);
	YMGUI_Grid_SetSelectCb(g_grid, onGridSelect);
	YMGUI_Grid_SetEditCb(g_grid, onGridEdit);

	//---- 就地编辑框(浮层,平时隐藏)----
	g_editor = YMGUI_Creat_TextInput_Creat(g_ctx->root, 0, 0, 90, 22);
	YMGUI_TextInput_SetChanged(g_editor, onEditorChanged);
	YMGUI_Obj_SetHidden(g_editor, 1);

	//---- 底部状态栏 ----
	g_status = YMGUI_Creat_Label_Creat(g_ctx->root, 8, SCR_H - STATUS_H, SCR_W - 16, 16);
	YMGUI_Label_SetTextColor(g_status, GY_ARGB(0xFF, 0xA0, 0xE0, 0xA0));
	YMGUI_Label_SetText(g_status, "Ready");

	//预置示例数据 + 首刷
	loadSample();
	refreshDisplay();
	YMGUI_Grid_SetSelected(g_grid, 0, 0);
	g_sel_r = 0; g_sel_c = 0;
	g_suppress = 1;
	YMGUI_TextInput_SetText(g_fx, g_cells[0][0].raw);
	g_suppress = 0;
	updateStatus();

	YMGUI_Inject_SetCtx(g_ctx);

	if (max_frames > 0)
		selftest();

	while (SDL_LCD_PumpEvents())
	{
		YMGUI_Refresh(g_ctx);
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}

	YMGUI_Free_CtxFree(g_ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	if (s_blob != NULL) fclose(s_blob);
	gy_log_print("excel_edit exit ok\n");
	return g_selftest_fail ? 1 : 0;
}
