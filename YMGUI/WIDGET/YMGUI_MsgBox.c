#include "YMGUI_MsgBox.h"

#if YMGUI_MSGBOX

#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Button.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_MsgBox.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-05
  *	@Description: 模态对话框。全屏 backdrop 挂 top_layer(渲染最上/命中最先),吞掉卡片外点击 → 输入模态锁死底层;
  *	              点卡片按钮才隐藏解锁。居中卡片含标题 + 多行正文 + 底部 N 个按钮(各带回调)。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息:
  * 1.句柄(返回给用户的 GYOBJ)就是 backdrop 本身;card 是它的子、按钮是 card 的子,构成一棵子树。
  *   整棵挂 top_layer,在 root 子树里无所有者 → 析构由 YMGUI_Free_CtxFree 的 top_layer 兜底级联释放,
  *   不存在 Dropdown 那种"所有者二次释放"(那是所有者在 root、弹出层在 top_layer 分属两处才有的坑)。
  * 2.生命周期沿用 Dropdown 铁律:关闭只 SetHidden(不释放 —— 按钮回调仍在其事件派发中,释放=use-after-free);
  *   Show() 在安全上下文(不在事件调用栈)里"先拆旧 card 子树再重建",故可反复 Show/Hide 复用不泄漏。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define MB_PAD        16  //卡片内边距
#define MB_TITLE_GAP  10  //标题与正文间距
#define MB_BTN_H      34  //按钮高
#define MB_BTN_GAP    10  //按钮间距 / 正文与按钮间距
#define MB_BTN_MINW   72  //按钮最小宽
#define MB_LINE_EXTRA 4   //正文行间额外行距
#define MB_MIN_CARDW  200 //卡片最小宽

//单个按钮记录(label 深拷,cb 用户回调)
typedef struct
{
	char            label[GY_BTN_TEXT_MAX];
	GYmsgbox_btn_cb cb;
	GYOBJ           btn;   //Show 时建的按钮对象(隐藏/重建间可为陈旧,仅在 rebuild 时用)
}GYmb_btn;

//对话框私有数据(挂 backdrop->user_data)
typedef struct
{
	char     title[GY_MSGBOX_TITLE_MAX];
	char     text[GY_MSGBOX_TEXT_MAX];
	GYmb_btn btns[GY_MSGBOX_MAX_BTN];
	uint8    btn_count;
	GYOBJ    card;          //居中卡片(backdrop 的子);NULL=未建
	GYcolor  c_backdrop;    //遮罩色(含 alpha)
	GYcolor  c_card;        //卡片底
	GYcolor  c_border;      //卡片边框
	GYcolor  c_title;       //标题字
	GYcolor  c_text;        //正文字
}GYmb_data;

//======================== 遮罩(模态核心)========================
/**
  * @brief 遮罩绘制:半透明填满全屏,让底层 UI 变暗(alpha 由 c_backdrop 高字节决定)
  */
static void backdropDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYmb_data* d = (GYmb_data*)obj->user_data;
	if (d == NULL)
		return;
	GYopa opa = (GYopa)((d->c_backdrop >> 24) & 0xFF);
	if (opa == GY_OPA_TRANSP)
		return;//全透明:不画(仍吞点击,只是不变暗)
	YMGUI_Draw_Fill(s, abs, d->c_backdrop, opa);
}

/**
  * @brief 遮罩事件:吞掉一切落在卡片外的点击(什么都不做)。
  *        这是"输入模态"的关键 —— HitTest 优先测 top_layer,点卡片外命中遮罩,
  *        事件到此为止,不会穿透到底层 root 的控件 → 底层被锁死。
  */
static void backdropEventCb(GYOBJ obj, GYEvent e)
{
	(void)obj;
	(void)e;
	//故意为空:模态期间外部点击无效,必须点卡片按钮才能关闭
}

//======================== 卡片 ========================
/**
  * @brief 计算正文行数(按 '\n' 断行)
  */
static int textLineCount(const char* t)
{
	if (t == NULL || t[0] == '\0')
		return 0;
	int n = 1;
	for (const char* p = t; *p != '\0'; p++)
		if (*p == '\n')
			n++;
	return n;
}

/**
  * @brief 卡片绘制:不透明底 + 1px 边框 + 标题 + 逐行正文
  */
static void cardDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYOBJ mb = obj->parent;//card 的父就是 backdrop(句柄)
	GYmb_data* d = (mb != NULL) ? (GYmb_data*)mb->user_data : NULL;
	if (d == NULL)
		return;

	YMGUI_Draw_Fill(s, abs, d->c_card, GY_OPA_COVER);
	//边框
	GYrect top = {abs->x, abs->y, abs->w, 1};
	GYrect bot = {abs->x, abs->y + abs->h - 1, abs->w, 1};
	GYrect lft = {abs->x, abs->y, 1, abs->h};
	GYrect rgt = {abs->x + abs->w - 1, abs->y, 1, abs->h};
	YMGUI_Draw_Fill(s, &top, d->c_border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &bot, d->c_border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &lft, d->c_border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &rgt, d->c_border, GY_OPA_COVER);

	GYFONT font = &YMGUI_Font_Default;
	GYcoord ty = abs->y + MB_PAD;

	//标题(居中)
	if (d->title[0] != '\0')
	{
		GYcoord tw = YMGUI_Font_TextWidth(font, d->title);
		GYcoord tx = abs->x + ((abs->w > tw) ? (abs->w - tw) / 2 : 0);
		YMGUI_Draw_Text(s, font, tx, ty, d->title, d->c_title);
		ty += font->cell_h + MB_TITLE_GAP;
	}

	//正文(逐行,居中),按 '\n' 断
	if (d->text[0] != '\0')
	{
		const char* line = d->text;
		while (line != NULL)
		{
			const char* nl = line;
			while (*nl != '\0' && *nl != '\n')
				nl++;
			uint32 nbytes = (uint32)(nl - line);
			GYcoord lw = YMGUI_Font_TextWidthN(font, line, nbytes);
			GYcoord lx = abs->x + ((abs->w > lw) ? (abs->w - lw) / 2 : 0);
			YMGUI_Draw_TextN(s, font, lx, ty, line, nbytes, d->c_text);
			ty += font->cell_h + MB_LINE_EXTRA;
			if (*nl == '\0')
				break;
			line = nl + 1;
		}
	}
}

//======================== 按钮回调转接 ========================
/**
  * @brief 按钮点击转接:从按钮回溯到 backdrop(btn->parent=card, card->parent=backdrop),
  *        扫按钮数组定位下标 → 调用户回调 → 隐藏模态(只 SetHidden,不释放:本回调仍在按钮事件派发中)
  */
static void mbBtnClickedCb(GYOBJ btn)
{
	GYOBJ card = (btn != NULL) ? btn->parent : NULL;
	GYOBJ mb   = (card != NULL) ? card->parent : NULL;
	if (mb == NULL || mb->user_data == NULL)
		return;
	GYmb_data* d = (GYmb_data*)mb->user_data;

	int idx = -1;
	for (uint8 i = 0; i < d->btn_count; i++)
		if (d->btns[i].btn == btn) { idx = (int)i; break; }

	GYmsgbox_btn_cb cb = (idx >= 0) ? d->btns[idx].cb : NULL;
	//先隐藏解锁(标脏底层),再调用户回调 —— 回调里若又 Show 本模态也安全(hidden 已置)
	YMGUI_Obj_SetHidden(mb, 1);
	if (cb != NULL)
		cb(mb, idx);
}

//======================== 卡片重建(Show 时,安全上下文)========================
/**
  * @brief 拆掉旧 card 子树(连同其按钮)。仅在 Show()/析构等安全上下文调用,不在事件回调里。
  */
static void teardownCard(GYmb_data* d)
{
	if (d->card != NULL)
	{
		YMGUI_Free_ObjFree(d->card);//级联释放卡片及其按钮子对象
		d->card = NULL;
	}
	for (uint8 i = 0; i < d->btn_count; i++)
		d->btns[i].btn = NULL;
}

/**
  * @brief 析构:释放私有数据。card 子树随对象树级联(ObjFree 先递归子)已释放,
  *        此处 d->card 可能已是野指针,故不碰它、只 free 数据本身。
  */
static void mbFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL)
	{
		GY_free0(obj->user_data);
		obj->user_data = NULL;
	}
}

/**
  * @brief 按当前标题/正文/按钮重建居中卡片(含按钮行),并把 backdrop 铺满全屏
  */
static void rebuildCard(GYOBJ mb)
{
	GYmb_data* d = (GYmb_data*)mb->user_data;
	teardownCard(d);

	GYCTX ctx = mb->ctx;
	GYDISP disp = (ctx != NULL) ? (GYDISP)ctx->disp : NULL;
	GYcoord scr_w = (disp != NULL) ? disp->hor_res : mb->area.w;
	GYcoord scr_h = (disp != NULL) ? disp->ver_res : mb->area.h;

	//backdrop 铺满全屏(尺寸可能随 disp 变;此处对齐 top_layer 尺寸)
	mb->area.x = 0; mb->area.y = 0;
	mb->area.w = scr_w; mb->area.h = scr_h;

	GYFONT font = &YMGUI_Font_Default;

	//---- 内容宽度:max(标题, 各正文行, 按钮行) ----
	GYcoord content_w = 0;
	if (d->title[0] != '\0')
	{
		GYcoord tw = YMGUI_Font_TextWidth(font, d->title);
		if (tw > content_w) content_w = tw;
	}
	//正文逐行测宽
	if (d->text[0] != '\0')
	{
		const char* line = d->text;
		while (line != NULL)
		{
			const char* nl = line;
			while (*nl != '\0' && *nl != '\n') nl++;
			GYcoord lw = YMGUI_Font_TextWidthN(font, line, (uint32)(nl - line));
			if (lw > content_w) content_w = lw;
			if (*nl == '\0') break;
			line = nl + 1;
		}
	}
	//按钮行宽:各按钮等宽(取最宽 label),横排
	GYcoord btn_w = MB_BTN_MINW;
	for (uint8 i = 0; i < d->btn_count; i++)
	{
		GYcoord lw = (GYcoord)(YMGUI_Font_TextWidth(font, d->btns[i].label) + 24);
		if (lw > btn_w) btn_w = lw;
	}
	GYcoord btn_row_w = 0;
	if (d->btn_count > 0)
		btn_row_w = (GYcoord)(d->btn_count * btn_w + (d->btn_count - 1) * MB_BTN_GAP);
	if (btn_row_w > content_w) content_w = btn_row_w;

	//---- 卡片尺寸 ----
	GYcoord card_w = (GYcoord)(content_w + MB_PAD * 2);
	if (card_w < MB_MIN_CARDW) card_w = MB_MIN_CARDW;
	if (card_w > scr_w) card_w = scr_w;

	GYcoord card_h = MB_PAD;
	if (d->title[0] != '\0')
		card_h += font->cell_h + MB_TITLE_GAP;
	int lines = textLineCount(d->text);
	if (lines > 0)
		card_h += (GYcoord)(lines * (font->cell_h + MB_LINE_EXTRA));
	if (d->btn_count > 0)
		card_h += MB_BTN_GAP + MB_BTN_H;
	card_h += MB_PAD;

	GYcoord card_x = (GYcoord)((scr_w - card_w) / 2);
	GYcoord card_y = (GYcoord)((scr_h - card_h) / 2);
	if (card_x < 0) card_x = 0;
	if (card_y < 0) card_y = 0;

	//---- 建卡片(backdrop 的子)----
	GYOBJ card = YMGUI_Creat_Obj_Creat(mb, card_x, card_y, card_w, card_h);
	if (card == NULL)
		return;
	card->draw_cb = cardDrawCb;
	card->event_cb = NULL;//卡片本体点击不做事(按钮才做),但仍挡住其下的 backdrop 命中
	card->free_cb = NULL;
	d->card = card;

	//---- 底部按钮行(card 的子,横向居中排布)----
	if (d->btn_count > 0)
	{
		GYcoord by = (GYcoord)(card_h - MB_PAD - MB_BTN_H);
		GYcoord bx = (GYcoord)((card_w - btn_row_w) / 2);
		for (uint8 i = 0; i < d->btn_count; i++)
		{
			GYOBJ b = YMGUI_Creat_Button_Creat(card, bx, by, btn_w, MB_BTN_H);
			if (b != NULL)
			{
				YMGUI_Button_SetText(b, d->btns[i].label);
				YMGUI_Button_SetClicked(b, mbBtnClickedCb);
				d->btns[i].btn = b;
			}
			bx = (GYcoord)(bx + btn_w + MB_BTN_GAP);
		}
	}
}

//======================== 公共 API ========================
/**
  * @brief 创建模态对话框(挂 top_layer,初始隐藏)
  */
GYOBJ YMGUI_Creat_MsgBox_Creat(GYCTX ctx)
{
	gy_assert(ctx);
	gy_log_explain(ctx == NULL, GY_LOG_PtrI, "上下文不存在");
	if (ctx == NULL)
		return NULL;
	GYOBJ top = YMGUI_Ctx_GetTopLayer(ctx);
	if (top == NULL)
		return NULL;

	//backdrop = 句柄,全屏挂 top_layer
	GYOBJ mb = YMGUI_Creat_Obj_Creat(top, 0, 0, top->area.w, top->area.h);
	gy_assert(mb);
	if (mb == NULL)
		return NULL;

	GYmb_data* d = (GYmb_data*)GY_malloc0(sizeof(GYmb_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "对话框数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(mb); return NULL; }
	GY_memset(d, 0, sizeof(GYmb_data));
	d->title[0] = '\0';
	d->text[0] = '\0';
	d->btn_count = 0;
	d->card = NULL;
	d->c_backdrop = GY_ARGB(0xA0, 0x00, 0x00, 0x00);//半透明黑,变暗底层
	d->c_card     = GY_ARGB(0xFF, 0x2A, 0x2A, 0x34);
	d->c_border   = GY_ARGB(0xFF, 0x50, 0x50, 0x5A);
	d->c_title    = GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
	d->c_text     = GY_ARGB(0xFF, 0xD0, 0xD0, 0xD8);

	mb->user_data = d;
	mb->draw_cb = backdropDrawCb;
	mb->event_cb = backdropEventCb;//吞点击(输入模态)
	mb->free_cb = mbFreeCb;        //释放私有数据(card 子树随对象树级联释放)

	//初始隐藏(Show 才显示)
	mb->state |= GY_STATE_Hidden;
	return mb;
}

/**
  * @brief 追加一个按钮(label 拷贝,cb 可为 NULL)。下次 Show 生效
  */
int YMGUI_MsgBox_AddButton(GYOBJ mb, const char* label, GYmsgbox_btn_cb cb)
{
	gy_assert(mb && mb->user_data && label);
	gy_log_explain((mb == NULL) || (mb->user_data == NULL) || (label == NULL), GY_LOG_PtrI, "对话框/数据/文本不存在");
	GYmb_data* d = (GYmb_data*)mb->user_data;
	if (d->btn_count >= GY_MSGBOX_MAX_BTN)
		return -1;
	uint8 slot = d->btn_count;
	uint16 i = 0;
	while (label[i] != '\0' && i < GY_BTN_TEXT_MAX - 1)
	{
		d->btns[slot].label[i] = label[i];
		i++;
	}
	d->btns[slot].label[i] = '\0';
	d->btns[slot].cb = cb;
	d->btns[slot].btn = NULL;
	d->btn_count++;
	return (int)slot;
}

/**
  * @brief 清空所有按钮(下次 Show 生效)
  */
void YMGUI_MsgBox_ClearButtons(GYOBJ mb)
{
	gy_assert(mb && mb->user_data);
	gy_log_explain((mb == NULL) || (mb->user_data == NULL), GY_LOG_PtrI, "对话框或数据不存在");
	((GYmb_data*)mb->user_data)->btn_count = 0;
}

/**
  * @brief 设标题(拷贝,截断)
  */
void YMGUI_MsgBox_SetTitle(GYOBJ mb, const char* title)
{
	gy_assert(mb && mb->user_data && title);
	gy_log_explain((mb == NULL) || (mb->user_data == NULL) || (title == NULL), GY_LOG_PtrI, "对话框/数据/文本不存在");
	GYmb_data* d = (GYmb_data*)mb->user_data;
	uint16 i = 0;
	while (title[i] != '\0' && i < GY_MSGBOX_TITLE_MAX - 1)
	{
		d->title[i] = title[i];
		i++;
	}
	d->title[i] = '\0';
}

/**
  * @brief 设正文(拷贝,'\n' 断多行,截断)
  */
void YMGUI_MsgBox_SetText(GYOBJ mb, const char* text)
{
	gy_assert(mb && mb->user_data && text);
	gy_log_explain((mb == NULL) || (mb->user_data == NULL) || (text == NULL), GY_LOG_PtrI, "对话框/数据/文本不存在");
	GYmb_data* d = (GYmb_data*)mb->user_data;
	uint16 i = 0;
	while (text[i] != '\0' && i < GY_MSGBOX_TEXT_MAX - 1)
	{
		d->text[i] = text[i];
		i++;
	}
	d->text[i] = '\0';
}

/**
  * @brief 显示模态:重建卡片内容并居中,置顶(先清隐藏位再重建再标脏 —— 标脏对 Hidden 对象直接返回)
  */
void YMGUI_MsgBox_Show(GYOBJ mb)
{
	gy_assert(mb && mb->user_data);
	gy_log_explain((mb == NULL) || (mb->user_data == NULL), GY_LOG_PtrI, "对话框或数据不存在");
	mb->state &= (uint8)~GY_STATE_Hidden;//先清隐藏位,后续 Invalidate 才生效
	rebuildCard(mb);
	YMGUI_Obj_Invalidate(mb);//全屏 backdrop 标脏 → 画遮罩 + 卡片
}

/**
  * @brief 主动关闭(只隐藏;顺序:先标脏露出底层,再置隐藏位 —— 因 Invalidate 对 Hidden 对象直接返回)
  */
void YMGUI_MsgBox_Close(GYOBJ mb)
{
	gy_assert(mb && mb->user_data);
	gy_log_explain((mb == NULL) || (mb->user_data == NULL), GY_LOG_PtrI, "对话框或数据不存在");
	YMGUI_Obj_SetHidden(mb, 1);//SetHidden 内部已处理"显示态先标脏再置位"的顺序
}

/**
  * @brief 当前是否显示
  */
uint8 YMGUI_MsgBox_IsShown(GYOBJ mb)
{
	gy_assert(mb && mb->user_data);
	gy_log_explain((mb == NULL) || (mb->user_data == NULL), GY_LOG_PtrI, "对话框或数据不存在");
	return (mb->state & GY_STATE_Hidden) ? 0 : 1;
}

/**
  * @brief 设颜色
  */
void YMGUI_MsgBox_SetColors(GYOBJ mb, GYcolor backdrop, GYcolor card,
                            GYcolor border, GYcolor title, GYcolor text)
{
	gy_assert(mb && mb->user_data);
	gy_log_explain((mb == NULL) || (mb->user_data == NULL), GY_LOG_PtrI, "对话框或数据不存在");
	GYmb_data* d = (GYmb_data*)mb->user_data;
	d->c_backdrop = backdrop;
	d->c_card = card;
	d->c_border = border;
	d->c_title = title;
	d->c_text = text;
	if (YMGUI_MsgBox_IsShown(mb))
		YMGUI_Obj_Invalidate(mb);
}

#endif // YMGUI_MSGBOX

