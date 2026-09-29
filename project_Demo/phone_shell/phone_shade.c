#include "phone_ui.h"
#include "phone_shade.h"
#include "phone_host.h"
#include "YMGUI_Image.h"
#include "phone_quick.h"
#include "phone_quick_builtin.h"

static GYCTX ctx;
static GYOBJ trigger, overlay, sheet, tiles[PHONE_QUICK_CAPACITY], bright, cards[2], empty, preview, preview_image;
static int enabled, visible, closing, start_y, drag_origin, dragging;
static int notices[2] = {1, 1}, quick_page;
static int clearing;
static GYOBJ page_label, page_prev, page_next;
static int last_active[PHONE_QUICK_CAPACITY];
static GYpx* captured;
static GYimg capture_image;
static void transparent_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	(void)s;
	(void)a;
}
static void backdrop_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	YMGUI_Draw_Fill(s, a, RGB(14, 20, 36), 160);
}

static void refresh(void)
{
	for (int i = 0; i < PhoneQuick_Count(); ++i)
	{
		YMGUI_Obj_SetHidden(tiles[i], i / 8 != quick_page);
		YMGUI_Obj_Invalidate(tiles[i]);
	}
	if (page_label)
	{
		char text[24];
		snprintf(text, sizeof(text), "%d/%d", quick_page + 1, (PhoneQuick_Count() + 7) / 8);
		PhoneUI_text_set(page_label, text);
	}
	for (int i = 0; i < 2; ++i)
		YMGUI_Obj_SetHidden(cards[i], !notices[i]);
	YMGUI_Obj_SetHidden(empty, notices[0] || notices[1]);
}
static void tile_draw(GYOBJ obj, GYSURFACE surface, const GYrect* a)
{
	const PhoneQuick* feature = obj->user_data;
	GYsurface clipped = *surface;
	if (!GY_Rect_Intersect(&clipped.clip, a, &surface->clip))
		return;
	GYSURFACE s = &clipped;
	GYcolor bg = feature->active && feature->active() ? RGB(77, 100, 221) : RGB(42, 51, 70);
	if (obj->state & GY_STATE_Pressed)
		bg = RGB(103, 120, 201);
	PhoneUI_rounded(s, a, bg, 8, 255);
	int x = a->x + a->w / 2, y = a->y + 20;
	if (feature->draw_icon)
		feature->draw_icon(s, x, y, bg);
	else
		PhoneUI_icon_draw(s, ICON_SETTINGS, x, y, bg);
	char title[96];
	snprintf(title, sizeof(title), "%s", feature->title);
	while (PhoneUI_text_width(title, 0) > a->w - 10 && title[0])
	{
		size_t n = strlen(title) - 1;
		while (n && ((unsigned char)title[n] & 0xc0) == 0x80)
			--n;
		title[n] = 0;
	}
	PhoneUI_draw_text(s, title, WHITE, 0, x, a->y + 37);
}
static void page_click(GYOBJ obj)
{
	int last = (PhoneQuick_Count() - 1) / 8;
	if (obj == page_next && quick_page < last)
		++quick_page;
	if (obj == page_prev && quick_page > 0)
		--quick_page;
	refresh();
}
static void notification_draw(GYOBJ obj, GYSURFACE surface, const GYrect* a)
{
	GYsurface clipped = *surface;
	if (!GY_Rect_Intersect(&clipped.clip, a, &surface->clip))
		return;
	GYSURFACE s = &clipped;
	int message = obj == cards[0];
	PhoneUI_rounded(s, a, RGB(42, 51, 70), 8, 255);
	GYrect icon = {a->x + 9, a->y + 13, 24, 24};
	PhoneUI_rounded(s, &icon, message ? RGB(64, 143, 217) : RGB(193, 147, 66), 5, 255);
	PhoneUI_draw_text(s, message ? "信" : "晴", WHITE, 2, a->x + 21, a->y + 16);
	const char* title = message ? "小雨 · 短信" : "天气提醒";
	const char* detail = message ? "今天一起去散步吗？" : "今天晴，适合出门";
	PhoneUI_draw_text(s, title, WHITE, 2, a->x + 44 + PhoneUI_text_width(title, 2) / 2, a->y + 6);
	PhoneUI_draw_text(s, detail, RGB(162, 174, 198), 2, a->x + 44 + PhoneUI_text_width(detail, 2) / 2, a->y + 28);
}
static void slider_panel_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	PhoneUI_rounded(s, a, RGB(34, 43, 62), 8, 255);
}
static void close_click(GYOBJ obj)
{
	(void)obj;
	PhoneShade_Close(1);
}
static void preview_close(GYOBJ obj)
{
	(void)obj;
	YMGUI_Obj_SetHidden(preview, 1);
}
void PhoneShade_Capture(void)
{
	PhoneShade_Close(0);
	if (!captured)
		captured = GY_malloc1(W * H * sizeof(GYpx));
	if (!captured)
	{
		PhoneHost_Notice("截屏", "内存不足，未截取画面");
		return;
	}
	PhoneHost_Capture(captured);
	capture_image = (GYimg){.data = captured, .w = W, .h = H};
	YMGUI_Image_SetSrc(preview_image, &capture_image);
	PhoneShade_Open();
	YMGUI_Obj_SetHidden(preview, 0);
}
static void tile_event(GYOBJ obj, GYEvent event)
{
	if (event == GY_EVENT_Pressed || event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
		YMGUI_Obj_Invalidate(obj);
	if (event != GY_EVENT_Clicked)
		return;
	const PhoneQuick* feature = obj->user_data;
	feature->activate();
	refresh();
}
static void brightness_changed(GYOBJ obj, int32 value)
{
	(void)obj;
	PhoneHost_SetBrightness(value);
}
/* 收起中途也提交清空，取消动画后复位隐藏卡片，重开不留下半截卡片。 */
static void finish_clear(void)
{
	clearing = 0;
	for (int i = 0; i < 2; ++i)
	{
#if YMGUI_ANIM
		YMGUI_Anim_CancelObject(cards[i]);
#endif
		notices[i] = 0;
		YMGUI_Obj_SetHidden(cards[i], 1);
		PhoneUI_set_pos(cards[i], 14, cards[i]->area.y);
	}
	refresh();
}
static void clear_click(GYOBJ obj)
{
	(void)obj;
	if (clearing || (!notices[0] && !notices[1]))
		return;
	clearing = 1;
#if YMGUI_ANIM
	int order = 0;
	for (int i = 0; i < 2; ++i)
	{
		if (!notices[i])
			continue;
		GYANIM anim = YMGUI_Anim_MoveTo(cards[i], W + 8, cards[i]->area.y, 240, GY_ANIM_EASE_IN);
		if (anim)
			YMGUI_Anim_SetDelay(ctx, anim, (uint32)(order++ * 55));
		else
			PhoneUI_set_pos(cards[i], W + 8, cards[i]->area.y);
	}
#else
	finish_clear();
#endif
}
static void notification_click(GYOBJ obj)
{
	if (clearing)
		return;
	int id = obj == cards[0] ? 0 : 1;
	notices[id] = 0;
	PhoneShade_Close(0);
	PhoneHost_Open(id ? WEATHER : MESSAGES, NULL);
}
static void drag_event(GYOBJ obj, GYEvent event)
{
	if (!enabled)
		return;
	if (event == GY_EVENT_Pressed)
	{
		start_y = ctx->point_y;
		drag_origin = visible ? sheet->area.y : -448;
		dragging = 0;
#if YMGUI_ANIM
		YMGUI_Anim_CancelObject(sheet);
#endif
	}
	else if (event == GY_EVENT_Pressing)
	{
		int delta = ctx->point_y - start_y;
		if (abs(delta) < 5 && !dragging)
			return;
		dragging = 1;
		if (!visible)
		{
			PhoneIME_Hide();
			visible = 1;
			closing = 0;
			YMGUI_Obj_SetHidden(overlay, 0);
		}
		int y = drag_origin + delta;
		if (y < -448)
			y = -448;
		if (y > 0)
			y = 0;
		PhoneUI_set_pos(sheet, 0, y);
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
	{
		if (!ctx->pressed_obj)
		{
			if (drag_origin == -448)
				PhoneShade_Close(0);
			else
				PhoneShade_Open();
			dragging = 1;
		}
		else if (dragging)
		{
			int delta = ctx->point_y - start_y;
			if ((drag_origin == -448 && delta > 45) || (drag_origin != -448 && delta > -65))
				PhoneShade_Open();
			else
				PhoneShade_Close(1);
		}
	}
	else if (event == GY_EVENT_Clicked && !dragging)
	{
		if (obj == trigger)
			PhoneShade_Open();
		else
			PhoneShade_Close(1);
	}
}
void PhoneShade_Open(void)
{
	if (!enabled)
		return;
	PhoneIME_Hide();
	if (!visible)
		PhoneUI_set_pos(sheet, 0, -448);
	visible = 1;
	closing = 0;
	YMGUI_Obj_SetHidden(overlay, 0);
	PhoneShade_Sync();
	refresh();
	PhoneUI_move(sheet, 0, 0, 220);
}
int PhoneShade_Close(int animate)
{
	if (!visible)
		return 0;
	YMGUI_Obj_SetHidden(preview, 1);
	if (animate)
	{
		closing = 1;
		PhoneUI_move(sheet, 0, -448, 200);
	}
	else
	{
		if (clearing)
			finish_clear();
#if YMGUI_ANIM
		YMGUI_Anim_CancelObject(sheet);
#endif
		visible = closing = 0;
		YMGUI_Obj_SetHidden(overlay, 1);
	}
	return 1;
}
int PhoneShade_Visible(void)
{
	return visible;
}
void PhoneShade_Update(int allow)
{
	if (clearing && (!notices[0] || cards[0]->area.x >= W) && (!notices[1] || cards[1]->area.x >= W))
		finish_clear();
	enabled = allow;
	YMGUI_Obj_SetHidden(trigger, !allow);
	if (!allow)
		PhoneShade_Close(0);
	if (closing && sheet->area.y <= -448)
		PhoneShade_Close(0);
	if (visible)
		for (int i = 0; i < PhoneQuick_Count(); ++i)
		{
			const PhoneQuick* feature = PhoneQuick_Get(i);
			int on = feature->active && feature->active();
			if (on != last_active[i])
			{
				last_active[i] = on;
				YMGUI_Obj_Invalidate(tiles[i]);
			}
		}
}
void PhoneShade_SetWifi(int on)
{
	PhoneQuickBuiltin_SetWifi(on);
	if (sheet)
		refresh();
}
int PhoneShade_GetWifi(void)
{
	return PhoneQuickBuiltin_State("wifi");
}
void PhoneShade_Sync(void)
{
	if (bright)
		YMGUI_Slider_SetValue(bright, PhoneHost_GetBrightness());
}
void PhoneShade_Init(GYCTX context)
{
	ctx = context;
	PhoneQuick_RegisterDefaults();
	PhoneQuick_Lock();
	trigger = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, 0, W, 28);
	trigger->event_cb = drag_event;
	trigger->draw_cb = transparent_draw;
	YMGUI_Obj_SetHidden(trigger, 1);
	overlay = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, 0, W, 448);
	overlay->draw_cb = backdrop_draw;
	overlay->state |= GY_STATE_ClipChildren;
	overlay->event_cb = drag_event;
	sheet = YMGUI_Creat_Obj_Creat(overlay, 0, -448, W, 448);
	YMGUI_Obj_SetBgColor(sheet, RGB(20, 28, 44));
	sheet->event_cb = drag_event;
	PhoneUI_left_label(sheet, 18, 4, 180, "Yaomi", RGB(162, 174, 198), 2);
	PhoneUI_left_label(sheet, 18, 27, 165, "09:41", WHITE, 3);
	PhoneUI_left_label(sheet, 196, 33, 110, "控制中心", WHITE, 0);
	PhoneUI_left_label(sheet, 18, 65, 176, "9月29日 星期二", RGB(162, 174, 198), 2);
	if (PhoneQuick_Count() > 8)
	{
		page_label = PhoneUI_label_ex(sheet, 174, 64, 56, "", MUTED, 2);
		page_prev = PhoneUI_button(sheet, 236, 62, 30, 22, "<", page_click, RGB(42, 51, 70));
		page_next = PhoneUI_button(sheet, 276, 62, 30, 22, ">", page_click, RGB(42, 51, 70));
	}
	else
		PhoneUI_left_label(sheet, 228, 65, 78, "本地演示", RGB(120, 142, 195), 2);
	for (int i = 0; i < PhoneQuick_Count(); ++i)
	{
		GYrect slot;
		PhoneQuick_Place(i, &slot);
		tiles[i] = YMGUI_Creat_Obj_Creat(sheet, slot.x, slot.y, slot.w, slot.h);
		tiles[i]->user_data = (void*)PhoneQuick_Get(i);
		tiles[i]->draw_cb = tile_draw;
		tiles[i]->event_cb = tile_event;
		tiles[i]->state |= GY_STATE_ClipChildren;
	}
	GYOBJ slider_panel = YMGUI_Creat_Obj_Creat(sheet, 14, 224, 292, 36);
	slider_panel->draw_cb = slider_panel_draw;
	PhoneUI_left_label(sheet, 18, 230, 55, "亮度", WHITE, 2);
	bright = YMGUI_Creat_Slider_Creat(sheet, 78, 230, 226, 24);
	bright->draw_cb = PhoneUI_slider_draw;
	YMGUI_Slider_SetChanged(bright, brightness_changed);
	PhoneUI_left_label(sheet, 18, 272, 180, "通知", WHITE, 0);
	PhoneUI_button(sheet, 242, 270, 64, 26, "清空", clear_click, RGB(55, 62, 83));
	cards[0] = PhoneUI_button(sheet, 14, 301, 292, 51, "小雨 · 今天一起去散步吗？", notification_click, RGB(55, 62, 83));
	cards[1] = PhoneUI_button(sheet, 14, 361, 292, 51, "天气 · 今天晴，适合出门", notification_click, RGB(55, 62, 83));
	cards[0]->draw_cb = cards[1]->draw_cb = notification_draw;
	empty = PhoneUI_label(sheet, 14, 350, 292, "暂无通知", RGB(164, 174, 202));
	GYOBJ handle = PhoneUI_button(sheet, 108, 423, 104, 23, "收起", close_click, 0);
	handle->event_cb = drag_event;
	preview = YMGUI_Creat_Obj_Creat(overlay, 0, 0, W, 448);
	YMGUI_Obj_SetBgColor(preview, PAPER);
	PhoneUI_label(preview, 18, 25, 284, "截屏预览", INK);
	preview_image = YMGUI_Creat_Image_Creat(preview, 80, 75, 160, 240);
	YMGUI_Image_SetScaleMode(preview_image, GY_IMG_FIT);
	PhoneUI_label_ex(preview, 10, 329, 300, "当前画面 · 仅保留本次截屏", MUTED, 2);
	PhoneUI_button(preview, 72, 369, 176, 42, "返回控制中心", preview_close, ACCENT);
	YMGUI_Obj_SetHidden(preview, 1);
	YMGUI_Obj_SetHidden(overlay, 1);
	refresh();
}
void PhoneShade_Shutdown(void)
{
	GY_free1(captured);
	captured = NULL;
}
intptr_t PhoneShade_Inspect(const char* name)
{
	if (!strcmp(name, "page"))
		return quick_page;
	if (!strcmp(name, "flight"))
		return PhoneQuickBuiltin_State("flight");
	if (!strcmp(name, "data"))
		return PhoneQuickBuiltin_State("data");
	if (!strcmp(name, "hotspot"))
		return PhoneQuickBuiltin_State("hotspot");
	if (!strcmp(name, "bluetooth"))
		return PhoneQuickBuiltin_State("bluetooth");
	if (!strcmp(name, "muted"))
		return PhoneQuickBuiltin_State("sound");
	if (!strcmp(name, "clearing"))
		return clearing;
	if (!strcmp(name, "card0"))
		return (intptr_t)cards[0];
	if (!strcmp(name, "card1"))
		return (intptr_t)cards[1];
	if (!strcmp(name, "empty"))
		return !(empty->state & GY_STATE_Hidden);
	if (!strcmp(name, "notices"))
		return notices[0] + notices[1];
	if (!strcmp(name, "capture"))
		return (intptr_t)captured;
	if (!strcmp(name, "preview"))
		return preview && !(preview->state & GY_STATE_Hidden);
	return 0;
}
