#include "phone_desktop.h"
#include "phone_app.h"
#include "phone_launcher.h"
#include "YMGUI_Event.h"

static GYCTX ctx;
static GYOBJ* icons;
static GYOBJ labels[APP_COUNT], menu, menu_card, menu_title, info_button, remove_button, restore_button, ghost, target_mark, dialog;
static int order[APP_COUNT], *current_page;
static PhoneLauncherModel model;
static void (*select_page)(int, int);
static void (*on_removed)(int);
static int selected = -1, active, dragged, origin_page, origin_x, origin_y, edge_time, pending_remove;

static void layout(int animate)
{
	for (int id = 0; id < APP_COUNT; ++id)
	{
		int index = PhoneLauncher_Index(&model, id);
		YMGUI_Obj_SetHidden(icons[id], index < 0);
		YMGUI_Obj_SetHidden(labels[id], index < 0);
		if (index < 0)
			continue;
		PhoneLauncherSlot slot;
		PhoneLauncher_Place(index, model.count, &slot);
		if (animate)
		{
			PhoneUI_move(icons[id], slot.page * W + slot.x, slot.y, 180);
			PhoneUI_move(labels[id], slot.page * W + slot.x - 10, slot.y + 55, 180);
		}
		else
		{
#if YMGUI_ANIM
			YMGUI_Anim_CancelObject(icons[id]);
			YMGUI_Anim_CancelObject(labels[id]);
#endif
			PhoneUI_set_pos(icons[id], slot.page * W + slot.x, slot.y);
			PhoneUI_set_pos(labels[id], slot.page * W + slot.x - 10, slot.y + 55);
		}
	}
}
int PhoneDesktop_Index(int id)
{
	return PhoneLauncher_Index(&model, id);
}
int PhoneDesktop_Hit(int x, int y)
{
	for (int id = 0; id < APP_COUNT; ++id)
	{
		if (PhoneDesktop_Index(id) < 0)
			continue;
		GYrect a;
		YMGUI_Obj_GetAbsArea(icons[id], &a);
		if (x >= a.x - 5 && x < a.x + a.w + 5 && y >= a.y && y < a.y + a.h + 22)
			return id;
	}
	return -1;
}
static int drop_index(void)
{
	for (int i = 0; i < model.count; ++i)
	{
		PhoneLauncherSlot slot;
		PhoneLauncher_Place(i, model.count, &slot);
		if (slot.page == *current_page && abs(ctx->point_x - (slot.x + 24)) < 38 &&
			abs(ctx->point_y - (slot.y + 28 + 25)) < 47)
			return i;
	}
	return -1;
}
static void move_ghost(void)
{
	int x = ctx->point_x - 24, y = ctx->point_y - 25;
	if (x < 0)
		x = 0;
	if (x > W - 49)
		x = W - 49;
	if (y < 28)
		y = 28;
	if (y > 397)
		y = 397;
	PhoneUI_set_pos(ghost, x, y);
	int index = drop_index();
	YMGUI_Obj_SetHidden(target_mark, index < 0);
	if (index >= 0)
	{
		PhoneLauncherSlot slot;
		PhoneLauncher_Place(index, model.count, &slot);
		PhoneUI_set_pos(target_mark, slot.x - 4, slot.y + 24);
	}
}
static void backdrop_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	YMGUI_Draw_Fill(s, a, RGB(8, 13, 26), 70);
}
static void card_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	PhoneUI_rounded(s, a, RGB(34, 43, 61), 8, 255);
}
static void target_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	PhoneUI_rounded(s, a, RGB(196, 217, 255), 8, 130);
}
static void menu_event(GYOBJ obj, GYEvent event)
{
	(void)obj;
	if (event == GY_EVENT_Clicked)
		PhoneDesktop_Close();
}
static void show_menu(int id)
{
	selected = id;
	int x = ctx->point_x - 72, y = ctx->point_y - 144;
	if (x < 12)
		x = 12;
	if (x > 152)
		x = 152;
	if (y < 35)
		y = ctx->point_y + 30;
	if (y > 310)
		y = 310;
	PhoneUI_set_pos(menu_card, x, y);
	PhoneUI_text_set(menu_title, id < 0 ? "桌面管理" : PhoneApps_Get(id)->title);
	YMGUI_Obj_SetHidden(info_button, id < 0);
	YMGUI_Obj_SetHidden(remove_button, id < 0);
	YMGUI_Obj_SetHidden(restore_button, id >= 0);
	YMGUI_Obj_SetHidden(menu, 0);
}
static void dialog_result(GYOBJ obj, int index)
{
	YMGUI_MsgBox_Close(obj);
	if (pending_remove && index == 1 && selected >= 0)
	{
		int id = selected;
		PhoneLauncher_Remove(&model, id);
		on_removed(id);
		layout(1);
		select_page(*current_page, 0);
	}
	pending_remove = 0;
}
static void menu_info(GYOBJ obj)
{
	(void)obj;
	YMGUI_Obj_SetHidden(menu, 1);
	pending_remove = 0;
	char text[192];
	snprintf(text, sizeof(text), "%s\n模块：%s\n本地界面演示，数据保留于本次运行。", PhoneApps_Get(selected)->title, PhoneApps_Get(selected)->key);
	YMGUI_MsgBox_SetTitle(dialog, "应用信息");
	YMGUI_MsgBox_SetText(dialog, text);
	YMGUI_MsgBox_ClearButtons(dialog);
	YMGUI_MsgBox_AddButton(dialog, "知道了", dialog_result);
	YMGUI_MsgBox_Show(dialog);
}
static void menu_remove(GYOBJ obj)
{
	(void)obj;
	YMGUI_Obj_SetHidden(menu, 1);
	pending_remove = 1;
	YMGUI_MsgBox_SetTitle(dialog, "移除演示应用");
	YMGUI_MsgBox_SetText(dialog, "关闭应用并移除桌面入口。\n不会删除源码或电脑上的程序。\n长按桌面空白处可恢复。");
	YMGUI_MsgBox_ClearButtons(dialog);
	YMGUI_MsgBox_AddButton(dialog, "取消", dialog_result);
	YMGUI_MsgBox_AddButton(dialog, "卸载", dialog_result);
	YMGUI_MsgBox_Show(dialog);
}
static void menu_restore(GYOBJ obj)
{
	(void)obj;
	PhoneDesktop_Close();
	PhoneLauncher_Reset(&model, order, APP_COUNT);
	layout(1);
	select_page(0, 1);
}
int PhoneDesktop_Busy(void)
{
	return active || (menu && !(menu->state & GY_STATE_Hidden)) || (dialog && YMGUI_MsgBox_IsShown(dialog));
}
int PhoneDesktop_Close(void)
{
	int was = PhoneDesktop_Busy();
	if (!menu)
		return 0;
	if (active && dragged)
	{
		layout(0);
		select_page(origin_page, 0);
	}
	active = dragged = pending_remove = 0;
	YMGUI_Obj_SetHidden(menu, 1);
	YMGUI_Obj_SetHidden(ghost, 1);
	YMGUI_Obj_SetHidden(target_mark, 1);
	YMGUI_MsgBox_Close(dialog);
	return was;
}
void PhoneDesktop_Context(GYEvent event)
{
	if (event == GY_EVENT_ContextRequested)
	{
		PhoneDesktop_Close();
		selected = PhoneDesktop_Hit(ctx->point_x, ctx->point_y);
		origin_page = *current_page;
		origin_x = ctx->point_x;
		origin_y = ctx->point_y;
		active = ctx->context_obj != NULL;
		dragged = edge_time = 0;
		show_menu(selected);
	}
	else if (event == GY_EVENT_ContextDragging && active && selected >= 0)
	{
		if (!dragged && abs(ctx->point_x - origin_x) + abs(ctx->point_y - origin_y) < 10)
			return;
		if (!dragged)
		{
			dragged = 1;
			YMGUI_Obj_SetHidden(menu, 1);
			YMGUI_Obj_SetHidden(icons[selected], 1);
			YMGUI_Obj_SetHidden(labels[selected], 1);
			YMGUI_Obj_SetBgColor(ghost, PhoneApps_Get(selected)->color);
			PhoneUI_button_icon(ghost, PhoneApps_Get(selected)->icon);
			YMGUI_Obj_SetHidden(ghost, 0);
		}
		move_ghost();
	}
	else if (event == GY_EVENT_ContextReleased && active)
	{
		if (dragged)
		{
			int index = drop_index();
			if (index >= 0)
				PhoneLauncher_Move(&model, selected, index);
			layout(1);
			YMGUI_Obj_SetHidden(ghost, 1);
			YMGUI_Obj_SetHidden(target_mark, 1);
		}
		active = dragged = 0;
	}
	else if (event == GY_EVENT_ContextCancelled)
		PhoneDesktop_Close();
}
void PhoneDesktop_Tick(uint32 elapsed)
{
	if (!active || !dragged)
		return;
	int delta = ctx->point_x < 16 ? -1 : ctx->point_x > 303 ? 1
															: 0;
	int next = *current_page + delta;
	if (!delta || next < 0 || next >= PHONE_LAUNCHER_PAGE_COUNT(APP_COUNT))
	{
		edge_time = 0;
		return;
	}
	edge_time += elapsed;
	if (edge_time >= 550)
	{
		select_page(next, 1);
		edge_time = 0;
		move_ghost();
	}
}
void PhoneDesktop_Init(GYCTX context, GYOBJ strip, GYOBJ* app_icons, int* page,
					   void (*select_cb)(int, int), void (*remove_cb)(int))
{
	ctx = context;
	icons = app_icons;
	current_page = page;
	select_page = select_cb;
	on_removed = remove_cb;
	PhoneLauncher_Reset(&model, order, APP_COUNT);
	for (int i = 0; i < APP_COUNT; ++i)
	{
		icons[i] = PhoneUI_button(strip, 0, 0, 49, 51, "", NULL, PhoneApps_Get(i)->color);
		PhoneUI_button_icon(icons[i], PhoneApps_Get(i)->icon);
		labels[i] = PhoneUI_label_ex(strip, 0, 0, 69, PhoneApps_Get(i)->title, WHITE, 2);
	}
	layout(0);
}
void PhoneDesktop_BuildOverlay(void)
{
	menu = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, 0, W, H);
	menu->draw_cb = backdrop_draw;
	menu->event_cb = menu_event;
	menu_card = YMGUI_Creat_Obj_Creat(menu, 12, 100, 156, 132);
	menu_card->draw_cb = card_draw;
	menu_title = PhoneUI_left_label(menu_card, 12, 8, 132, "", WHITE, 2);
	info_button = PhoneUI_button(menu_card, 8, 39, 140, 36, "应用信息", menu_info, RGB(48, 59, 81));
	remove_button = PhoneUI_button(menu_card, 8, 84, 140, 36, "卸载", menu_remove, RGB(146, 68, 88));
	restore_button = PhoneUI_button(menu_card, 8, 48, 140, 42, "恢复默认桌面", menu_restore, ACCENT);
	YMGUI_Obj_SetHidden(menu, 1);
	target_mark = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, 0, 57, 59);
	target_mark->draw_cb = target_draw;
	ghost = PhoneUI_button(ctx->top_layer, 0, 0, 49, 51, "", NULL, ACCENT);
	YMGUI_Obj_SetHidden(target_mark, 1);
	YMGUI_Obj_SetHidden(ghost, 1);
	dialog = YMGUI_Creat_MsgBox_Creat(ctx);
	YMGUI_MsgBox_SetColors(dialog, GY_ARGB(140, 15, 20, 35), WHITE, RGB(215, 219, 231), INK, MUTED);
}
intptr_t PhoneDesktop_Inspect(const char* name)
{
	if (!strcmp(name, "info-button"))
		return (intptr_t)info_button;
	if (!strcmp(name, "remove-button"))
		return (intptr_t)remove_button;
	if (!strcmp(name, "restore-button"))
		return (intptr_t)restore_button;
	if (!strcmp(name, "dialog"))
		return (intptr_t)dialog;
	if (!strcmp(name, "menu"))
		return menu && !(menu->state & GY_STATE_Hidden);
	if (!strcmp(name, "count"))
		return model.count;
	if (!strcmp(name, "dragging"))
		return dragged;
	return 0;
}
