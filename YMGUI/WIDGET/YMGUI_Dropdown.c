#include "YMGUI_Dropdown.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawLine.h"
#include "YMGUI_Font.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Debug.h"
#include "YMGUI_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Dropdown.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-31
  *	@Description: 下拉框。合起=选中项+箭头;点击在 top_layer 弹出浮动菜单(跨子树置顶,不被父裁剪)。
  *	              菜单外点击经全屏透明 backdrop 收起;屏幕下方放不下时菜单向上翻。
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.弹出层(backdrop + menu)是 top_layer 的子,与 dropdown 本体不在同一子树;
  *   dropdown 的 free_cb 负责显式拆除它们(不会随本体级联释放)。
  * 2.每次展开都重建菜单(先拆旧再建新)——展开由"点击合起态本体"触发,此刻弹出层
  *   对象不在事件调用栈上,释放安全;收起只隐藏 backdrop(不在选项回调里释放对象)。
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define OPT_ROW_H 22 //菜单每行高
#define OPT_TEXT_PAD 6 //选项文字左右留白(与 optDrawCb 的 abs->x + 6 对齐)

//下拉框私有数据
typedef struct
{
	char   opts[GY_DROPDOWN_MAX_OPT][GY_DROPDOWN_OPT_LEN];//选项文字
	uint16 opt_count;   //选项个数
	uint16 selected;    //当前选中下标
	GYdropdown_sel_cb sel_cb;//选中变化回调
	GYOBJ  backdrop;    //全屏透明遮罩(菜单外点击收起);NULL=未展开
	GYOBJ  menu;        //浮动菜单容器(挂 top_layer)
}GYdd_data;

//菜单选项行私有数据
typedef struct
{
	GYOBJ  dd;    //回指下拉框本体
	uint16 idx;   //本行选项下标
}GYopt_data;

//---- 收起:隐藏并拆除弹出层 ----
/**
  * @brief 拆除弹出层(backdrop 连同其下的 menu 子树一起释放)
  *        仅在安全上下文调用(展开重建时、或本体析构时)——不在选项/遮罩回调里调。
  */
static void teardownPopup(GYdd_data* d)
{
	if (d->backdrop != NULL)
	{
		//菜单是 backdrop 的兄弟(都挂 top_layer),分别释放
		if (d->menu != NULL)
		{
			YMGUI_Free_ObjFree(d->menu);
			d->menu = NULL;
		}
		YMGUI_Free_ObjFree(d->backdrop);
		d->backdrop = NULL;
	}
}

/**
  * @brief 遮罩事件:菜单外点击 → 收起。此处只隐藏(标脏重绘露出的底层),
  *        实际拆除留到下次展开或析构——避免在回调里释放正处于事件派发中的对象。
  */
static void backdropEventCb(GYOBJ obj, GYEvent e)
{
	if (e != GY_EVENT_Clicked)
		return;
	GYdd_data* d = (GYdd_data*)obj->user_data;//backdrop 的 user_data 存 dd_data
	if (d == NULL)
		return;
	//隐藏 backdrop 与 menu(下次展开时才真正释放重建)
	if (d->menu != NULL)     YMGUI_Obj_SetHidden(d->menu, 1);
	if (d->backdrop != NULL) YMGUI_Obj_SetHidden(d->backdrop, 1);
}

//---- 菜单选项行 ----
/**
  * @brief 选项行绘制:背景条(按下高亮)+ 左对齐文字 + 底部分隔线
  */
static void optDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	GYcolor bg = (obj->state & GY_STATE_Pressed)
	           ? GY_ARGB(0xFF, 0x35, 0x5A, 0x8A)
	           : obj->bg_color;
	YMGUI_Draw_Fill(s, abs, bg, GY_OPA_COVER);
	GYopt_data* od = (GYopt_data*)obj->user_data;
	GYdd_data* d = (GYdd_data*)od->dd->user_data;
	GYFONT font = &YMGUI_Font_Default;
	GYcoord ty = abs->y + ((abs->h > font->cell_h) ? (abs->h - font->cell_h) / 2 : 0);
	YMGUI_Draw_Text(s, font, abs->x + 6, ty, d->opts[od->idx], GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0));
	GYrect sep = {abs->x, abs->y + abs->h - 1, abs->w, 1};
	YMGUI_Draw_Fill(s, &sep, GY_ARGB(0xFF, 0x40, 0x40, 0x48), GY_OPA_COVER);
}

/**
  * @brief 选项行事件:按下/抬起标脏(高亮);点击 → 定选中项、触发回调、收起菜单
  */
static void optEventCb(GYOBJ obj, GYEvent e)
{
	GYopt_data* od = (GYopt_data*)obj->user_data;
	switch (e)
	{
	case GY_EVENT_Pressed:
	case GY_EVENT_Released:
	case GY_EVENT_ReleasedOff:
		YMGUI_Obj_Invalidate(obj);
		break;
	case GY_EVENT_Clicked:
	{
		GYOBJ dd = od->dd;
		GYdd_data* d = (GYdd_data*)dd->user_data;
		uint16 sel = od->idx;
		d->selected = sel;
		//收起(只隐藏,不释放——本回调仍在此选项对象的事件派发中)
		if (d->menu != NULL)     YMGUI_Obj_SetHidden(d->menu, 1);
		if (d->backdrop != NULL) YMGUI_Obj_SetHidden(d->backdrop, 1);
		YMGUI_Obj_Invalidate(dd);//合起态显示新选中项
		if (d->sel_cb != NULL)
			d->sel_cb(dd, sel);
		break;
	}
	default:
		break;
	}
}

static void optFreeCb(GYOBJ obj)
{
	if (obj->user_data != NULL) { GY_free0(obj->user_data); obj->user_data = NULL; }
}

/**
  * @brief 菜单容器绘制:背景 + 1px 边框
  */
static void menuDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	YMGUI_Draw_Fill(s, abs, obj->bg_color, GY_OPA_COVER);
	GYcolor border = GY_ARGB(0xFF, 0x50, 0x50, 0x5A);
	GYrect top = {abs->x, abs->y, abs->w, 1};
	GYrect bot = {abs->x, abs->y + abs->h - 1, abs->w, 1};
	GYrect lft = {abs->x, abs->y, 1, abs->h};
	GYrect rgt = {abs->x + abs->w - 1, abs->y, 1, abs->h};
	YMGUI_Draw_Fill(s, &top, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &bot, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &lft, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &rgt, border, GY_OPA_COVER);
}

//---- 展开 ----
/**
  * @brief 展开菜单:拆旧弹出层 → 在 top_layer 建全屏透明 backdrop + 浮动 menu(含各选项行)
  *        菜单默认贴合起态本体正下方;屏幕下方放不下则向上翻贴其正上方
  */
static void openMenu(GYOBJ dd)
{
	GYdd_data* d = (GYdd_data*)dd->user_data;
	if (d->opt_count == 0)
		return;//无选项不展开

	GYCTX ctx = dd->ctx;
	GYOBJ top = YMGUI_Ctx_GetTopLayer(ctx);
	if (top == NULL)
		return;

	teardownPopup(d);//确保干净

	//合起态本体的屏幕绝对矩形(决定菜单落点)
	GYrect ba;
	YMGUI_Obj_GetAbsArea(dd, &ba);

	//菜单尺寸:宽 = max(本体宽, 最宽选项文字 + 左右留白),高 = 行数 * 行高。
	//  (本体常做窄"菜单名",选项文字更长——按内容自适应,避免文字溢出框外)
	GYFONT mfont = &YMGUI_Font_Default;
	GYcoord menu_w = ba.w;
	for (uint16 oi = 0; oi < d->opt_count; oi++)
	{
		GYcoord tw = (GYcoord)(YMGUI_Font_TextWidth(mfont, d->opts[oi]) + OPT_TEXT_PAD * 2);
		if (tw > menu_w)
			menu_w = tw;
	}
	GYcoord menu_h = (GYcoord)(d->opt_count * OPT_ROW_H);

	//落点:优先本体正下方;若下方超出屏幕、且上方更宽裕则向上翻
	GYDISP disp = (GYDISP)ctx->disp;
	GYcoord scr_h = (disp != NULL) ? disp->ver_res : (GYcoord)(ba.y + ba.h + menu_h);
	GYcoord scr_w = (disp != NULL) ? disp->hor_res : (GYcoord)(ba.x + menu_w);
	GYcoord below_y = ba.y + ba.h;
	GYcoord menu_y;
	if (below_y + menu_h <= scr_h || ba.y < menu_h)
		menu_y = below_y;          //下方放得下,或上方也放不下 → 仍朝下
	else
		menu_y = ba.y - menu_h;    //向上翻
	//横向:贴本体左缘,但整宽超出屏幕右沿则左移,不跑框外
	GYcoord menu_x = ba.x;
	if (menu_x + menu_w > scr_w)
		menu_x = (GYcoord)(scr_w - menu_w);
	if (menu_x < 0)
		menu_x = 0;

	//全屏透明遮罩(挂 top_layer;先建 → 处于菜单之下)
	GYOBJ backdrop = YMGUI_Creat_Obj_Creat(top, 0, 0, top->area.w, top->area.h);
	if (backdrop == NULL)
		return;
	backdrop->draw_cb = NULL;               //透明:不画
	backdrop->event_cb = backdropEventCb;   //外部点击收起
	backdrop->free_cb = NULL;
	backdrop->user_data = d;                //回指 dd_data(取 menu/backdrop 句柄)

	//菜单容器(挂 top_layer;后建 → 处于遮罩之上,遮住其下的遮罩命中)
	GYOBJ menu = YMGUI_Creat_Obj_Creat(top, menu_x, menu_y, menu_w, menu_h);
	if (menu == NULL)
	{
		YMGUI_Free_ObjFree(backdrop);
		return;
	}
	menu->draw_cb = menuDrawCb;
	menu->event_cb = NULL;
	menu->free_cb = NULL;
	menu->bg_color = GY_ARGB(0xFF, 0x28, 0x28, 0x30);

	//各选项行(菜单的子,相对菜单纵向堆叠)
	for (uint16 i = 0; i < d->opt_count; i++)
	{
		GYOBJ row = YMGUI_Creat_Obj_Creat(menu, 0, (GYcoord)(i * OPT_ROW_H), menu_w, OPT_ROW_H);
		if (row == NULL)
			continue;
		GYopt_data* rd = (GYopt_data*)GY_malloc0(sizeof(GYopt_data));
		gy_assert(rd);
		gy_log_explain(rd == NULL, GY_LOG_Mem0, "选项行数据内存申请失败");
		if (rd == NULL) { YMGUI_Free_ObjFree(row); continue; }
		GY_memset(rd, 0, sizeof(GYopt_data));
		rd->dd = dd;
		rd->idx = i;
		row->user_data = rd;
		row->draw_cb = optDrawCb;
		row->event_cb = optEventCb;
		row->free_cb = optFreeCb;
		row->bg_color = GY_ARGB(0xFF, 0x28, 0x28, 0x30);
	}

	d->backdrop = backdrop;
	d->menu = menu;
	YMGUI_Obj_Invalidate(menu);
}

//---- 合起态本体 ----
/**
  * @brief 本体绘制:背景 + 边框 + 选中项文字(无选项显占位)+ 右侧下拉箭头(两段线)
  */
static void ddDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	YMGUI_Draw_Fill(s, abs, obj->bg_color, GY_OPA_COVER);
	GYcolor border = GY_ARGB(0xFF, 0x50, 0x50, 0x5A);
	GYrect top = {abs->x, abs->y, abs->w, 1};
	GYrect bot = {abs->x, abs->y + abs->h - 1, abs->w, 1};
	GYrect lft = {abs->x, abs->y, 1, abs->h};
	GYrect rgt = {abs->x + abs->w - 1, abs->y, 1, abs->h};
	YMGUI_Draw_Fill(s, &top, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &bot, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &lft, border, GY_OPA_COVER);
	YMGUI_Draw_Fill(s, &rgt, border, GY_OPA_COVER);

	GYdd_data* d = (GYdd_data*)obj->user_data;
	GYFONT font = &YMGUI_Font_Default;
	GYcoord ty = abs->y + ((abs->h > font->cell_h) ? (abs->h - font->cell_h) / 2 : 0);
	const char* txt = (d->opt_count > 0) ? d->opts[d->selected] : "";
	YMGUI_Draw_Text(s, font, abs->x + 6, ty, txt, GY_ARGB(0xFF, 0xF0, 0xF0, 0xF0));

	//右侧下拉箭头:一个朝下的小 ∨(两段线),置于右内边距处
	GYcoord cx = abs->x + abs->w - 12;
	GYcoord cy = abs->y + abs->h / 2 - 2;
	GYcolor arrow = GY_ARGB(0xFF, 0xC0, 0xC0, 0xC8);
	YMGUI_Draw_Line(s, cx, cy, cx + 4, cy + 4, arrow);
	YMGUI_Draw_Line(s, cx + 4, cy + 4, cx + 8, cy, arrow);
}

/**
  * @brief 本体事件:点击 → 展开菜单(已展开则本次点击已被 backdrop 收起,不重复处理)
  */
static void ddEventCb(GYOBJ obj, GYEvent e)
{
	GYdd_data* d = (GYdd_data*)obj->user_data;
	switch (e)
	{
	case GY_EVENT_Pressed:
	case GY_EVENT_Released:
	case GY_EVENT_ReleasedOff:
		YMGUI_Obj_Invalidate(obj);
		break;
	case GY_EVENT_Clicked:
		//展开:仅当当前未展开(backdrop 为空或已隐藏)
		if (d->backdrop == NULL || (d->backdrop->state & GY_STATE_Hidden))
			openMenu(obj);
		break;
	default:
		break;
	}
}

static void ddFreeCb(GYOBJ obj)
{
	GYdd_data* d = (GYdd_data*)obj->user_data;
	if (d != NULL)
	{
		teardownPopup(d);//显式拆除挂在 top_layer 的弹出层(不随本体级联)
		GY_free0(d);
		obj->user_data = NULL;
	}
}

//---- 公共 API ----
/**
  * @brief 创建下拉框
  */
GYOBJ YMGUI_Creat_Dropdown_Creat(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h)
{
	GYOBJ dd = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	gy_assert(dd);
	if (dd == NULL)
		return NULL;
	GYdd_data* d = (GYdd_data*)GY_malloc0(sizeof(GYdd_data));
	gy_assert(d);
	gy_log_explain(d == NULL, GY_LOG_Mem0, "下拉框数据内存申请失败");
	if (d == NULL) { YMGUI_Free_ObjFree(dd); return NULL; }
	GY_memset(d, 0, sizeof(GYdd_data));
	d->opt_count = 0;
	d->selected = 0;
	d->sel_cb = NULL;
	d->backdrop = NULL;
	d->menu = NULL;

	dd->type = GY_OBJ_Dropdown;
	dd->user_data = d;
	dd->draw_cb = ddDrawCb;
	dd->event_cb = ddEventCb;
	dd->free_cb = ddFreeCb;
	dd->bg_color = GY_ARGB(0xFF, 0x1C, 0x1C, 0x24);
	YMGUI_Obj_Invalidate(dd);
	return dd;
}

/**
  * @brief 追加一个选项
  */
int YMGUI_Dropdown_AddOption(GYOBJ dd, const char* text)
{
	gy_assert(dd && dd->user_data && text);
	gy_log_explain((dd == NULL) || (dd->user_data == NULL) || (text == NULL), GY_LOG_PtrI, "下拉框/数据/文本不存在");
	GYdd_data* d = (GYdd_data*)dd->user_data;
	if (d->opt_count >= GY_DROPDOWN_MAX_OPT)
		return -1;//满
	uint16 slot = d->opt_count;
	uint16 i = 0;
	while (text[i] != '\0' && i < GY_DROPDOWN_OPT_LEN - 1)
	{
		d->opts[slot][i] = text[i];
		i++;
	}
	d->opts[slot][i] = '\0';
	d->opt_count++;
	YMGUI_Obj_Invalidate(dd);
	return (int)slot;
}

/**
  * @brief 设选中项(越界忽略)
  */
void YMGUI_Dropdown_SetSelected(GYOBJ dd, uint16 sel)
{
	gy_assert(dd && dd->user_data);
	gy_log_explain((dd == NULL) || (dd->user_data == NULL), GY_LOG_PtrI, "下拉框或数据不存在");
	GYdd_data* d = (GYdd_data*)dd->user_data;
	if (sel >= d->opt_count)
		return;
	d->selected = sel;
	YMGUI_Obj_Invalidate(dd);
}

/**
  * @brief 取当前选中项下标
  */
uint16 YMGUI_Dropdown_GetSelected(GYOBJ dd)
{
	gy_assert(dd && dd->user_data);
	gy_log_explain((dd == NULL) || (dd->user_data == NULL), GY_LOG_PtrI, "下拉框或数据不存在");
	return ((GYdd_data*)dd->user_data)->selected;
}

/**
  * @brief 取选项个数
  */
uint16 YMGUI_Dropdown_GetOptionCount(GYOBJ dd)
{
	gy_assert(dd && dd->user_data);
	gy_log_explain((dd == NULL) || (dd->user_data == NULL), GY_LOG_PtrI, "下拉框或数据不存在");
	return ((GYdd_data*)dd->user_data)->opt_count;
}

/**
  * @brief 设选中变化回调
  */
void YMGUI_Dropdown_SetSelectedCb(GYOBJ dd, GYdropdown_sel_cb cb)
{
	gy_assert(dd && dd->user_data);
	gy_log_explain((dd == NULL) || (dd->user_data == NULL), GY_LOG_PtrI, "下拉框或数据不存在");
	((GYdd_data*)dd->user_data)->sel_cb = cb;
}

/**
  * @brief 菜单是否展开(backdrop 存在且未隐藏)
  */
uint8 YMGUI_Dropdown_IsOpen(GYOBJ dd)
{
	gy_assert(dd && dd->user_data);
	gy_log_explain((dd == NULL) || (dd->user_data == NULL), GY_LOG_PtrI, "下拉框或数据不存在");
	GYdd_data* d = (GYdd_data*)dd->user_data;
	return (d->backdrop != NULL && !(d->backdrop->state & GY_STATE_Hidden)) ? 1 : 0;
}

/**
  * @brief 主动收起(只隐藏,弹出层留到下次展开重建或析构时释放)
  */
void YMGUI_Dropdown_Close(GYOBJ dd)
{
	gy_assert(dd && dd->user_data);
	gy_log_explain((dd == NULL) || (dd->user_data == NULL), GY_LOG_PtrI, "下拉框或数据不存在");
	GYdd_data* d = (GYdd_data*)dd->user_data;
	if (d->menu != NULL)     YMGUI_Obj_SetHidden(d->menu, 1);
	if (d->backdrop != NULL) YMGUI_Obj_SetHidden(d->backdrop, 1);
}
