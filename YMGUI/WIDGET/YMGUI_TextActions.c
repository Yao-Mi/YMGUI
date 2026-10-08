#include "YMGUI_TextInput.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Event.h"
#include "YMGUI_Hal.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Geom.h"
/* Shared by single-line and multiline editors. */
#include "YMGUI_TextActions.h"
#include "YMGUI_EditView.h"
typedef struct
{
	GYOBJ menu, target, buttons[4];
	uint32 held;
	GYcoord start_x, start_y, bottom;
	uint8 multiline, moved, opened;
	int actions[4];
	GYtext_action_translate_cb translate;
	void* translate_user;
	uint8 enabled;
} GYtext_actions;
static void textActionsFree(GYOBJ obj)
{
	GY_free1(obj->user_data);
	obj->user_data = NULL;
}
static GYtext_actions* textActionsFind(GYCTX ctx)
{
	if (!ctx || !ctx->top_layer)
		return NULL;
	for (GYOBJ o = ctx->top_layer->child_head; o; o = o->sibling)
		if (o->free_cb == textActionsFree)
			return (GYtext_actions*)o->user_data;
	return NULL;
}
static void textActionsHide(GYtext_actions* d)
{
	if (d)
		YMGUI_Obj_SetHidden(d->menu, 1);
}
static void textActionsFocus(GYtext_actions* d)
{
	if (d->target)
	{
		YMGUI_SetFocus(d->target->ctx, d->target);
		d->target->state |= GY_STATE_Editing;
	}
}
static void textActionsShow(GYtext_actions* d);
static void textActionDraw(GYOBJ obj, GYSURFACE surface, const GYrect* a)
{
	GYsurface clipped = *surface;
	if (!GY_Rect_Intersect(&clipped.clip, &surface->clip, a))
		return;
	surface = &clipped;
	GYtext_actions* d = (GYtext_actions*)obj->user_data;
	int index = 0;
	while (index < 4 && d->buttons[index] != obj)
		++index;
	if (index == 4)
		return;
	int action = d->actions[index];
	const char* label = action == GY_KEY_COPY ? "复制" : action == GY_KEY_CUT ? "剪切"
													 : action == GY_KEY_PASTE ? "粘贴"
																			  : "全选";
	if (d->translate)
	{
		const char* text = d->translate((uint32)action, d->translate_user);
		if (text)
			label = text;
	}
	YMGUI_Draw_Fill(surface, a, (obj->state & GY_STATE_Pressed) ? GY_ARGB(255, 190, 207, 230) : GY_ARGB(255, 240, 244, 250), 255);
	int width = YMGUI_Font_TextWidth(&YMGUI_Font_Default, label);
	YMGUI_Draw_Text(surface, &YMGUI_Font_Default, a->x + (a->w - width) / 2, a->y + 12, label, GY_ARGB(255, 30, 40, 60));
}
static void textActionEvent(GYOBJ obj, GYEvent event)
{
	GYtext_actions* d = (GYtext_actions*)obj->user_data;
	if (!d || !d->target)
		return;
	if (event == GY_EVENT_Pressed || event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
	{
		textActionsFocus(d);
		YMGUI_Obj_Invalidate(obj);
	}
	if (event == GY_EVENT_Clicked)
	{
		int index = 0;
		while (index < 4 && d->buttons[index] != obj)
			++index;
		if (index == 4)
			return;
		int key = d->actions[index];
		textActionsFocus(d);
		/* Direct dispatch preserves selection and bypasses composition filters. */
		if (key == GY_KEY_COPY)
		{
			if (d->multiline)
			{
				YMGUI_EditView_Copy(d->target);
				YMGUI_EditView_ClearSelection(d->target);
			}
			else
			{
				YMGUI_TextInput_Copy(d->target);
				YMGUI_TextInput_ClearSelection(d->target);
			}
			textActionsHide(d);
			return;
		}
		if (key == GY_KEY_SEL_ALL)
		{
			if (d->multiline)
				YMGUI_EditView_SelectAll(d->target);
			else
				YMGUI_TextInput_SelectAll(d->target);
			textActionsShow(d);
			return;
		}
		GYCTX ctx = d->target->ctx;
		textActionsHide(d);
		YMGUI_Event_Key(ctx, (uint32)key);
		return;
	}
}
static GYtext_actions* textActionsCreate(GYCTX ctx)
{
	GYtext_actions* d = textActionsFind(ctx);
	if (d)
		return d;
	d = GY_malloc1(sizeof(*d));
	if (!d)
		return NULL;
	GY_memset(d, 0, sizeof(*d));
	d->bottom = ctx->root->area.h;
	d->menu = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, 0, 288, 40);
	if (!d->menu)
	{
		GY_free1(d);
		return NULL;
	}
	d->menu->state |= GY_STATE_ClipChildren;
	d->menu->user_data = d;
	d->menu->free_cb = textActionsFree;
	d->menu->event_cb = textActionEvent;
	YMGUI_Obj_SetHidden(d->menu, 1);
	for (int i = 0; i < 4; i++)
	{
		GYOBJ b = YMGUI_Creat_Obj_Creat(d->menu, i * 72, 0, 72, 40);
		if (!b)
		{
			YMGUI_Free_ObjFree(d->menu);
			return NULL;
		}
		d->buttons[i] = b;
		b->user_data = d;
		b->draw_cb = textActionDraw;
		b->event_cb = textActionEvent;
		b->state |= GY_STATE_OpaqueRect | GY_STATE_BoundedDraw;
	}
	return d;
}
static void textActionsShow(GYtext_actions* d)
{
	if (!d->target || !d->enabled)
		return;
	int count = 0;
	int selected = d->multiline ? YMGUI_EditView_HasSelection(d->target) : YMGUI_TextInput_HasSelection(d->target);
	const char* text = d->multiline ? YMGUI_EditView_GetText(d->target) : YMGUI_TextInput_GetText(d->target);
	if (selected)
	{
		d->actions[count++] = GY_KEY_COPY;
		d->actions[count++] = GY_KEY_CUT;
	}
	if (YMGUI_Clipboard_GetText()[0])
		d->actions[count++] = GY_KEY_PASTE;
	if (text && text[0])
		d->actions[count++] = GY_KEY_SEL_ALL;
	if (!count)
	{
		textActionsHide(d);
		return;
	}
	YMGUI_Obj_Invalidate(d->menu);
	for (int i = 0; i < 4; i++)
		YMGUI_Obj_SetHidden(d->buttons[i], i >= count);
	GYrect a;
	YMGUI_Obj_GetAbsArea(d->target, &a);
	int button_width = d->target->ctx->root->area.w / count;
	if (button_width > 72)
		button_width = 72;
	if (button_width < 1)
	{
		textActionsHide(d);
		return;
	}
	for (int i = 0; i < count; ++i)
		d->buttons[i]->area = (GYrect){i * button_width, 0, button_width, 40};
	int height = d->bottom < 40 ? d->bottom : 40;
	if (height <= 0)
	{
		textActionsHide(d);
		return;
	}
	for (int i = 0; i < count; ++i)
		d->buttons[i]->area.h = height;
	int width = count * button_width, x = d->start_x - width / 2, y = a.y - 44;
	if (x < 0)
		x = 0;
	if (x + width > d->target->ctx->root->area.w)
		x = d->target->ctx->root->area.w - width;
	if (y < 0)
		y = a.y + a.h + 4;
	if (y + height > d->bottom)
		y = d->bottom - height;
	if (y < 0)
		y = 0;
	d->menu->area = (GYrect){x, y, width, height};
	YMGUI_Obj_SetHidden(d->menu, 0);
	YMGUI_Obj_Invalidate(d->menu);
}
void YMGUI_TextActions_Update(GYCTX ctx, GYcoord bottom, uint8 enabled)
{
	GYtext_actions* d = textActionsFind(ctx);
	if (!d && enabled && ctx && !ctx->destroying)
		d = textActionsCreate(ctx);
	if (!d)
		return;
	d->enabled = enabled;
	int changed = d->bottom != bottom;
	d->bottom = bottom;
	if (!enabled || !d->target || ctx->focus_obj != d->target)
	{
		textActionsHide(d);
		return;
	}
	for (GYOBJ o = d->target; o; o = o->parent)
		if (o->state & GY_STATE_Hidden)
		{
			textActionsHide(d);
			return;
		}
	if (changed && !(d->menu->state & GY_STATE_Hidden))
		textActionsShow(d);
}
void YMGUI_TextActions_Detach(GYOBJ editor)
{
	if (!editor || !editor->ctx)
		return;
	GYtext_actions* d = textActionsFind(editor->ctx);
	if (d && d->target == editor)
	{
		textActionsHide(d);
		d->target = NULL;
	}
}
uint8 YMGUI_TextActions_Event(GYOBJ editor, GYEvent event, uint8 multiline)
{
	if (!editor || !editor->ctx)
		return 0;
	GYtext_actions* d = textActionsFind(editor->ctx);
	if (event == GY_EVENT_Pressed || event == GY_EVENT_ContextRequested)
	{
		if (!d || !d->enabled || editor->ctx->destroying)
			return 0;
		textActionsHide(d);
		d->target = editor;
		d->multiline = multiline;
		d->held = 0;
		d->moved = 0;
		d->opened = 0;
		d->start_x = editor->ctx->point_x;
		d->start_y = editor->ctx->point_y;
	}
	if (!d || !d->enabled || d->target != editor)
		return 0;
	if (event == GY_EVENT_Pressing)
	{
		int dx = editor->ctx->point_x - d->start_x, dy = editor->ctx->point_y - d->start_y;
		if (dx > 6 || dx < -6 || dy > 6 || dy < -6)
			d->moved = 1;
		if (d->opened)
			return 1;
	}
	if (event == GY_EVENT_Tick && !d->moved && !d->opened)
	{
		d->held += editor->ctx->tick_elapsed;
		if (d->held >= 550)
		{
			d->opened = 1;
			textActionsShow(d);
		}
	}
	if (event == GY_EVENT_ContextRequested)
		textActionsShow(d);
	if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
	{
		int selected = multiline ? YMGUI_EditView_HasSelection(editor) : YMGUI_TextInput_HasSelection(editor);
		if (selected && !d->opened)
			textActionsShow(d);
	}
	if (event == GY_EVENT_Key)
		textActionsHide(d);
	return 0;
}

void YMGUI_TextActions_SetTranslator(GYCTX ctx, GYtext_action_translate_cb translate, void* user)
{
	GYtext_actions* d = textActionsFind(ctx);
	if (!d)
		return;
	d->translate = translate;
	d->translate_user = user;
	YMGUI_Obj_Invalidate(d->menu);
}
GYOBJ YMGUI_TextActions_GetMenu(GYCTX ctx)
{
	GYtext_actions* d = textActionsFind(ctx);
	return d ? d->menu : NULL;
}
