#include "YMGUI_TextActions.h"
#include "phone_ime.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Event.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_Font.h"
#include "YMGUI_Geom.h"
#include "YMGUI_Anim.h"
#include "ime_candidates.inc"
#include "ime_symbols.h"

#define RGB(r, g, b) GY_ARGB(255, r, g, b)
#define KEYBOARD_Y 226
typedef struct InputTarget
{
	GYOBJ obj;
	GYobj_free_cb original_free;
	int multiline;
	struct InputTarget* next;
} InputTarget;
typedef struct
{
	GYOBJ obj;
	char text[96];
	int action;
} Key;
enum
{
	MODE = 0x2000,
	SYMBOLS,
	NUMBERS,
	SHIFT,
	SPACE,
	PREV,
	NEXT,
	CANDIDATE = 0x2100,
	SYMBOL_CATEGORY = 0x2200,
	SYMBOL_ITEM = 0x2300
};
static GYCTX context;
static GYOBJ keyboard, query_label;
static InputTarget *inputs, *active;
static GYrect original_area;
static Key keys[48], candidates[5], candidate_prev, candidate_next;
static Key symbol_categories[7], symbol_keys[15], symbol_backspace;
static int symbol_panel, symbol_category, symbol_page;
static int key_count, visible, symbols, upper, bypass, page_start, page_count;
static char composition[IME_QUERY_CAP];
static void refresh_symbols(void);
static void update_letters(void);
static int is_child(GYOBJ obj, GYOBJ ancestor)
{
	for (; obj; obj = obj->parent)
		if (obj == ancestor)
			return 1;
	return 0;
}
static int usable(GYOBJ obj)
{
	for (; obj; obj = obj->parent)
		if (obj->state & GY_STATE_Hidden)
			return 0;
	return 1;
}
static InputTarget* lookup(GYOBJ obj)
{
	for (InputTarget* p = inputs; p; p = p->next)
		if (p->obj == obj)
			return p;
	return NULL;
}
static void restore_area(void)
{
	if (!active)
		return;
	YMGUI_Obj_Invalidate(active->obj);
	active->obj->area = original_area;
	YMGUI_Obj_Invalidate(active->obj);
}
static void focus_target(void)
{
	if (!active)
		return;
	YMGUI_SetFocus(context, active->obj);
	active->obj->state |= GY_STATE_Editing;
}
static void send_key(uint32 key)
{
	if (!active)
		return;
	focus_target();
	bypass = 1;
	YMGUI_Inject_Key(key, 1);
	YMGUI_Inject_Key(key, 0);
	bypass = 0;
}
static void insert(const char* text)
{
	if (!active)
		return;
	focus_target();
	if (active->multiline)
		YMGUI_EditView_InsertText(active->obj, text);
	else
		for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
			send_key(*p);
}
static void key_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	Key* key = obj->user_data;
	GYsurface clipped = *s;
	GYrect hit;
	if (!GY_Rect_Intersect(&hit, a, &s->clip))
		return;
	clipped.clip = hit;
	GYcolor bg = obj->state & GY_STATE_Pressed ? RGB(197, 204, 231) : RGB(255, 255, 255);
	if (symbol_panel && key->action == SYMBOL_CATEGORY + symbol_category)
		bg = RGB(197, 204, 231);
	YMGUI_Draw_Fill(&clipped, a, bg, 255);
	GYrect bottom = {a->x, a->y + a->h - 1, a->w, 1};
	YMGUI_Draw_Fill(&clipped, &bottom, RGB(186, 192, 203), 255);
	int w = YMGUI_Font_TextWidth(&YMGUI_Font_Default, key->text);
	YMGUI_Draw_Text(&clipped, &YMGUI_Font_Default, a->x + (a->w - w) / 2, a->y + (a->h - 16) / 2,
					key->text, RGB(40, 47, 68));
}
static void query_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	const char* text = composition[0] ? composition : (g_english ? "英文输入" : "拼音输入");
	char caption[96];
	if (symbol_panel)
	{
		snprintf(caption, sizeof(caption), "%s%s %d/%d", g_english ? "英" : "中",
				 s_symbol_set_names[symbol_category], symbol_page + 1,
				 (imeSymbolCount(symbol_category, g_english) + 14) / 15);
		text = caption;
	}
	YMGUI_Draw_Text(s, &YMGUI_Font_Default, a->x, a->y, text, RGB(83, 91, 140));
}
static void refresh_candidates(void)
{
	/* 空组合把两个窄翻页键合为一个收起键，文字和命中区域同步切换。 */
	int composing = composition[0] != 0 || symbol_panel;
	candidate_prev.obj->area.w = composing ? 24 : 54;
	candidate_prev.obj->area.y = candidate_next.obj->area.y = symbol_panel ? 0 : 24;
	candidate_prev.obj->area.h = candidate_next.obj->area.h = symbol_panel ? 22 : 35;
	snprintf(candidate_prev.text, sizeof(candidate_prev.text), "%s", composing ? "<" : "收起");
	YMGUI_Obj_SetHidden(candidate_next.obj, !composing);
	page_count = 0;
	int x = 5;
	for (int i = 0; i < 5; ++i)
		YMGUI_Obj_SetHidden(candidates[i].obj, 1);
	if (composition[0] && !symbol_panel)
	{
		for (int i = 0; i < 5; ++i)
		{
			extendPrefixCandidates(page_start + i + 1);
			if (page_start + i >= g_word_count)
				break;
			const char* word = g_words[page_start + i];
			int width = YMGUI_Font_TextWidth(&YMGUI_Font_Default, word) + 18;
			if (width > 247)
				width = 247;
			if (x + width > 255)
				break;
			Key* key = &candidates[i];
			key->obj->area = (GYrect){x, 24, width, 28};
			snprintf(key->text, sizeof(key->text), "%s", word);
			YMGUI_Obj_SetHidden(key->obj, 0);
			x += width + 3;
			page_count++;
		}
	}
	YMGUI_Obj_Invalidate(keyboard);
}
static void reset_query(void)
{
	page_start = 0;
	imeCandidatesReset(composition);
	refresh_candidates();
}
static void commit(int index)
{
	if (index >= 0 && index < page_count)
	{
		const char* word = g_words[page_start + index];
		insert(word);
		learnWord(word);
	}
	else if (composition[0])
		insert(composition);
	composition[0] = 0;
	reset_query();
}
int PhoneIME_Hide(void)
{
	int was_visible = visible;
	if (!context)
		return 0;
	visible = 0;
	YMGUI_Inject_SetKeyFilter(NULL, NULL);
	if (active && context->focus_obj == active->obj)
		YMGUI_SetFocus(context, NULL);
	restore_area();
	active = NULL;
	composition[0] = 0;
	if (keyboard)
	{
#if YMGUI_ANIM
		YMGUI_Anim_CancelObject(keyboard);
#endif
		YMGUI_Obj_SetHidden(keyboard, 1);
	}
	return was_visible;
}
int PhoneIME_Visible(void)
{
	return visible;
}
GYOBJ PhoneIME_Target(void)
{
	return active ? active->obj : NULL;
}

static void update_letters(void)
{
	const char* letters = "qwertyuiopasdfghjklzxcvbnm";
	const char* digits = "1234567890-/:;()$&@.,?!'\"+=%";
	for (int i = 0; i < 26; ++i)
	{
		char c = symbols ? digits[i] : letters[i];
		if (!symbols && upper)
			c -= 'a' - 'A';
		keys[i].action = c;
		keys[i].text[0] = c;
		keys[i].text[1] = 0;
	}
	for (int i = 0; i < 28; ++i)
		YMGUI_Obj_SetHidden(keys[i].obj, symbol_panel);
	snprintf(keys[28].text, sizeof(keys[28].text), "%s", symbol_panel ? "ABC" : "符号");
	refresh_symbols();
	YMGUI_Obj_Invalidate(keyboard);
}
static void refresh_symbols(void)
{
	int count = imeSymbolCount(symbol_category, g_english);
	for (int i = 0; i < 7; ++i)
		YMGUI_Obj_SetHidden(symbol_categories[i].obj, !symbol_panel);
	YMGUI_Obj_SetHidden(symbol_backspace.obj, !symbol_panel);
	for (int i = 0; i < 15; ++i)
	{
		int index = symbol_page * 15 + i;
		YMGUI_Obj_SetHidden(symbol_keys[i].obj, !symbol_panel || index >= count);
		if (index < count)
			snprintf(symbol_keys[i].text, sizeof(symbol_keys[i].text), "%s", imeSymbolAt(symbol_category, index, g_english));
	}
	refresh_candidates();
}
static void action(int code)
{
	if (!active)
		return;
	focus_target();
	if (code >= SYMBOL_ITEM && code < SYMBOL_ITEM + 15)
	{
		const char* text = imeSymbolAt(symbol_category, symbol_page * 15 + code - SYMBOL_ITEM, g_english);
		if (text)
			insert(text);
		return;
	}
	if (code >= SYMBOL_CATEGORY && code < SYMBOL_CATEGORY + 7)
	{
		symbol_category = code - SYMBOL_CATEGORY;
		symbol_page = 0;
		refresh_symbols();
		return;
	}
	if (code >= CANDIDATE && code < CANDIDATE + 5)
	{
		commit(code - CANDIDATE);
		return;
	}
	if (code == 27 || ((code == PREV || code == NEXT) && !composition[0] && !symbol_panel))
	{
		PhoneIME_Hide();
		return;
	}
	if (code == MODE)
	{
		commit(-1);
		g_english = !g_english;
		symbols = 0;
		symbol_page = 0;
		update_letters();
		return;
	}
	if (code == SYMBOLS)
	{
		if (composition[0])
			commit(page_count ? 0 : -1);
		symbol_panel = !symbol_panel;
		symbols = 0;
		symbol_page = 0;
		update_letters();
		return;
	}
	if (code == NUMBERS)
	{
		commit(-1);
		symbol_panel = 0;
		symbols = !symbols;
		update_letters();
		return;
	}
	if (symbol_panel && (code == NEXT || code == PREV))
	{
		int last = (imeSymbolCount(symbol_category, g_english) - 1) / 15;
		if (code == NEXT && symbol_page < last)
			++symbol_page;
		if (code == PREV && symbol_page > 0)
			--symbol_page;
		refresh_symbols();
		return;
	}
	if (code == SHIFT)
	{
		upper = !upper;
		update_letters();
		return;
	}
	if (code == NEXT)
	{
		extendPrefixCandidates(page_start + page_count + 1);
		if (page_count && page_start + page_count < g_word_count)
			page_start += page_count;
		refresh_candidates();
		return;
	}
	if (code == PREV)
	{
		int start = 0, previous = 0;
		while (start < page_start)
		{
			previous = start;
			int x = 5, count = 0;
			while (start < page_start && count < 5)
			{
				int width = YMGUI_Font_TextWidth(&YMGUI_Font_Default, g_words[start]) + 18;
				if (width > 247)
					width = 247;
				if (x + width > 255)
					break;
				x += width + 3;
				start++;
				count++;
			}
		}
		page_start = previous;
		refresh_candidates();
		return;
	}
	size_t n = strlen(composition);
	if (code == GY_KEY_BACKSPACE && n)
	{
		composition[n - 1] = 0;
		reset_query();
		return;
	}
	if (code == GY_KEY_ENTER)
	{
		/* 回车不是另一个选词键：先上屏，再交给目标控件换行或提交。 */
		if (n)
			commit(page_count ? 0 : -1);
		send_key(GY_KEY_ENTER);
		if (active && !active->multiline)
			PhoneIME_Hide();
		return;
	}
	if (code == SPACE || code == ' ')
	{
		if (n)
			commit(page_count ? 0 : -1);
		else
			insert(" ");
		return;
	}
	if (!g_english && code >= '1' && code <= '5' && n && code - '1' < page_count)
	{
		commit(code - '1');
		return;
	}
	if (!g_english && !symbols && !symbol_panel && ((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')))
	{
		if (n < sizeof(composition) - 1)
		{
			composition[n] = code >= 'a' ? code : code + 'a' - 'A';
			composition[n + 1] = 0;
			reset_query();
		}
		return;
	}
	if (n && code >= 32 && code < 127)
		commit(page_count ? 0 : -1);
	send_key((uint32)code);
}
static uint8 filter(uint32 key, uint8 pressed, void* user)
{
	(void)user;
	if (bypass || !visible || !active || context->focus_obj != active->obj)
		return 0;
	if (key == GY_KEY_TAB || key >= GY_KEY_LEFT)
		return 0;
	if (pressed)
		action((int)key);
	return 1;
}
static void keyboard_event(GYOBJ obj, GYEvent event)
{
	if (event == GY_EVENT_Pressed || event == GY_EVENT_ReleasedOff || event == GY_EVENT_Released)
	{
		/* 原生指针按下会清除旧焦点；软键盘按键不成为新的输入目标。 */
		focus_target();
		YMGUI_Obj_Invalidate(obj);
	}
}
static void key_event(GYOBJ obj, GYEvent event)
{
	keyboard_event(obj, event);
	if (event == GY_EVENT_Clicked)
		action(((Key*)obj->user_data)->action);
}
static void make_key(Key* key, int x, int y, int w, const char* text, int code)
{
	key->obj = YMGUI_Creat_Obj_Creat(keyboard, x, y, w, 35);
	if (!key->obj)
		return;
	snprintf(key->text, sizeof(key->text), "%s", text);
	key->action = code;
	key->obj->user_data = key;
	key->obj->draw_cb = key_draw;
	key->obj->event_cb = key_event;
}
void PhoneIME_Update(int enabled)
{
	YMGUI_TextActions_Update(context, visible ? KEYBOARD_Y : 448, (uint8)enabled);
	if (!context || !keyboard)
		return;
	if (!enabled)
	{
		if (visible)
			PhoneIME_Hide();
		return;
	}
	InputTarget* next = lookup(context->focus_obj);
	if (!next || !usable(next->obj))
	{
		if (visible && !is_child(context->pressed_obj, keyboard))
			PhoneIME_Hide();
		return;
	}
	if (next == active && visible)
		return;
	restore_area();
	active = next;
	original_area = active->obj->area;
	composition[0] = 0;
	symbol_panel = 0;
	update_letters();
	reset_query();
	GYrect abs;
	YMGUI_Obj_GetAbsArea(active->obj, &abs);
	if (abs.y + abs.h > KEYBOARD_Y - 6)
	{
		if (abs.y > KEYBOARD_Y - 40)
		{
			active->obj->area.y -= abs.y - (KEYBOARD_Y - 44);
			abs.y = KEYBOARD_Y - 44;
		}
		active->obj->area.h = KEYBOARD_Y - 6 - abs.y;
		YMGUI_Obj_Invalidate(context->root);
	}
	int opening = !visible;
	visible = 1;
	YMGUI_Obj_SetHidden(keyboard, 0);
	if (opening)
	{
		keyboard->area.y = KEYBOARD_Y;
#if YMGUI_ANIM
		keyboard->area.y = 448;
		if (!YMGUI_Anim_MoveTo(keyboard, 0, KEYBOARD_Y, 180, GY_ANIM_EASE_OUT))
			keyboard->area.y = KEYBOARD_Y;
#endif
		YMGUI_Obj_Invalidate(keyboard);
	}
	YMGUI_Inject_SetKeyFilter(filter, NULL);
}
static void input_free(GYOBJ obj)
{
	InputTarget** link = &inputs;
	while (*link && (*link)->obj != obj)
		link = &(*link)->next;
	if (!*link)
		return;
	InputTarget* entry = *link;
	if (active == entry)
		PhoneIME_Hide();
	*link = entry->next;
	GYobj_free_cb callback = entry->original_free;
	GY_free1(entry);
	if (callback)
		callback(obj);
}
static GYOBJ bind(GYOBJ obj, int multiline)
{
	if (!obj)
		return NULL;
	InputTarget* p = GY_malloc1(sizeof(*p));
	if (!p)
	{
		YMGUI_Free_ObjFree(obj);
		return NULL;
	}
	*p = (InputTarget){obj, obj->free_cb, multiline, inputs};
	inputs = p;
	obj->free_cb = input_free;
	return obj;
}
GYOBJ PhoneIME_TextInput(GYOBJ p, GYcoord x, GYcoord y, GYcoord w, GYcoord h, size_t cap)
{
	return bind(YMGUI_Creat_TextInput_Creat(p, x, y, w, h, cap), 0);
}
GYOBJ PhoneIME_EditView(GYOBJ p, GYcoord x, GYcoord y, GYcoord w, GYcoord h, size_t cap)
{
	return bind(YMGUI_Creat_EditView_Creat(p, x, y, w, h, cap), 1);
}
int PhoneIME_Init(GYCTX ctx)
{
	if (!ctx || context)
		return 0;
	if (!allocateCandidateWorkspace())
		return 0;
	context = ctx;
	key_count = visible = symbols = upper = bypass = page_start = page_count = 0;
	symbol_panel = symbol_category = symbol_page = 0;
	loadCharDictionary();
	loadPhraseDictionary();
	loadEnglishDictionary();
	if (S_CHAR_COUNT)
		s_char_matches = GY_malloc1(S_CHAR_COUNT * sizeof(*s_char_matches));
	if (S_CHAR_COUNT && !s_char_matches)
	{
		PhoneIME_Shutdown();
		return 0;
	}
	keyboard = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, KEYBOARD_Y, 320, 222);
	if (!keyboard)
	{
		PhoneIME_Shutdown();
		return 0;
	}
	keyboard->state |= GY_STATE_ClipChildren;
	keyboard->event_cb = keyboard_event;
	YMGUI_Obj_SetBgColor(keyboard, RGB(227, 231, 239));
	query_label = YMGUI_Creat_Obj_Creat(keyboard, 8, 4, 304, 18);
	if (!query_label)
	{
		PhoneIME_Shutdown();
		return 0;
	}
	query_label->draw_cb = query_draw;
	query_label->event_cb = keyboard_event;
	for (int i = 0; i < 5; ++i)
		make_key(&candidates[i], 5, 24, 45, "", CANDIDATE + i);
	for (int i = 0; i < 26; ++i)
	{
		int row = i < 10 ? 0 : i < 19 ? 1
									  : 2;
		int col = i - (row == 0 ? 0 : row == 1 ? 10
											   : 19);
		make_key(&keys[key_count++], (row == 0 ? 5 : row == 1 ? 20
															  : 43) +
										 col * (row == 2 ? 30 : 31),
				 58 + row * 40, 28, "", 0);
	}
	make_key(&keys[key_count++], 5, 138, 35, "Aa", SHIFT);
	make_key(&keys[key_count++], 257, 138, 58, "退格", GY_KEY_BACKSPACE);
	make_key(&keys[key_count++], 5, 178, 45, "符号", SYMBOLS);
	make_key(&keys[key_count++], 54, 178, 40, "123", NUMBERS);
	make_key(&keys[key_count++], 98, 178, 105, "空格", SPACE);
	make_key(&keys[key_count++], 207, 178, 60, "中/英", MODE);
	make_key(&keys[key_count++], 271, 178, 44, "回车", GY_KEY_ENTER);
	make_key(&candidate_prev, 260, 24, 24, "<", PREV);
	make_key(&candidate_next, 290, 24, 24, ">", NEXT);
	for (int i = 0; i < 7; ++i)
	{
		make_key(&symbol_categories[i], 5 + i % 4 * 79, 26 + i / 4 * 26, 73,
				 s_symbol_set_names[i], SYMBOL_CATEGORY + i);
		if (!symbol_categories[i].obj)
		{
			PhoneIME_Shutdown();
			return 0;
		}
		symbol_categories[i].obj->area.h = 23;
	}
	make_key(&symbol_backspace, 242, 52, 73, "退格", GY_KEY_BACKSPACE);
	if (!symbol_backspace.obj)
	{
		PhoneIME_Shutdown();
		return 0;
	}
	symbol_backspace.obj->area.h = 23;
	for (int i = 0; i < 15; ++i)
	{
		make_key(&symbol_keys[i], 5 + i % 5 * 63, 80 + i / 5 * 32, 58, "", SYMBOL_ITEM + i);
		if (!symbol_keys[i].obj)
		{
			PhoneIME_Shutdown();
			return 0;
		}
		symbol_keys[i].obj->area.h = 28;
	}
	for (int i = 0; i < key_count; ++i)
		if (!keys[i].obj)
		{
			PhoneIME_Shutdown();
			return 0;
		}
	for (int i = 0; i < 5; ++i)
		if (!candidates[i].obj)
		{
			PhoneIME_Shutdown();
			return 0;
		}
	if (!candidate_prev.obj || !candidate_next.obj)
	{
		PhoneIME_Shutdown();
		return 0;
	}
	update_letters();
	reset_query();
	YMGUI_Obj_SetHidden(keyboard, 1);
	return 1;
}
void PhoneIME_Shutdown(void)
{
	PhoneIME_Hide();
	while (inputs)
	{
		InputTarget* entry = inputs;
		inputs = entry->next;
		entry->obj->free_cb = entry->original_free;
		GY_free1(entry);
	}
	if (keyboard)
		YMGUI_Free_ObjFree(keyboard);
	keyboard = NULL;
	GY_free1(s_char_matches);
	s_char_matches = NULL;
	freeEnglishDictionary();
	freeCharDictionary();
	freePhraseDictionary();
	freeCandidateWorkspace();
	context = NULL;
}

GYOBJ PhoneIME_Keyboard(void)
{
	return visible ? keyboard : NULL;
}
int PhoneIME_IsKey(GYOBJ obj)
{
	return obj && obj->event_cb == key_event;
}
