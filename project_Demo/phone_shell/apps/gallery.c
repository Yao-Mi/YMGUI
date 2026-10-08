#include "phone_ui.h"
#include "phone_host.h"
#include "YMGUI_TouchGesture.h"

typedef struct
{
	GYOBJ gallery_panel, gallery_caption;
	int gallery_index;
	GYOBJ zoom_label;
	GYpx* preview;
	GYcolor preview_color;
	int zoom, anchor_zoom;
	uint32 anchor_distance;
	uint8 pinch, ids[2];
} AppState;
static AppState state;

static void on_gallery(GYOBJ btn);

static void zoom_label(void)
{
	char text[64];
	snprintf(text, sizeof(text), "双指缩放 / %d%%", state.zoom);
	PhoneUI_text_set(state.zoom_label, text);
}
static void gallery_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	if (state.zoom == 100 || !state.preview)
	{
		PhoneUI_gallery_draw(obj, surface, area);
		return;
	}
	if (state.preview_color != obj->bg_color)
	{
		GYrect full = {0, 0, 284, 230};
		GYsurface cache = {state.preview, full, full, 284};
		PhoneUI_gallery_draw(obj, &cache, &full);
		state.preview_color = obj->bg_color;
	}
	/* Keep the original antialiased rim, sample only the rounded interior. */
	PhoneUI_gallery_draw(obj, surface, area);
	GYrect hit;
	if (!GY_Rect_Intersect(&hit, area, &surface->clip) || !GY_Rect_Intersect(&hit, &hit, &surface->buf_area))
		return;
	for (int y = hit.y; y < hit.y + hit.h; ++y)
	{
		int yy = y - area->y, inset = 0;
		if (yy < 20 || yy >= area->h - 20)
		{
			int d = yy < 20 ? 39 - 2 * yy : 2 * (yy - (area->h - 20)) + 1;
			int root = (int)GY_TouchISqrt(1600 - d * d);
			inset = (40 - root + 1) / 2;
		}
		int left = area->x + inset, right = area->x + area->w - inset;
		if (left < hit.x)
			left = hit.x;
		if (right > hit.x + hit.w)
			right = hit.x + hit.w;
		int sy = 115 + (yy - area->h / 2) * 100 / state.zoom;
		GYpx* dst = surface->buf + (y - surface->buf_area.y) * surface->stride;
		for (int x = left; x < right; ++x)
		{
			int sx = 142 + (x - area->x - area->w / 2) * 100 / state.zoom;
			dst[x - surface->buf_area.x] = state.preview[sy * 284 + sx];
		}
	}
}
static void gallery_touch(const GYtouchpoint* points, uint8 count)
{
	if (count < 2)
	{
		state.pinch = 0;
		return;
	}
	if (!state.pinch)
	{
		GYrect a;
		YMGUI_Obj_GetAbsArea(state.gallery_panel, &a);
		for (int i = 0; i < 2; ++i)
			if (points[i].x < a.x || points[i].x >= a.x + a.w || points[i].y < a.y || points[i].y >= a.y + a.h)
				return;
		uint32 distance = GY_TouchDistance(points, points + 1);
		if (distance < 8)
			return;
		if (!state.preview)
		{
			state.preview = (GYpx*)GY_malloc1(284 * 230 * sizeof(GYpx));
			if (!state.preview)
				return;
			state.preview_color = 0;
		}
		state.ids[0] = points[0].id;
		state.ids[1] = points[1].id;
		state.anchor_distance = distance;
		state.anchor_zoom = state.zoom;
		state.pinch = 1;
	}
	const GYtouchpoint *a = NULL, *b = NULL;
	for (uint8 i = 0; i < count; ++i)
	{
		if (points[i].id == state.ids[0])
			a = points + i;
		if (points[i].id == state.ids[1])
			b = points + i;
	}
	if (!a || !b)
	{
		state.pinch = 0;
		return;
	}
	int zoom = state.anchor_zoom * GY_TouchDistance(a, b) / state.anchor_distance;
	if (zoom < 100)
		zoom = 100;
	if (zoom > 300)
		zoom = 300;
	if (zoom != state.zoom)
	{
		state.zoom = zoom;
		zoom_label();
		YMGUI_Obj_Invalidate(state.gallery_panel);
	}
}
static void gallery_hide(void)
{
	state.pinch = 0;
}
static void gallery_close(void)
{
	gallery_hide();
	GY_free1(state.preview);
	state.preview = NULL;
	state.zoom = 100;
	zoom_label();
	YMGUI_Obj_Invalidate(state.gallery_panel);
}



static void on_gallery(GYOBJ btn)
{
	(void)btn;
	state.zoom = 100;
	state.pinch = 0;
	zoom_label();
	state.gallery_index = (state.gallery_index + 1) % 3;
	static const GYcolor colors[3] = {RGB(196, 105, 113), RGB(83, 155, 173), RGB(149, 128, 204)};
	static const char* names[3] = {"01 / 珊瑚", "02 / 海岸", "03 / 暮色"};
	PhoneUI_text_set(state.gallery_caption, names[state.gallery_index]);
#if YMGUI_ANIM
	if (YMGUI_Anim_BgColorTo(state.gallery_panel, colors[state.gallery_index], 350, GY_ANIM_SMOOTH))
		return;
#endif
	YMGUI_Obj_SetBgColor(state.gallery_panel, colors[state.gallery_index]);
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	state.zoom = 100;
	PhoneUI_app_header(view, "相册", "收藏安静的风景");
	state.gallery_panel = PhoneUI_panel(view, 18, 65, 284, 230, RGB(196, 105, 113));
	state.gallery_panel->draw_cb = gallery_draw;
	state.gallery_panel->event_cb = NULL;
	state.gallery_caption = PhoneUI_left_label(view, 22, 307, 276, "01 / 珊瑚", INK, 3);
	state.zoom_label = PhoneUI_left_label(view, 23, 345, 275, "双指缩放 / 100%", MUTED, 2);
	PhoneUI_button(view, 18, 373, 284, 34, "下一张风景   >", on_gallery, RGB(147, 112, 136));
}

static void app_destroy(void)
{
	GY_free1(state.preview);
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "gallery_zoom"))
		return state.zoom;
	if (!strcmp(name, "gallery_pinch"))
		return state.pinch;
	if (!strcmp(name, "gallery_panel"))
		return (intptr_t)state.gallery_panel;
	if (!strcmp(name, "gallery_caption"))
		return (intptr_t)state.gallery_caption;
	if (!strcmp(name, "gallery_index"))
		return (intptr_t)state.gallery_index;
	return 0;
}

const PhoneApp PhoneApp_gallery = {
	.key = "gallery", .title = "相册", .color = RGB(239, 165, 72), .icon = ICON_GALLERY, .create = app_create, .inspect = app_inspect, .destroy = app_destroy, .hide = gallery_hide, .close = gallery_close, .touch = gallery_touch};
