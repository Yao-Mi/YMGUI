#if YAOMI_IME
#include "ime_symbols.h"
#endif
#include "phone_quick.h"
static int quick_probe_hits;
static void quick_probe_click(void)
{
	++quick_probe_hits;
}
static const PhoneQuick quick_probe = {"test-extra", "测试入口", NULL, quick_probe_click, NULL};
static void click_at(int x, int y)
{
	YMGUI_Inject_Pointer((GYcoord)x, (GYcoord)y, 1);
	YMGUI_Inject_Pointer((GYcoord)x, (GYcoord)y, 0);
}

static void step_time(int elapsed)
{
	while (elapsed > 0)
	{
		int part = elapsed > 50 ? 50 : elapsed;
		YMGUI_Inject_Tick((uint32)part);
		advance((uint32)part);
		YMGUI_Refresh(ctx);
		if (test_frame && (scene == RECENTS || (scene == APP && busy) || PhoneShade_Visible()))
		{
			memcpy(test_reference, test_frame, W * H * sizeof(GYpx));
			YMGUI_Obj_Invalidate(ctx->root);
			YMGUI_Refresh(ctx);
			if (memcmp(test_reference, test_frame, W * H * sizeof(GYpx)))
			{
				printf("selftest failed: transition dirty frame differs from full redraw\n");
				++failures;
			}
		}
		elapsed -= part;
	}
}

static void drag_pointer(int x, int y, int dx, int dy)
{
	YMGUI_Inject_Pointer(x, y, 1);
	for (int i = 1; i <= 5; ++i)
		YMGUI_Inject_Pointer(x + dx * i / 5, y + dy * i / 5, 1);
	YMGUI_Inject_Pointer(x + dx, y + dy, 0);
}

static void click_message_button(GYOBJ dialog, int index)
{
	GYOBJ card = dialog->child_head, selected = NULL;
	if (!card)
		return;
	for (GYOBJ child = card->child_head; child; child = child->sibling)
	{
		if (child->type != GY_OBJ_Button)
			continue;
		if (!selected || (index == 0 ? child->area.x < selected->area.x : child->area.x > selected->area.x))
			selected = child;
	}
	if (selected)
	{
		GYrect a;
		YMGUI_Obj_GetAbsArea(selected, &a);
		click_at(a.x + a.w / 2, a.y + a.h / 2);
	}
}
static void click_demo_dialog(int index)
{
	click_message_button(app_demo_dialog, index);
}
static void click_object(GYOBJ obj)
{
	GYrect a;
	YMGUI_Obj_GetAbsArea(obj, &a);
	click_at(a.x + a.w / 2, a.y + a.h / 2);
}

static void selftest(void)
{
#define CHECK(condition, name)                                                                           \
	do                                                                                                   \
	{                                                                                                    \
		if (!(condition))                                                                                \
		{                                                                                                \
			printf("selftest failed: %s (scene=%d, app=%d, busy=%d)\n", name, scene, current_app, busy); \
			++failures;                                                                                  \
		}                                                                                                \
	} while (0)
	test_frame = GY_malloc1(W * H * sizeof(GYpx));
	test_reference = GY_malloc1(W * H * sizeof(GYpx));
	CHECK(test_frame && test_reference, "allocate frame regression buffers");
	if (!test_frame || !test_reference)
		return;
	memset(test_frame, 0, W * H * sizeof(GYpx));
	step_time(1250);
	CHECK(scene == HOME, "boot to home");
	CHECK(PhoneApps_Validate(), "app catalog validates");
	for (int i = 0; i < APP_COUNT; ++i)
		CHECK(PhoneApps_Find(PhoneApps_Get(i)->key) == i, "every descriptor has a unique discoverable key");
	CHECK(!PhoneHost_Open(-1, NULL) && !PhoneHost_Open(APP_COUNT, NULL) && scene == HOME,
		  "invalid app requests leave shell unchanged");
	CHECK(HOME_PAGES == 3, "sixteen apps use three desktop pages");
	CHECK(!(home_dock->state & GY_STATE_Hidden), "first page keeps music widget");
	CHECK(PHONE_LAUNCHER_PAGE_COUNT(0) == 1 && PHONE_LAUNCHER_PAGE_COUNT(4) == 1 &&
			  PHONE_LAUNCHER_PAGE_COUNT(5) == 2 && PHONE_LAUNCHER_PAGE_COUNT(12) == 2 &&
			  PHONE_LAUNCHER_PAGE_COUNT(13) == 3 && PHONE_LAUNCHER_PAGE_COUNT(20) == 3 &&
			  PHONE_LAUNCHER_PAGE_COUNT(21) == 4,
		  "automatic pagination capacity boundaries");
	PhoneLauncherSlot slot;
	CHECK(!PhoneLauncher_Place(-1, APP_COUNT, &slot) && !PhoneLauncher_Place(APP_COUNT, APP_COUNT, &slot) &&
			  !PhoneLauncher_Place(0, APP_COUNT, NULL),
		  "invalid launcher indexes rejected");
	for (int i = 0; i < 25; ++i)
	{
		CHECK(PhoneLauncher_Place(i, 25, &slot), "new entries receive automatic slots");
		CHECK(slot.page < PHONE_LAUNCHER_PAGE_COUNT(25) && slot.x >= 0 && slot.x + 49 <= W &&
				  slot.y >= 0 && slot.y + 77 <= 408,
			  "automatic slots stay within page");
		for (int j = 0; j < i; ++j)
		{
			PhoneLauncherSlot other;
			PhoneLauncher_Place(j, 25, &other);
			CHECK(slot.page != other.page || slot.x != other.x || slot.y != other.y, "launcher slots are unique");
		}
	}
	YMGUI_Inject_Pointer(47, 290, 1);
	YMGUI_Inject_Pointer(2, 290, 1);
	YMGUI_Inject_PointerCancel();
	step_time(300);
	CHECK(scene == HOME && home_index == 0 && home_strip->area.x == 0, "cancel launcher swipe without opening app");
	drag_pointer(200, 290, -120, 0);
	step_time(300);
	CHECK(home_index == 1 && home_strip->area.x == -W && scene == HOME, "swipe across icon changes desktop page");
	CHECK(home_dock->state & GY_STATE_Hidden, "app-only pages hide music widget");
	CHECK(home_dots->area.y == 422, "app-only page indicator sits above navigation");
	drag_pointer(200, 190, -120, 0);
	step_time(300);
	CHECK(home_index == 2, "third desktop page opens");
	drag_pointer(200, 190, -120, 0);
	step_time(300);
	drag_pointer(200, 190, -120, 0);
	step_time(300);
	CHECK(home_index == HOME_PAGES - 1, "last desktop page clamps");
	home_select(1, 0);
	click_at(47, 80);
	step_time(450);
	CHECK(current_app == CLOCK, "second page clock opens");
	click_at(85, 400);
	step_time(1250);
	CHECK(((int)PhoneApps_Inspect(CLOCK, "stopwatch_ms", 0)) >= 1200 && ((int)PhoneApps_Inspect(CLOCK, "stopwatch_running", 0)), "stopwatch ticks");
	click_at(235, 400);
	CHECK(((int)PhoneApps_Inspect(CLOCK, "stopwatch_ms", 0)) == 0 && !((int)PhoneApps_Inspect(CLOCK, "stopwatch_running", 0)), "stopwatch reset");
	click_at(160, 464);
	step_time(320);
	CHECK(home_index == 1, "return retains desktop page");
	click_at(123, 80);
	step_time(450);
	click_at(160, 423);
	CHECK(current_app == WEATHER && ((int)PhoneApps_Inspect(WEATHER, "city_index", 0)) == 1, "weather sample city");
	click_at(160, 464);
	step_time(320);
	click_at(196, 80);
	step_time(450);
	click_at(160, 410);
	CHECK(current_app == NOTES && ((int)PhoneApps_Inspect(NOTES, "note_pinned", 0)), "pin sample note");
	click_at(160, 464);
	step_time(320);
	click_at(270, 80);
	step_time(450);
	click_at(50, 220);
	click_at(270, 220);
	click_at(120, 280);
	click_at(195, 400);
	CHECK(current_app == CALCULATOR && ((int64_t)PhoneApps_Inspect(CALCULATOR, "calc_value", 0)) == 12, "calculator arithmetic");
	click_at(50, 400);
	click_at(50, 220);
	click_at(270, 400);
	click_at(120, 400);
	click_at(195, 400);
	CHECK(!strcmp(((PhoneText*)((GYOBJ)PhoneApps_Inspect(CALCULATOR, "calculator_display", 0))->user_data)->value, "不能除以零"), "calculator zero division guard");
	click_at(255, 464);
	step_time(350);
	CHECK(recent_total == 4, "four previews in two columns");
	drag_pointer(80, 220, 0, -100);
	step_time(300);
	CHECK(recent_scroll == 100 && running[CLOCK], "vertical grid scroll does not dismiss");
	YMGUI_Inject_Pointer(80, 300, 1);
	YMGUI_Inject_Pointer(80, 210, 1);
	YMGUI_Inject_PointerCancel();
	step_time(300);
	CHECK(recent_scroll == 100, "cancel grid scroll restores offset");
	click_at(160, 425);
	step_time(1100);
	CHECK(recent_total == 0 && !busy, "clear scrolled grid");
	click_at(160, 464);
	step_time(320);
	click_at(160, 464);
	step_time(300);
	CHECK(home_index == 0, "home key returns to first page");
	click_at(47, 289);
	CHECK(scene == APP && current_app == TASKS && running[TASKS], "open tasks");
#if YMGUI_ANIM
	GYrect launch_window;
	YMGUI_Obj_GetAbsArea(app_page, &launch_window);
	GYrect launch_tile = splash_tile_area(&launch_window);
	CHECK(launch_tile.x == 23 && launch_tile.y == 267 && launch_tile.w == 49,
		  "first frame icon is visible at tapped launcher position");
	step_time(120);
	CHECK(app_page->area.w > 49 && app_page->area.w < W, "app expands from launcher");
	click_at(256, 464);
	CHECK(scene == APP, "navigation protected during transition");
#endif
	step_time(450);
	click_at(52, 196);
	CHECK(YMGUI_Checkbox_GetChecked(((GYOBJ)PhoneApps_Inspect(TASKS, "task_boxes", 0))), "complete task");
	step_time(300);
	CHECK(YMGUI_Bar_GetValue(((GYOBJ)PhoneApps_Inspect(TASKS, "task_progress", 0))) == 33, "task progress animation");
	click_at(160, 424);
	CHECK(((int)PhoneApps_Inspect(TASKS, "task_count", 0)) == 4, "add task");
	step_time(300);
	CHECK(((GYOBJ)PhoneApps_Inspect(TASKS, "task_boxes", 3))->parent->area.x == 18, "new task settles");
	click_at(160, 464);
	step_time(320);
	CHECK(scene == HOME && !busy, "return to launcher");
	click_at(123, 290);
	step_time(450);
	CHECK(scene == APP && current_app == MUSIC, "open music");
	click_at(160, 405);
	CHECK(((int)PhoneApps_Inspect(MUSIC, "music_playing", 0)), "play music");
	drag_pointer(60, 336, 155, 0);
	CHECK(YMGUI_Slider_GetValue(((GYOBJ)PhoneApps_Inspect(MUSIC, "music_seek", 0))) > 65, "seek slider capture");
	click_at(256, 464);
	step_time(350);
	CHECK(scene == RECENTS && recent_total == 2, "show app snapshots");
	CHECK(snapshots[TASKS] && snapshots[MUSIC], "snapshot allocation");
	drag_pointer(80, 220, 25, 0);
	step_time(300);
	CHECK(recent_cards[TASKS]->area.x == 16 && recent_cards[MUSIC]->area.x == 168, "two column layout and short drag snap");
	YMGUI_Inject_Pointer(80, 220, 1);
	YMGUI_Inject_Pointer(180, 220, 1);
	YMGUI_Inject_PointerCancel();
	step_time(300);
	CHECK(running[TASKS] && recent_cards[TASKS]->area.x == 16, "cancel restores card");
	click_at(80, 220);
	step_time(450);
	CHECK(scene == APP && current_app == TASKS && YMGUI_Checkbox_GetChecked(((GYOBJ)PhoneApps_Inspect(TASKS, "task_boxes", 0))), "resume retains task state");
	click_at(256, 464);
	step_time(350);
	drag_pointer(80, 220, -100, 0);
	step_time(320);
	CHECK(!running[TASKS] && running[MUSIC] && !snapshots[TASKS], "swipe closes one app");
	click_at(160, 425);
	step_time(600);
	CHECK(!running[MUSIC] && !((int)PhoneApps_Inspect(MUSIC, "music_playing", 0)) && !snapshots[MUSIC], "clear all releases snapshots");
	click_at(160, 425);
	step_time(100);
	CHECK(!busy, "clear empty list");
	click_at(160, 464);
	step_time(320);
	click_at(196, 290);
	step_time(450);
	CHECK(scene == APP && current_app == GALLERY, "open gallery");
	click_at(160, 427);
	step_time(400);
	CHECK(((int)PhoneApps_Inspect(GALLERY, "gallery_index", 0)) == 1, "next landscape");
	click_at(160, 464);
	step_time(320);
	click_at(270, 290);
	step_time(450);
	CHECK(scene == APP && current_app == SETTINGS, "open settings");
	click_at(259, 200);
	CHECK(theme_alt && YMGUI_Switch_GetOn(((GYOBJ)PhoneApps_Inspect(SETTINGS, "theme_switch", 0))), "wallpaper switch");
	click_at(259, 133);
	CHECK(!YMGUI_Switch_GetOn(((GYOBJ)PhoneApps_Inspect(SETTINGS, "wifi_switch", 0))), "native wifi switch");
	drag_pointer(275, 293, -110, 0);
	CHECK(YMGUI_Slider_GetValue(((GYOBJ)PhoneApps_Inspect(SETTINGS, "brightness_slider", 0))) < 65, "display brightness");
	YMGUI_Slider_SetValue(((GYOBJ)PhoneApps_Inspect(SETTINGS, "brightness_slider", 0)), 100);
	click_at(160, 393);
	step_time(300);
	CHECK(!(power_dialog->state & GY_STATE_Hidden), "open power sheet");
	click_at(255, 464);
	CHECK(scene == APP, "modal blocks navigation");
	CHECK(power_dialog->state & GY_STATE_Hidden, "backdrop dismisses sheet");
	click_at(160, 393);
	step_time(300);
	click_at(85, 405);
	step_time(300);
	CHECK(power_dialog->state & GY_STATE_Hidden, "cancel power");
	click_at(160, 393);
	step_time(300);
	click_at(225, 405);
	step_time(900);
	CHECK(scene == OFF, "shutdown");
	click_at(150, 357);
	step_time(1250);
	CHECK(scene == HOME && !running[GALLERY] && !running[SETTINGS], "restart resets running apps");
	CHECK(YMGUI_Checkbox_GetChecked(((GYOBJ)PhoneApps_Inspect(TASKS, "task_boxes", 0))) && theme_alt, "restart keeps session settings");
	for (int i = 0; i < APP_COUNT; ++i)
	{
		PhoneLauncher_Place(i, APP_COUNT, &slot);
		if (slot.page != home_index)
		{
			drag_pointer(200, 190, -120, 0);
			step_time(300);
		}
		click_at(slot.x + 24, slot.y + 28 + 24);
#if YMGUI_ANIM
		CHECK(app_page->area.x == slot.x && app_page->area.y == slot.y + 28 &&
				  app_page->area.w == 49 && app_page->area.h == 51,
			  "each app starts at its own desktop icon including music");
		YMGUI_Refresh(ctx);
		GYrect first_tile = splash_tile_area(&app_page->area);
		GYpx first_color = GY_ColorToPx(PhoneApps_Get(i)->color);
		CHECK(!memcmp(&test_frame[(first_tile.y + first_tile.h / 2) * W + first_tile.x + 4],
					  &first_color, sizeof(GYpx)),
			  "launch icon is painted in the first frame, not clipped away");
		for (int frame = 0; frame < 10; ++frame)
		{
			GYrect window;
			YMGUI_Obj_GetAbsArea(app_page, &window);
			GYrect tile = splash_tile_area(&window);
			CHECK(tile.x >= window.x && tile.y >= window.y &&
					  tile.x + tile.w <= window.x + window.w && tile.y + tile.h <= window.y + window.h,
				  "launch icon stays wholly inside expanding window");
			CHECK(abs(tile.x * 2 + tile.w - (window.x * 2 + window.w)) <= 1 &&
					  abs(tile.y * 2 + tile.h - (window.y * 2 + window.h)) <= 1,
				  "launch icon stays centered without bottom-right offset");
			step_time(32);
		}
		CHECK(busy && !(app_splash->state & GY_STATE_Hidden), "content does not replace splash before resize completes");
#endif
		step_time(450);
		CHECK(current_app == i && running[i], "every launcher entry opens its own app");
		click_at(160, 464);
		step_time(320);
	}
	click_at(255, 464);
	step_time(350);
	CHECK(recent_total == APP_COUNT, "all registered app previews");
	drag_pointer(80, 220, 0, -3000);
	step_time(300);
	CHECK(recent_scroll == recent_scroll_limit(), "grid bottom clamps");
	CHECK(recent_cards[CAMERA]->area.x == 16 && recent_cards[BROWSER]->area.x == 168, "last row keeps two columns");
	click_at(160, 425);
#if YMGUI_ANIM
	step_time(100);
	CHECK(recent_cards[CAMERA]->area.x < 16 && recent_cards[BROWSER]->area.x > 168, "clear scatters left and right by column");
	CHECK(recent_cards[CAMERA]->area.y == card_home_y[CAMERA], "clear keeps card vertical position");
#endif
	step_time(1200);
	CHECK(recent_total == 0 && !busy, "clear all eight cards from bottom");
	for (int i = 0; i < APP_COUNT; ++i)
		CHECK(!snapshots[i], "all eight snapshots released");
	click_at(160, 464);
	step_time(320);
	click_at(160, 464);
	step_time(300);
	open_app(NOTES);
	step_time(450);
	YMGUI_EditView_SetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0)), "");
	click_at(70, 215);
	step_time(220);
#if YAOMI_IME
	CHECK(PhoneIME_Visible() && PhoneIME_Target() == ((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0)), "body tap opens shared keyboard");
	GYOBJ ime_body = (GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0);
	click_at(300, 264);
	step_time(50);
	CHECK(!PhoneIME_Visible() && ime_body->area.h == 128, "empty candidate right area hides keyboard and restores editor");
	click_at(70, 188);
	step_time(220);
	click_at(270, 264);
	step_time(50);
	CHECK(!PhoneIME_Visible(), "empty candidate left area also hides keyboard");
	click_at(70, 188);
	step_time(220);
	click_at(65, 421);
	click_at(20, 301);
	CHECK(!strcmp(YMGUI_EditView_GetText(ime_body), "1"), "123 remains quick numeric input");
	click_at(65, 421);
	YMGUI_EditView_SelectAll(ime_body);
	const char* py = "nihao";
	for (const char* p = py; *p; ++p)
		YMGUI_Inject_Key(*p, 1);
	click_at(30, 264);
	CHECK(!strcmp(YMGUI_EditView_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0))), "你好"), "tap Chinese candidate inserts at editor cursor");
	click_at(290, 421);
	CHECK(!strcmp(YMGUI_EditView_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0))), "你好\n"), "software enter inserts newline");
	CHECK(PhoneIME_Visible(), "multiline enter keeps keyboard open");
	click_at(285, 379);
	CHECK(!strcmp(YMGUI_EditView_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0))), "你好"), "software backspace deletes newline");
	/* 用多页候选验证翻页确实生效，并能准确返回前页。 */
	char first_ni[96];
	YMGUI_EditView_SelectAll(ime_body);
	YMGUI_Inject_Key('n', 1);
	YMGUI_Inject_Key('i', 1);
	click_at(30, 264);
	snprintf(first_ni, sizeof(first_ni), "%s", YMGUI_EditView_GetText(ime_body));
	YMGUI_EditView_SelectAll(ime_body);
	YMGUI_Inject_Key('n', 1);
	YMGUI_Inject_Key('i', 1);
	click_at(300, 264);
	CHECK(PhoneIME_Visible(), "next candidate page does not dismiss composing keyboard");
	click_at(30, 264);
	CHECK(strcmp(first_ni, YMGUI_EditView_GetText(ime_body)), "next page selects a different candidate");
	YMGUI_EditView_SelectAll(ime_body);
	YMGUI_Inject_Key('n', 1);
	YMGUI_Inject_Key('i', 1);
	click_at(300, 264);
	click_at(270, 264);
	click_at(30, 264);
	CHECK(!strcmp(first_ni, YMGUI_EditView_GetText(ime_body)), "previous page restores first candidate");
	YMGUI_EditView_SelectAll(ime_body);
	for (const char* p = py; *p; ++p)
		YMGUI_Inject_Key(*p, 1);
	click_at(290, 421);
	CHECK(!strcmp(YMGUI_EditView_GetText(ime_body), "你好\n") && PhoneIME_Visible(), "enter commits composition and inserts newline in one tap");
	click_at(285, 379);
	for (int i = 0; i < 6; ++i)
		YMGUI_Inject_Key('z', 1);
	click_at(300, 264);
	click_at(270, 264);
	CHECK(PhoneIME_Visible() && !strcmp(YMGUI_EditView_GetText(ime_body), "你好"), "unmatched composition keeps navigation without committing or hiding");
	for (int i = 0; i < 6; ++i)
		click_at(285, 379);
	click_at(300, 264);
	step_time(50);
	CHECK(!PhoneIME_Visible(), "deleting last composition letter restores full-width hide key");
	click_at(70, 188);
	step_time(220);
	click_at(65, 421);
	click_at(225, 421);
	click_at(20, 301);
	CHECK(!strcmp(YMGUI_EditView_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0))), "你好q"), "language switch exits symbols and English key inserts into editor");
	YMGUI_EditView_SelectAll(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0)));
	click_at(51, 301);
	CHECK(!strcmp(YMGUI_EditView_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0))), "w"), "software key replaces selected text");
	YMGUI_TextInput_SetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_title_input", 0)), "");
	click_at(60, 145);
	step_time(50);
	CHECK(PhoneIME_Target() == ((GYOBJ)PhoneApps_Inspect(NOTES, "note_title_input", 0)), "keyboard follows title input focus");
	click_at(20, 301);
	CHECK(!strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_title_input", 0))), "q"), "single-line target receives software text");
	CHECK(!strcmp(YMGUI_EditView_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0))), "w"), "switching inputs does not write to previous target");
	YMGUI_Inject_Pointer(20, 301, 1);
	step_time(50);
	CHECK(PhoneIME_Visible() && PhoneIME_Target() == ((GYOBJ)PhoneApps_Inspect(NOTES, "note_title_input", 0)), "held software key retains input focus");
	YMGUI_Inject_PointerCancel();
	step_time(50);
	CHECK(!strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_title_input", 0))), "q"), "cancel software key does not type");
	click_at(180, 236);
	step_time(50);
	CHECK(PhoneIME_Visible(), "keyboard blank area does not dismiss input");
	click_at(225, 421);
	YMGUI_TextInput_SelectAll(((GYOBJ)PhoneApps_Inspect(NOTES, "note_title_input", 0)));
	for (const char* p = py; *p; ++p)
		YMGUI_Inject_Key(*p, 1);
	YMGUI_Inject_Key(' ', 1);
	CHECK(!strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_title_input", 0))), "你好"), "Chinese replaces single-line selection");
	YMGUI_Inject_Key(GY_KEY_BACKSPACE, 1);
	CHECK(!strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_title_input", 0))), "你"), "Chinese backspace removes entire codepoint");
	YMGUI_Inject_Key('n', 1);
	YMGUI_Inject_Key('i', 1);
	click_at(70, 188);
	step_time(50);
	YMGUI_Inject_Key(' ', 1);
	CHECK(!strcmp(YMGUI_EditView_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0))), "w ") && !strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_title_input", 0))), "你"), "pending composition does not leak to another input");
	click_at(225, 421);
	click_at(65, 464);
	step_time(50);
	CHECK(!PhoneIME_Visible() && scene == APP && ((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0))->area.h == 128, "back first hides keyboard and restores editor");
	click_at(60, 145);
	step_time(220);
	CHECK(PhoneIME_Visible(), "same input can reopen keyboard");
	click_at(255, 464);
	step_time(320);
	CHECK(scene == RECENTS && !PhoneIME_Visible(), "recent navigation hides keyboard");
	open_app(SETTINGS);
	step_time(450);
	YMGUI_TextInput_SetText(((GYOBJ)PhoneApps_Inspect(SETTINGS, "device_name_input", 0)), "");
	click_at(75, 343);
	step_time(220);
	CHECK(PhoneIME_Target() == ((GYOBJ)PhoneApps_Inspect(SETTINGS, "device_name_input", 0)), "another app automatically uses same keyboard");
	click_at(20, 301);
	CHECK(!strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(SETTINGS, "device_name_input", 0))), "q"), "cross-app input targets device name");
	click_at(290, 421);
	step_time(50);
	CHECK(!PhoneIME_Visible() && !strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(SETTINGS, "device_name_input", 0))), "q"), "single-line return submits without newline and hides keyboard");
	CHECK(((GYOBJ)PhoneApps_Inspect(SETTINGS, "device_name_input", 0))->area.y == 293, "obscured input geometry restored");
	GYOBJ transient = YMGUI_Creat_TextInput_Creat(app_views[SETTINGS], 20, 100, 200, 30, 32);
	YMGUI_SetFocus(ctx, transient);
	step_time(220);
	CHECK(PhoneIME_Target() == transient, "new input defaults to shared keyboard");
	YMGUI_Free_ObjFree(transient);
	step_time(50);
	CHECK(!PhoneIME_Visible() && !PhoneIME_Target(), "destroying active input detaches keyboard safely");
	open_app(NOTES);
	step_time(450);
	click_at(70, 188);
	step_time(220);
	click_at(225, 421); /* 英文 -> 中文 */
	YMGUI_EditView_SetText(ime_body, "");
	YMGUI_Inject_Key('n', 1);
	YMGUI_Inject_Key('i', 1);
	click_at(25, 421);
	CHECK(YMGUI_EditView_GetText(ime_body)[0] != 0, "symbol page commits pending candidate first");
	for (int category = 0; category < 7; ++category)
	{
		click_at(30 + category % 4 * 79, 263 + category / 4 * 26);
		int count = imeSymbolCount(category, 0);
		for (int item = 0; item < count; ++item)
		{
			if (item && item % 15 == 0)
				click_at(300, 237);
			YMGUI_EditView_SelectAll(ime_body);
			click_at(30 + item % 5 * 63, 320 + item % 15 / 5 * 32);
			CHECK(!strcmp(YMGUI_EditView_GetText(ime_body), imeSymbolAt(category, item, 0)), "every categorized symbol inserts intact UTF-8");
		}
		click_at(300, 237); /* 末页不能越界 */
		YMGUI_EditView_SelectAll(ime_body);
		click_at(30, 320);
		CHECK(!strcmp(YMGUI_EditView_GetText(ime_body), imeSymbolAt(category, (count - 1) / 15 * 15, 0)), "symbol last page is clamped");
		click_at(270, 237);
		YMGUI_EditView_SelectAll(ime_body);
		click_at(30, 320);
		CHECK(!strcmp(YMGUI_EditView_GetText(ime_body), imeSymbolAt(category, ((count - 1) / 15 - 1) * 15, 0)), "symbol previous page navigates backwards");
	}
	click_at(30, 263);	/* 常用 */
	click_at(225, 421); /* 中文 -> 英文，留在符号页 */
	YMGUI_EditView_SelectAll(ime_body);
	click_at(30, 320);
	CHECK(!strcmp(YMGUI_EditView_GetText(ime_body), "!"), "English common symbols use separate table");
	click_at(25, 421); /* ABC */
	YMGUI_EditView_SelectAll(ime_body);
	click_at(20, 301);
	CHECK(!strcmp(YMGUI_EditView_GetText(ime_body), "q"), "ABC returns from symbol page to typing");
	PhoneIME_Hide();
#else
	YMGUI_Inject_Key('a', 1);
	CHECK(!strcmp(YMGUI_EditView_GetText(((GYOBJ)PhoneApps_Inspect(NOTES, "note_editor", 0))), "a"), "IME off retains native editor input");
#endif
	CHECK(PhoneUI_text_width("电话", 0) == 32 && YMGUI_Font_GetFallback(), "Chinese shell text is independent of IME");
	open_app(PHONE);
	step_time(450);
	click_at(66, 194);
	click_at(160, 194);
	CHECK(!strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(PHONE, "dial_input", 0))), "12"), "dial pad enters digits");
	click_at(276, 119);
	CHECK(!strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(PHONE, "dial_input", 0))), "1"), "dial delete");
	click_at(160, 407);
	step_time(1100);
	CHECK(((int)PhoneApps_Inspect(PHONE, "call_active", 0)) && ((int)PhoneApps_Inspect(PHONE, "call_ms", 0)) >= 1000, "local call simulation starts");
	click_at(160, 407);
	CHECK(!((int)PhoneApps_Inspect(PHONE, "call_active", 0)), "local call simulation hangs up");
	open_app(CONTACTS);
	step_time(450);
	click_at(60, 118);
	step_time(220);
	for (const unsigned char* p = (const unsigned char*)"小雨"; *p; ++p)
		YMGUI_Inject_Key(*p, 1);
	PhoneIME_Hide();
	CHECK((((GYOBJ)PhoneApps_Inspect(CONTACTS, "contact_rows", 0))->state & GY_STATE_Hidden) && !(((GYOBJ)PhoneApps_Inspect(CONTACTS, "contact_rows", 2))->state & GY_STATE_Hidden), "Chinese contact search");
	click_at(160, 257);
	click_at(84, 407);
	step_time(450);
	CHECK(current_app == PHONE && !strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(PHONE, "dial_input", 0))), "13800000003"), "contact links to dialer");
	open_app(CONTACTS);
	step_time(450);
	click_at(230, 407);
	step_time(450);
	CHECK(current_app == MESSAGES && !strcmp(YMGUI_TextInput_GetText(((GYOBJ)PhoneApps_Inspect(MESSAGES, "message_to", 0))), "13800000003"), "contact links to message composer");
	YMGUI_EditView_SetText(((GYOBJ)PhoneApps_Inspect(MESSAGES, "message_editor", 0)), "");
	click_at(160, 407);
	CHECK(strstr(((PhoneText*)((GYOBJ)PhoneApps_Inspect(MESSAGES, "message_status", 0))->user_data)->value, "请填写"), "empty message is rejected");
	YMGUI_EditView_SetText(((GYOBJ)PhoneApps_Inspect(MESSAGES, "message_editor", 0)), "这是一条演示消息。");
	click_at(160, 407);
	CHECK(strstr(((PhoneText*)((GYOBJ)PhoneApps_Inspect(MESSAGES, "message_status", 0))->user_data)->value, "已模拟"), "message simulation reports no network send");
	open_app(RECORDER);
	step_time(450);
	click_at(160, 407);
	step_time(1100);
	click_at(160, 407);
	CHECK(!((int)PhoneApps_Inspect(RECORDER, "recording", 0)) && ((int)PhoneApps_Inspect(RECORDER, "record_ms", 0)) >= 1000 && ((int)PhoneApps_Inspect(RECORDER, "recording_count", 0)) == 1, "recording state simulation starts and stops");
	click_at(160, 407);
	open_recent();
	step_time(350);
	clear_recent();
	step_time(1200);
	CHECK(!((int)PhoneApps_Inspect(RECORDER, "recording", 0)) && !((int)PhoneApps_Inspect(PHONE, "call_active", 0)) && !recent_total, "clear stops simulated background activity");
	open_app(FILES);
	step_time(450);
	click_at(160, 253);
	CHECK(((int)PhoneApps_Inspect(FILES, "file_folder", 0)) == 2, "virtual file folder opens");
	click_at(65, 464);
	CHECK(scene == APP && current_app == FILES && ((int)PhoneApps_Inspect(FILES, "file_folder", 0)) == -1, "file back returns to parent folder");
	click_at(160, 155);
	click_at(160, 155);
	step_time(450);
	CHECK(current_app == GALLERY, "virtual photo links to album");
	open_app(GYROSCOPE);
	step_time(450);
	drag_pointer(160, 211, 45, -20);
	int16 gx, gy;
	YMGUI_Joystick_GetValue(((GYOBJ)PhoneApps_Inspect(GYROSCOPE, "gyro_control", 0)), &gx, &gy);
	CHECK(gx != 0 && gy != 0, "gyro mock uses native joystick");
	click_at(160, 407);
	YMGUI_Joystick_GetValue(((GYOBJ)PhoneApps_Inspect(GYROSCOPE, "gyro_control", 0)), &gx, &gy);
	CHECK(gx == 0 && gy == 0, "gyro mock resets");
	for (int id = CAMERA; id <= BROWSER; ++id)
	{
		open_app(id);
		step_time(450);
		click_at(160, id == CAMERA ? 352 : 359);
		CHECK(YMGUI_MsgBox_IsShown(app_demo_dialog), "camera/browser opens native demo dialog");
		click_at(255, 464);
		CHECK(scene == APP && current_app == id && YMGUI_MsgBox_IsShown(app_demo_dialog), "demo modal blocks navigation");
		click_demo_dialog(0);
		CHECK(!YMGUI_MsgBox_IsShown(app_demo_dialog) && current_app == id, "demo dialog closes without leaving app");
		click_at(160, id == CAMERA ? 352 : 359);
		click_demo_dialog(1);
		step_time(320);
		CHECK(scene == HOME && !running[id] && !snapshots[id] && !YMGUI_MsgBox_IsShown(app_demo_dialog), "demo dialog can close app and release preview");
		open_app(id);
		step_time(450);
		click_at(160, 410);
		step_time(320);
		CHECK(scene == HOME && !running[id], "camera/browser close button works repeatedly");
	}
	home_select(0, 0);
	click_at(48, 400);
	CHECK(current_app == MUSIC && scene == APP, "music dock icon opens music");
#if YMGUI_ANIM
	GYrect dock_origin;
	YMGUI_Obj_GetAbsArea(dock_buttons[MUSIC], &dock_origin);
	CHECK(app_page->area.x == dock_origin.x && app_page->area.y == dock_origin.y &&
			  app_page->area.w == dock_origin.w,
		  "dock launch uses actual dock position");
#endif
	step_time(450);
	go_home();
	step_time(320);
	/* 状态栏手势、快捷状态、通知和真实内存截屏。 */
	YMGUI_Inject_Pointer(160, 8, 1);
	YMGUI_Inject_Pointer(160, 90, 1);
	YMGUI_Inject_PointerCancel();
	step_time(260);
	CHECK(!PhoneShade_Visible() && scene == HOME, "cancel pull-down returns to home without opening app");
	drag_pointer(160, 8, 0, 160);
	step_time(260);
	CHECK(PhoneShade_Visible() && scene == HOME, "status bar pull opens control center");
	CHECK(PhoneQuick_Count() == 9, "independent ninth feature registers without shell branch changes");
	click_at(290, 72);
	CHECK(PhoneShade_Inspect("page") == 1, "ninth feature automatically gets a second page");
	click_at(45, 117);
	CHECK(quick_probe_hits == 1, "registered feature dispatches its own callback");
	click_at(290, 72);
	CHECK(PhoneShade_Inspect("page") == 1, "quick setting page is clamped at last page");
	click_at(250, 72);
	CHECK(PhoneShade_Inspect("page") == 0, "quick setting previous page restores builtins");
	int old_wifi = PhoneHost_GetWifi();
	click_at(45, 117);
	CHECK(PhoneHost_GetWifi() != old_wifi, "Wi-Fi quick toggle changes shared state");
	CHECK(YMGUI_Switch_GetOn((GYOBJ)PhoneApps_Inspect(SETTINGS, "wifi_switch", 0)) == PhoneHost_GetWifi(), "settings Wi-Fi follows control center");
	click_at(200, 117);
	CHECK(PhoneShade_Inspect("flight") && !PhoneShade_Inspect("data") && !PhoneShade_Inspect("hotspot"), "flight disables data and hotspot");
	click_at(45, 185);
	CHECK(PhoneShade_Inspect("hotspot") && !PhoneShade_Inspect("flight"), "enabling hotspot exits flight mode");
	click_at(275, 117);
	CHECK(PhoneShade_Inspect("bluetooth"), "Bluetooth is toggleable locally");
	CHECK(!PhoneShade_Inspect("muted"), "mute mode is off by default");
	click_at(115, 185);
	CHECK(PhoneShade_Inspect("muted"), "mute tile enables silent mode");
	click_at(115, 185);
	CHECK(!PhoneShade_Inspect("muted"), "mute tile restores ringing mode");
	click_at(155, 242);
	CHECK(PhoneHost_GetBrightness() > 0 && PhoneHost_GetBrightness() < 100, "shade brightness adjusts preview");
	CHECK(YMGUI_Slider_GetValue((GYOBJ)PhoneApps_Inspect(SETTINGS, "brightness_slider", 0)) == PhoneHost_GetBrightness(), "settings brightness follows control center");
	PhoneHost_SetBrightness(100);
	click_at(200, 185);
	step_time(260);
	CHECK(PhoneShade_Inspect("preview") && PhoneShade_Inspect("capture"), "screenshot opens actual image preview");
	PhoneShade_Close(0);
	PhoneHost_Capture(test_reference);
	CHECK(!memcmp((void*)PhoneShade_Inspect("capture"), test_reference, W * H * sizeof(GYpx)), "screenshot pixels match underlying screen without shade");
	PhoneShade_Open();
	step_time(260);
	click_at(160, 326);
	step_time(450);
	CHECK(!PhoneShade_Visible() && scene == APP && current_app == MESSAGES, "message notification opens message app");
	drag_pointer(160, 8, 0, 160);
	step_time(260);
	CHECK(PhoneShade_Inspect("notices") == 1, "opened notification removed without clearing others");
	click_at(270, 282);
#if YMGUI_ANIM
	CHECK(PhoneShade_Inspect("clearing") && !PhoneShade_Inspect("empty"), "clear keeps cards until exit completes");
	step_time(100);
	GYOBJ exiting_notice = (GYOBJ)PhoneShade_Inspect("card1");
	CHECK(exiting_notice->area.x > 14 && exiting_notice->area.x < W && exiting_notice->area.y == 361,
		  "notification moves right, not upward");
	int exit_x = exiting_notice->area.x;
	click_at(270, 282);
	CHECK(exiting_notice->area.x == exit_x, "duplicate clear does not restart animation");
	click_at(exit_x + 20, 385);
	CHECK(PhoneShade_Visible() && current_app == MESSAGES, "exiting notification cannot launch another app");
#endif
	step_time(350);
	CHECK(!PhoneShade_Inspect("clearing") && PhoneShade_Inspect("notices") == 0 && PhoneShade_Inspect("empty"), "right exit finishes with empty state");
	click_at(270, 282);
	CHECK(!PhoneShade_Inspect("clearing"), "clearing empty notifications is a no-op");
	drag_pointer(160, 434, 0, -160);
	step_time(260);
	CHECK(!PhoneShade_Visible() && scene == APP, "swipe handle up dismisses shade without leaving app");
	PhoneShade_Open();
	step_time(260);
	CHECK(PhoneShade_Inspect("empty") && ((GYOBJ)PhoneShade_Inspect("card1"))->area.x == 14,
		  "reopening keeps notifications empty with hidden cards reset");
	click_at(65, 464);
	step_time(260);
	CHECK(!PhoneShade_Visible() && scene == APP, "back closes shade before app");
	PhoneHost_SetWifi(1);
	go_home();
	step_time(320);
	home_select(0, 0);
	YMGUI_Inject_ContextBegin(48, 289);
	CHECK(PhoneDesktop_Inspect("menu") && scene == HOME, "long press opens app menu without launching app");
	YMGUI_Inject_ContextMove(123, 289);
	CHECK(PhoneDesktop_Inspect("dragging") && !PhoneDesktop_Inspect("menu"), "moving after long press switches menu to icon drag");
	YMGUI_Inject_ContextEnd(123, 289);
	step_time(250);
	CHECK(PhoneDesktop_Index(TASKS) == 1 && PhoneDesktop_Index(MUSIC) == 0, "drag reorders desktop without changing app IDs");
	click_at(123, 289);
	CHECK(scene == APP && current_app == TASKS, "moved icon still opens its own app");
	step_time(450);
	go_home();
	step_time(320);
	YMGUI_Inject_ContextBegin(123, 289);
	YMGUI_Inject_ContextMove(314, 289);
	step_time(600);
	CHECK(home_index == 1, "holding dragged icon at edge changes desktop page");
	step_time(250);
	YMGUI_Inject_ContextMove(48, 82);
	YMGUI_Inject_ContextEnd(48, 82);
	step_time(250);
	CHECK(PhoneDesktop_Index(TASKS) == 4 && home_index == 1, "cross-page drop inserts at destination slot");
	YMGUI_Inject_ContextBegin(48, 82);
	YMGUI_Inject_ContextMove(123, 82);
	YMGUI_Inject_ContextCancel();
	step_time(250);
	CHECK(PhoneDesktop_Index(TASKS) == 4 && !PhoneDesktop_Busy(), "cancel drag preserves order and closes menu");
	click_at(48, 82);
	CHECK(scene == APP && current_app == TASKS, "cross-page icon opens from its new location");
	step_time(450);
	go_home();
	step_time(320);
	YMGUI_Inject_ContextBegin(48, 82);
	YMGUI_Inject_ContextEnd(48, 82);
	CHECK(PhoneDesktop_Inspect("menu"), "long press without drag leaves context menu open");
	click_object((GYOBJ)PhoneDesktop_Inspect("info-button"));
	GYOBJ desktop_dialog = (GYOBJ)PhoneDesktop_Inspect("dialog");
	CHECK(YMGUI_MsgBox_IsShown(desktop_dialog), "app information opens native dialog");
	click_message_button(desktop_dialog, 0);
	YMGUI_Inject_ContextRequest(48, 82);
	click_object((GYOBJ)PhoneDesktop_Inspect("remove-button"));
	click_message_button(desktop_dialog, 0);
	CHECK(PhoneDesktop_Index(TASKS) == 4, "cancel uninstall keeps desktop entry");
	YMGUI_Inject_ContextRequest(48, 82);
	click_object((GYOBJ)PhoneDesktop_Inspect("remove-button"));
	click_message_button(desktop_dialog, 1);
	step_time(250);
	CHECK(PhoneDesktop_Index(TASKS) < 0 && !running[TASKS] && !snapshots[TASKS], "demo uninstall removes entry and releases running snapshot");
	CHECK(!PhoneHost_Open(TASKS, NULL) && scene == HOME, "removed demo entry cannot open until restored");
	YMGUI_Inject_ContextRequest(170, 300);
	click_object((GYOBJ)PhoneDesktop_Inspect("restore-button"));
	step_time(300);
	CHECK(PhoneDesktop_Inspect("count") == APP_COUNT && PhoneDesktop_Index(TASKS) == 0 && home_index == 0, "blank-area menu restores original desktop and removed entries");
	GY_free1(test_frame);
	test_frame = NULL;
	GY_free1(test_reference);
	test_reference = NULL;
	printf("phone_shell selftest: %s\n", failures ? "FAIL" : "OK");
#undef CHECK
}
