#include "phone_ui.h"

static const GYfont* fonts[] = {&phone_font_regular, &phone_font_display, &phone_font_small, &phone_font_title};
static const uint8* font_advances[] = {phone_font_regular_advance, phone_font_display_advance,
									   phone_font_small_advance, phone_font_title_advance};
static const int font_heights[] = {24, 66, 18, 36};

void PhoneUI_rounded(GYSURFACE surface, const GYrect* area, GYcolor color, int radius, GYopa opa)
{
	if (radius > area->h / 2)
		radius = area->h / 2;
	if (radius > area->w / 2)
		radius = area->w / 2;
	int first = surface->clip.y > area->y ? surface->clip.y - area->y : 0;
	int last = surface->clip.y + surface->clip.h - area->y;
	if (last > area->h)
		last = area->h;
	for (int yy = first; yy < last; ++yy)
	{
		double d = yy < radius ? radius - yy - 0.5 : yy >= area->h - radius ? yy - (area->h - radius) + 0.5
																			: 0;
		double edge = d ? radius - sqrt(radius * radius - d * d) : 0;
		int inset = (int)ceil(edge);
		GYrect row = {(GYcoord)(area->x + inset), (GYcoord)(area->y + yy),
					  (GYcoord)(area->w - inset * 2), 1};
		YMGUI_Draw_Fill(surface, &row, color, opa);
		if (inset)
		{
			GYopa coverage = (GYopa)((inset - edge) * opa);
			GYrect pixel = {(GYcoord)(area->x + inset - 1), (GYcoord)(area->y + yy), 1, 1};
			YMGUI_Draw_Fill(surface, &pixel, color, coverage);
			pixel.x = area->x + area->w - inset;
			YMGUI_Draw_Fill(surface, &pixel, color, coverage);
		}
	}
}

static void panel_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	PhoneUI_rounded(surface, area, obj->bg_color, 17, GY_OPA_COVER);
}

void PhoneUI_glass_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	PhoneUI_rounded(surface, area, obj->bg_color, 22, 190);
}

static uint32 text_codepoint(const unsigned char** text)
{
	const unsigned char* p = *text;
	uint32 cp = *p++;
	int count = cp < 0x80 ? 0 : (cp & 0xe0) == 0xc0 ? 1
							: (cp & 0xf0) == 0xe0	? 2
							: (cp & 0xf8) == 0xf0	? 3
													: 0;
	if (count)
		cp &= (1u << (6 - count)) - 1;
	while (count--)
	{
		if ((*p & 0xc0) != 0x80)
		{
			*text = p;
			return '?';
		}
		cp = (cp << 6) | (*p++ & 63);
	}
	*text = p;
	return cp;
}

int PhoneUI_text_width(const char* value, uint8 large)
{
	const uint8* advances = font_advances[large];
	int width = 0;
	for (const unsigned char* p = (const unsigned char*)value; *p;)
	{
		uint32 cp = text_codepoint(&p);
		width += cp >= 32 && cp <= 126 ? advances[cp - 32] : 16;
	}
	return width;
}

void PhoneUI_draw_text(GYSURFACE surface, const char* value, GYcolor color,
					   uint8 large, int cx, int top)
{
	const GYfont* font = fonts[large];
	const uint8* advances = font_advances[large];
	int x = cx - PhoneUI_text_width(value, large) / 2;
	for (const unsigned char* p = (const unsigned char*)value; *p;)
	{
		uint32 cp = text_codepoint(&p);
		if (cp >= 32 && cp <= 126)
		{
			YMGUI_Draw_Glyph(surface, font, (GYcoord)x, (GYcoord)top, cp, color);
			x += advances[cp - 32];
		}
		else
		{
			YMGUI_Draw_Glyph(surface, &YMGUI_Font_Default, x, top + (font_heights[large] - 16) / 2, cp, color);
			x += 16;
		}
	}
}

static void text_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	PhoneText* t = (PhoneText*)obj->user_data;
	GYsurface clipped = *surface;
	if (!GY_Rect_Intersect(&clipped.clip, area, &surface->clip))
		return;
	PhoneUI_draw_text(&clipped, t->value, t->color, t->large,
					  area->x + (t->left ? PhoneUI_text_width(t->value, t->large) / 2 : area->w / 2),
					  area->y + (area->h - font_heights[t->large]) / 2);
}

static void data_free(GYOBJ obj)
{
	GY_free1(obj->user_data);
}

void PhoneUI_text_set(GYOBJ obj, const char* value)
{
	PhoneText* t = (PhoneText*)obj->user_data;
	snprintf(t->value, sizeof(t->value), "%s", value ? value : "");
	/* 固定缓冲截断时，回退到完整 UTF-8 码点。 */
	if (value && strlen(value) >= sizeof(t->value))
	{
		size_t end = sizeof(t->value) - 1;
		while (end && (((unsigned char)value[end] & 0xc0) == 0x80))
			--end;
		t->value[end] = 0;
	}
	YMGUI_Obj_Invalidate(obj);
}

GYOBJ PhoneUI_label_ex(GYOBJ parent, int x, int y, int w, const char* value,
					   GYcolor color, uint8 large)
{
	GYOBJ obj = YMGUI_Creat_Obj_Creat(parent, x, y, w, font_heights[large]);
	PhoneText* t = (PhoneText*)GY_malloc1(sizeof(*t));
	if (!obj || !t)
		return obj;
	memset(t, 0, sizeof(*t));
	t->color = color;
	t->large = large;
	obj->user_data = t;
	obj->draw_cb = text_draw;
	obj->free_cb = data_free;
	PhoneUI_text_set(obj, value);
	return obj;
}

GYOBJ PhoneUI_label(GYOBJ parent, int x, int y, int w, const char* value, GYcolor color)
{
	return PhoneUI_label_ex(parent, x, y, w, value, color, 0);
}

GYOBJ PhoneUI_left_label(GYOBJ parent, int x, int y, int w, const char* value, GYcolor color, uint8 size)
{
	GYOBJ obj = PhoneUI_label_ex(parent, x, y, w, value, color, size);
	((PhoneText*)obj->user_data)->left = 1;
	return obj;
}

void PhoneUI_button_set(GYOBJ obj, const char* value)
{
	PhoneButton* b = (PhoneButton*)obj->user_data;
	snprintf(b->value, sizeof(b->value), "%s", value ? value : "");
	YMGUI_Obj_Invalidate(obj);
}

void PhoneUI_button_icon(GYOBJ obj, uint8 icon)
{
	PhoneButton* b = (PhoneButton*)obj->user_data;
	b->icon = icon;
	YMGUI_Obj_Invalidate(obj);
}

void PhoneUI_icon_draw(GYSURFACE surface, uint8 icon, int cx, int cy, GYcolor tint)
{
	GYcolor white = RGB(250, 252, 253);
	if (icon == ICON_TASK)
	{
		for (int i = 0; i < 3; ++i)
		{
			GYrect box = {(GYcoord)(cx - 13), (GYcoord)(cy - 12 + i * 10), 5, 5};
			GYrect line = {(GYcoord)(cx - 3), (GYcoord)(cy - 11 + i * 10), 16, 2};
			YMGUI_Draw_Fill(surface, &box, white, GY_OPA_COVER);
			YMGUI_Draw_Fill(surface, &line, white, GY_OPA_COVER);
		}
	}
	else if (icon == ICON_MUSIC)
	{
		GYrect stem = {(GYcoord)(cx + 7), (GYcoord)(cy - 14), 3, 22};
		GYrect beam = {(GYcoord)(cx - 3), (GYcoord)(cy - 14), 13, 3};
		YMGUI_Draw_Fill(surface, &stem, white, GY_OPA_COVER);
		YMGUI_Draw_Fill(surface, &beam, white, GY_OPA_COVER);
		YMGUI_Draw_CircleFill(surface, cx + 2, cy + 8, 6, white);
	}
	else if (icon == ICON_GALLERY)
	{
		GYrect frame = {(GYcoord)(cx - 15), (GYcoord)(cy - 12), 30, 25};
		PhoneUI_rounded(surface, &frame, white, 5, GY_OPA_COVER);
		YMGUI_Draw_CircleFill(surface, cx + 7, cy - 4, 3, RGB(211, 125, 121));
		YMGUI_Draw_Line(surface, cx - 11, cy + 7, cx - 3, cy - 1, RGB(211, 125, 121));
		YMGUI_Draw_Line(surface, cx - 3, cy - 1, cx + 8, cy + 9, RGB(211, 125, 121));
	}
	else if (icon == ICON_SETTINGS)
	{
		for (int i = 0; i < 8; ++i)
		{
			double a = i * 3.14159265 / 4;
			GYrect tooth = {(GYcoord)(cx + cos(a) * 12 - 3), (GYcoord)(cy + sin(a) * 12 - 3), 6, 6};
			PhoneUI_rounded(surface, &tooth, white, 1, GY_OPA_COVER);
		}
		YMGUI_Draw_CircleFill(surface, cx, cy, 12, white);
		YMGUI_Draw_CircleFill(surface, cx, cy, 6, tint);
	}
	else if (icon == ICON_CLOCK)
	{
		YMGUI_Draw_ArcThick(surface, cx, cy, 12, 14, 0, 360, white);
		YMGUI_Draw_Line(surface, cx, cy, cx, cy - 8, white);
		YMGUI_Draw_Line(surface, cx, cy, cx + 6, cy + 4, white);
	}
	else if (icon == ICON_WEATHER)
	{
		YMGUI_Draw_CircleFill(surface, cx - 3, cy - 4, 10, RGB(255, 222, 125));
		YMGUI_Draw_CircleFill(surface, cx + 6, cy + 5, 9, white);
		YMGUI_Draw_CircleFill(surface, cx - 5, cy + 9, 6, white);
	}
	else if (icon == ICON_NOTES || icon == ICON_CALCULATOR)
	{
		GYrect paper = {cx - 12, cy - 15, 24, 30};
		PhoneUI_rounded(surface, &paper, white, 3, GY_OPA_COVER);
		GYcolor ink = tint;
		for (int i = 0; i < 3; ++i)
		{
			GYrect line = {cx - 7, cy - 8 + i * 8, 14, 2};
			YMGUI_Draw_Fill(surface, &line, ink, GY_OPA_COVER);
			if (icon == ICON_CALCULATOR && i)
			{
				line.x = cx - 1;
				line.y -= 2;
				line.w = 2;
				line.h = 6;
				YMGUI_Draw_Fill(surface, &line, ink, GY_OPA_COVER);
			}
		}
	}
	else if (icon == ICON_PHONE)
	{
		YMGUI_Draw_ArcThick(surface, cx + 7, cy - 6, 16, 21, 100, 190, white);
		GYrect cap = {cx - 14, cy - 14, 10, 9};
		PhoneUI_rounded(surface, &cap, white, 3, 255);
		cap = (GYrect){cx + 2, cy + 5, 12, 10};
		PhoneUI_rounded(surface, &cap, white, 3, 255);
	}
	else if (icon == ICON_MESSAGES)
	{
		GYrect bubble = {cx - 15, cy - 12, 30, 23};
		PhoneUI_rounded(surface, &bubble, white, 6, 255);
		YMGUI_Draw_Line(surface, cx - 7, cy + 10, cx - 11, cy + 16, white);
		for (int i = 0; i < 3; ++i)
			YMGUI_Draw_CircleFill(surface, cx - 8 + i * 8, cy, 2, tint);
	}
	else if (icon == ICON_CONTACTS)
	{
		GYrect card = {cx - 13, cy - 15, 26, 30};
		PhoneUI_rounded(surface, &card, white, 4, 255);
		YMGUI_Draw_CircleFill(surface, cx, cy - 6, 5, tint);
		GYrect person = {cx - 8, cy + 1, 16, 10};
		PhoneUI_rounded(surface, &person, tint, 5, 255);
	}
	else if (icon == ICON_RECORDER)
	{
		GYrect mic = {cx - 5, cy - 15, 10, 22};
		PhoneUI_rounded(surface, &mic, white, 5, 255);
		YMGUI_Draw_ArcThick(surface, cx, cy, 9, 11, 0, 180, white);
		GYrect stem = {cx - 1, cy + 9, 2, 6};
		YMGUI_Draw_Fill(surface, &stem, white, 255);
		stem = (GYrect){cx - 7, cy + 14, 14, 2};
		YMGUI_Draw_Fill(surface, &stem, white, 255);
	}
	else if (icon == ICON_FILES)
	{
		GYrect folder = {cx - 15, cy - 7, 30, 21};
		PhoneUI_rounded(surface, &folder, white, 3, 255);
		folder = (GYrect){cx - 15, cy - 13, 13, 10};
		PhoneUI_rounded(surface, &folder, white, 2, 255);
	}
	else if (icon == ICON_GYROSCOPE)
	{
		YMGUI_Draw_ArcThick(surface, cx, cy, 13, 15, 0, 360, white);
		YMGUI_Draw_Line(surface, cx - 17, cy, cx + 17, cy, white);
		YMGUI_Draw_Line(surface, cx, cy - 17, cx, cy + 17, white);
		YMGUI_Draw_CircleFill(surface, cx + 4, cy - 4, 4, RGB(255, 225, 145));
	}
	else if (icon == ICON_CAMERA)
	{
		GYrect camera = {cx - 16, cy - 10, 32, 23};
		PhoneUI_rounded(surface, &camera, white, 4, 255);
		camera = (GYrect){cx - 9, cy - 15, 13, 8};
		PhoneUI_rounded(surface, &camera, white, 2, 255);
		YMGUI_Draw_CircleFill(surface, cx, cy + 1, 8, tint);
		YMGUI_Draw_CircleFill(surface, cx, cy + 1, 4, white);
	}
	else if (icon == ICON_BROWSER)
	{
		YMGUI_Draw_ArcThick(surface, cx, cy, 13, 15, 0, 360, white);
		YMGUI_Draw_Line(surface, cx - 13, cy, cx + 13, cy, white);
		YMGUI_Draw_Line(surface, cx - 10, cy - 7, cx + 10, cy - 7, white);
		YMGUI_Draw_Line(surface, cx - 10, cy + 7, cx + 10, cy + 7, white);
		YMGUI_Draw_Line(surface, cx - 4, cy - 12, cx - 4, cy + 12, white);
		YMGUI_Draw_Line(surface, cx + 4, cy - 12, cx + 4, cy + 12, white);
	}
	else if (icon == ICON_HOME)
	{
		YMGUI_Draw_ArcThick(surface, cx, cy, 6, 8, 0, 360, white);
	}
	else if (icon == ICON_RECENTS)
	{
		GYrect box = {(GYcoord)(cx - 6), (GYcoord)(cy - 6), 12, 12};
		PhoneUI_rounded(surface, &box, white, 2, GY_OPA_COVER);
	}
	else if (icon == ICON_BACK)
	{
		YMGUI_Draw_Line(surface, cx + 4, cy - 7, cx - 5, cy, white);
		YMGUI_Draw_Line(surface, cx - 5, cy, cx + 4, cy + 7, white);
	}
	else if (icon == ICON_PLAY || icon == ICON_NEXT || icon == ICON_PREVIOUS)
	{
		int dir = icon == ICON_PREVIOUS ? -1 : 1;
		for (int i = 0; i < 15; ++i)
			YMGUI_Draw_Line(surface, cx + dir * (i - 6), cy - (14 - i) / 2,
							cx + dir * (i - 6), cy + (14 - i) / 2, white);
		if (icon != ICON_PLAY)
		{
			GYrect line = {(GYcoord)(cx + dir * 10), (GYcoord)(cy - 8), 2, 16};
			YMGUI_Draw_Fill(surface, &line, white, GY_OPA_COVER);
		}
	}
	else if (icon == ICON_PAUSE)
	{
		GYrect a = {(GYcoord)(cx - 7), (GYcoord)(cy - 8), 5, 16};
		PhoneUI_rounded(surface, &a, white, 1, GY_OPA_COVER);
		a.x += 10;
		PhoneUI_rounded(surface, &a, white, 1, GY_OPA_COVER);
	}
	else if (icon == ICON_POWER)
	{
		YMGUI_Draw_ArcThick(surface, cx, cy, 9, 11, 36, 322, white);
		GYrect stem = {(GYcoord)(cx - 1), (GYcoord)(cy - 14), 3, 12};
		YMGUI_Draw_Fill(surface, &stem, white, GY_OPA_COVER);
	}
}

void PhoneUI_icon_tile_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	PhoneUI_rounded(surface, area, obj->bg_color, 12, GY_OPA_COVER);
	PhoneUI_icon_draw(surface, (uint8)(intptr_t)obj->user_data,
					  area->x + area->w / 2, area->y + area->h / 2, obj->bg_color);
}

static void button_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	PhoneButton* b = (PhoneButton*)obj->user_data;
	GYcolor color = (obj->state & GY_STATE_Pressed) ? RGB(99, 135, 153) : obj->bg_color;
	if (color)
		PhoneUI_rounded(surface, area, color, area->h >= 50 ? 15 : 12, GY_OPA_COVER);
	if (b->icon)
	{
		PhoneUI_icon_draw(surface, b->icon, area->x + area->w / 2,
						  area->y + (b->value[0] ? area->h / 2 - 8 : area->h / 2), obj->bg_color);
		if (b->value[0])
			PhoneUI_draw_text(surface, b->value, RGB(237, 248, 252), 0,
							  area->x + area->w / 2, area->y + area->h - 20);
	}
	else
		PhoneUI_draw_text(surface, b->value, RGB(250, 252, 253), 0,
						  area->x + area->w / 2, area->y + (area->h - 24) / 2);
}

void PhoneUI_button_event(GYOBJ obj, GYEvent event)
{
	PhoneButton* b = (PhoneButton*)obj->user_data;
	if (event == GY_EVENT_Clicked && b->clicked)
		b->clicked(obj);
	if (event == GY_EVENT_Pressed || event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
		YMGUI_Obj_Invalidate(obj);
}

GYOBJ PhoneUI_button(GYOBJ parent, int x, int y, int w, int h,
					 const char* value, GYbtn_clicked_cb cb, GYcolor color)
{
	GYOBJ obj = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	PhoneButton* b = (PhoneButton*)GY_malloc1(sizeof(*b));
	if (!obj || !b)
		return obj;
	memset(b, 0, sizeof(*b));
	b->clicked = cb;
	obj->user_data = b;
	obj->draw_cb = button_draw;
	obj->event_cb = PhoneUI_button_event;
	obj->free_cb = data_free;
	YMGUI_Obj_SetBgColor(obj, color);
	PhoneUI_button_set(obj, value);
	return obj;
}

void PhoneUI_set_pos(GYOBJ obj, GYcoord x, GYcoord y)
{
	obj->area.x = x;
	obj->area.y = y;
	YMGUI_Obj_Invalidate(obj->ctx->root);
}

void PhoneUI_move(GYOBJ obj, GYcoord x, GYcoord y, uint32 duration)
{
#if YMGUI_ANIM
	if (YMGUI_Anim_MoveTo(obj, x, y, duration, GY_ANIM_EASE_OUT))
		return;
#else
	(void)duration;
#endif
	PhoneUI_set_pos(obj, x, y);
}

void PhoneUI_gallery_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	GYsurface clipped = *surface;
	int x0 = clipped.clip.x > area->x ? clipped.clip.x : area->x;
	int y0 = clipped.clip.y > area->y ? clipped.clip.y : area->y;
	int x1 = clipped.clip.x + clipped.clip.w < area->x + area->w ? clipped.clip.x + clipped.clip.w : area->x + area->w;
	int y1 = clipped.clip.y + clipped.clip.h < area->y + area->h ? clipped.clip.y + clipped.clip.h : area->y + area->h;
	if (x1 <= x0 || y1 <= y0)
		return;
	clipped.clip = (GYrect){(GYcoord)x0, (GYcoord)y0, (GYcoord)(x1 - x0), (GYcoord)(y1 - y0)};
	PhoneUI_rounded(&clipped, area, obj->bg_color, 20, GY_OPA_COVER);
	YMGUI_Draw_CircleFill(&clipped, area->x + 205, area->y + 64, 31, RGB(255, 213, 169));
	GYrect horizon = {area->x, (GYcoord)(area->y + 132), area->w, 75};
	YMGUI_Draw_Fill(&clipped, &horizon, RGB(54, 83, 112), GY_OPA_COVER);
	YMGUI_Draw_CircleFill(&clipped, area->x + 48, area->y + 206, 94, RGB(38, 72, 96));
	YMGUI_Draw_CircleFill(&clipped, area->x + 241, area->y + 237, 116, RGB(27, 59, 83));
	for (int yy = area->h - 20; yy < area->h; ++yy)
	{
		int d = yy - (area->h - 20);
		int inset = 20 - (int)sqrt((double)(400 - d * d));
		if (inset < 1)
			continue;
		GYrect left = {area->x, (GYcoord)(area->y + yy), (GYcoord)inset, 1};
		GYrect right = {(GYcoord)(area->x + area->w - inset), (GYcoord)(area->y + yy),
						(GYcoord)inset, 1};
		YMGUI_Draw_Fill(&clipped, &left, PAPER, GY_OPA_COVER);
		YMGUI_Draw_Fill(&clipped, &right, PAPER, GY_OPA_COVER);
	}
}

void PhoneUI_switch_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	int on = YMGUI_Switch_GetOn(obj);
	PhoneUI_rounded(s, a, on ? ACCENT : RGB(204, 209, 221), a->h / 2, GY_OPA_COVER);
	YMGUI_Draw_CircleFill(s, a->x + (on ? a->w - a->h / 2 : a->h / 2),
						  a->y + a->h / 2, a->h / 2 - 3, WHITE);
}

void PhoneUI_slider_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	int x = a->x + a->h / 2 + (a->w - a->h) * YMGUI_Slider_GetValue(obj) / 100;
	GYrect track = {a->x, a->y + a->h / 2 - 2, a->w, 4};
	PhoneUI_rounded(s, &track, RGB(214, 217, 229), 2, GY_OPA_COVER);
	track.w = x - a->x;
	PhoneUI_rounded(s, &track, ACCENT, 2, GY_OPA_COVER);
	YMGUI_Draw_CircleFill(s, x, a->y + a->h / 2, 7, ACCENT);
	YMGUI_Draw_CircleFill(s, x, a->y + a->h / 2, 3, WHITE);
}

GYOBJ PhoneUI_panel(GYOBJ parent, int x, int y, int w, int h, GYcolor color)
{
	GYOBJ obj = YMGUI_Creat_Obj_Creat(parent, x, y, w, h);
	obj->bg_color = color;
	obj->draw_cb = panel_draw;
	return obj;
}

void PhoneUI_weather_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	int x = a->x + 26, y = a->y + 24;
	YMGUI_Draw_CircleFill(s, x, y, 13, RGB(255, 214, 137));
	YMGUI_Draw_CircleFill(s, x + 11, y + 11, 10, WHITE);
	YMGUI_Draw_CircleFill(s, x - 1, y + 15, 8, WHITE);
	GYrect cloud = {x - 1, y + 12, 21, 10};
	PhoneUI_rounded(s, &cloud, WHITE, 5, GY_OPA_COVER);
}

void PhoneUI_app_header(GYOBJ view, const char* title, const char* subtitle)
{
	PhoneUI_left_label(view, 22, 0, 276, title, INK, 3);
	PhoneUI_left_label(view, 23, 37, 275, subtitle, MUTED, 2);
}

void PhoneUI_phone_input_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	/* 只换皮肤；输入、光标定位、选区和滚动仍由 TextInput 管理。 */
	GYcolor line = obj->state & GY_STATE_Focused ? ACCENT : RGB(218, 214, 204);
	YMGUI_Draw_Fill(s, a, WHITE, 255);
	GYrect border = {a->x, a->y + a->h - 2, a->w, 2};
	YMGUI_Draw_Fill(s, &border, line, 255);
	GYsurface clipped = *s;
	int x0 = a->x + 2 > s->clip.x ? a->x + 2 : s->clip.x;
	int y0 = a->y + 2 > s->clip.y ? a->y + 2 : s->clip.y;
	int x1 = a->x + a->w - 2 < s->clip.x + s->clip.w ? a->x + a->w - 2 : s->clip.x + s->clip.w;
	int y1 = a->y + a->h - 2 < s->clip.y + s->clip.h ? a->y + a->h - 2 : s->clip.y + s->clip.h;
	if (x1 <= x0 || y1 <= y0)
		return;
	clipped.clip = (GYrect){x0, y0, x1 - x0, y1 - y0};
	const char* text = YMGUI_TextInput_GetText(obj);
	int tx = a->x + 5 - YMGUI_TextInput_GetScrollX(obj), ty = a->y + (a->h - 16) / 2;
	size_t start, end;
	YMGUI_TextInput_GetSelection(obj, &start, &end);
	int cursor = tx + YMGUI_Font_TextWidthN(&YMGUI_Font_Default, text, (uint32)start);
	if (start != end)
	{
		GYrect selection = {cursor, ty, YMGUI_Font_TextWidthN(&YMGUI_Font_Default, text + start, (uint32)(end - start)), 16};
		YMGUI_Draw_Fill(&clipped, &selection, RGB(205, 214, 252), 255);
	}
	else if (obj->state & GY_STATE_Editing)
	{
		GYrect caret = {cursor, ty, 1, 16};
		YMGUI_Draw_Fill(&clipped, &caret, ACCENT, 255);
	}
	YMGUI_Draw_Text(&clipped, &YMGUI_Font_Default, tx, ty, text, INK);
}

#if YMGUI_ANIM
static void bar_value(int32 value, void* user)
{
	YMGUI_Bar_SetValue((GYOBJ)user, value);
}
void PhoneUI_animate_bar(GYOBJ bar, int value, uint32 duration)
{
	if (!YMGUI_Anim_ValueTo(bar->ctx, bar, YMGUI_Bar_GetValue(bar), value,
							duration, GY_ANIM_SMOOTH, bar_value, bar))
		YMGUI_Bar_SetValue(bar, value);
}
#else
void PhoneUI_animate_bar(GYOBJ bar, int value, uint32 duration)
{
	(void)duration;
	YMGUI_Bar_SetValue(bar, value);
}
#endif


GYOBJ PhoneUI_app_text(GYOBJ parent, int x, int y, int w, int h, const char* value)
{
	GYOBJ obj = YMGUI_Creat_TextView_Creat(parent, x, y, w, h);
	YMGUI_TextView_SetWrap(obj, 1);
	YMGUI_TextView_SetLineHeight(obj, 24);
	YMGUI_TextView_SetTextColor(obj, RGB(234, 238, 247));
	YMGUI_TextView_SetText(obj, value);
	return obj;
}
GYOBJ PhoneUI_app_input(GYOBJ view, int x, int y, int w, const char* value, size_t cap)
{
	GYOBJ obj = YMGUI_Creat_TextInput_Creat(view, x, y, w, 34, cap);
	obj->draw_cb = PhoneUI_phone_input_draw;
	YMGUI_TextInput_SetText(obj, value);
	return obj;
}
