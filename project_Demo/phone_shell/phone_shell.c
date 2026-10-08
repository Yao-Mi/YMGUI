#include "phone_ui.h"
#include "phone_app.h"
#include "phone_host.h"
#include "phone_locale.h"
#include "phone_launcher.h"
#include "phone_shade.h"
#include "phone_desktop.h"
#ifndef PHONE_PORTABLE_BUILD
#include "SDL_LCD.h"
#endif
#include "phone_runtime.h"
#include "YMGUI_TextActions.h"
#include "phone_quick.h"
#include "YMGUI_ScaleBilinear.h"

#define HOME_PAGES PHONE_LAUNCHER_PAGE_COUNT(APP_COUNT)
#define APP_OPEN_MS 330
enum
{
	BOOT,
	HOME,
	APP,
	RECENTS,
	SHUTDOWN,
	OFF
};

static GYCTX ctx;
static GYOBJ home_page, app_page, recent_page, boot_page, power_page;
static GYOBJ app_views[APP_COUNT], app_splash, app_title;
static GYOBJ boot_logo, boot_bar, power_bar, power_title, restart_button;
static GYOBJ recent_cards[APP_COUNT], home_buttons[APP_COUNT];
static GYOBJ recent_empty, recent_count;
static int scene = BOOT, current_app = -1, busy = 0, clearing = 0, splash_elapsed = 0;
static int phase_elapsed = 0, display_brightness = 100;
static int theme_alt = 0, running[APP_COUNT] = {0}, failures;

static GYOBJ navigation, fixed_status, power_dialog, app_demo_dialog, dock_buttons[APP_COUNT];
static GYOBJ mini_music_button, home_music_status;
static GYpx* dim_buffer;
static GYpx* capture_target;
static int recent_scroll = 0, recent_total = 0, recent_ids[APP_COUNT];
static GYOBJ recent_viewport, home_strip, home_dots, home_dock;
static int home_index = 0, home_start_x, home_start_y, home_origin, home_dragged, home_pressed_app = -1;
static int drag_scroll, drag_axis;
static int drag_x, drag_y, drag_dx, drag_dy, dragging, dismiss_id = -1;
static GYcoord drag_origins[APP_COUNT], card_home_y[APP_COUNT];
static GYcoord drag_origin_y;
/* Per-app previews only; one full-size capture shared by the active app. */
#define PREVIEW_W 120
#define PREVIEW_H 168
#ifndef PHONE_PREVIEW_BILINEAR
#define PHONE_PREVIEW_BILINEAR 1
#endif
static GYpx* snapshots[APP_COUNT];
static GYpx* snapshot_work;
static int snapshot_owner = -1, snapshot_dirty;
#if PHONE_PREVIEW_BILINEAR
typedef struct
{
	GYScaleAxis x[PREVIEW_W], y[PREVIEW_H];
} PhonePreviewPlan;
#else
typedef struct
{
	uint32 x[PREVIEW_W], y[PREVIEW_H];
} PhonePreviewPlan;
#endif
static uint8 snapshot_valid[APP_COUNT];
static PhonePreviewPlan* preview_plan;
volatile uint32 phone_preview_stats[3]; /* commits, pixels, missing buffers */
static void snapshot_commit(void)
{
	if (!snapshot_dirty || snapshot_owner < 0 || !snapshot_work)
		return;
	int id = snapshot_owner;
	if (!snapshots[id])
	{
		++phone_preview_stats[2];
		return;
	}
	if (!preview_plan)
	{
		preview_plan = (PhonePreviewPlan*)GY_malloc1(sizeof(*preview_plan));
		if (preview_plan)
		{
#if PHONE_PREVIEW_BILINEAR
			GY_ScaleAxisInit(preview_plan->x, W, PREVIEW_W, 1);
			GY_ScaleAxisInit(preview_plan->y, 448, PREVIEW_H, W);
#else
			GY_ScaleNearestInit(preview_plan->x, W, PREVIEW_W, 1);
			GY_ScaleNearestInit(preview_plan->y, 448, PREVIEW_H, W);
#endif
		}
	}
	for (int y = 0; y < PREVIEW_H; ++y)
	{
#if PHONE_PREVIEW_BILINEAR
		GYScaleAxis row = preview_plan ? preview_plan->y[y] : GY_ScaleAxisAt(y, 448, PREVIEW_H, W);
		for (int x = 0; x < PREVIEW_W; ++x)
		{
			GYScaleAxis col = preview_plan ? preview_plan->x[x] : GY_ScaleAxisAt(x, W, PREVIEW_W, 1);
			snapshots[id][y * PREVIEW_W + x] = GY_ScaleBilinearPx(snapshot_work, &col, &row);
		}
#else
		uint32 row = preview_plan ? preview_plan->y[y] : GY_ScaleNearestAt(y, 448, PREVIEW_H, W);
		for (int x = 0; x < PREVIEW_W; ++x)
		{
			uint32 col = preview_plan ? preview_plan->x[x] : GY_ScaleNearestAt(x, W, PREVIEW_W, 1);
			snapshots[id][y * PREVIEW_W + x] = snapshot_work[row + col];
		}
#endif
	}
	snapshot_valid[id] = 1;
	snapshot_dirty = 0;
	++phone_preview_stats[0];
	phone_preview_stats[1] += PREVIEW_W * PREVIEW_H;
}
static void snapshot_release(int id)
{
	GY_free1(snapshots[id]);
	snapshots[id] = NULL;
	snapshot_valid[id] = 0;
	if (snapshot_owner == id)
	{
		snapshot_owner = -1;
		snapshot_dirty = 0;
	}
	for (int i = 0; i < APP_COUNT; ++i)
		if (snapshots[i])
			return;
	GY_free1(snapshot_work);
	snapshot_work = NULL;
	GY_free1(preview_plan);
	preview_plan = NULL;
}
static GYpx *test_frame, *test_reference;
static void (*display_flush)(struct GYdisp*, const GYrect*, const GYpx*);

static PhoneRuntimeConfig runtime_config;
static GYDISP runtime_display;
static void splash_draw_policy(void);
static void phone_refresh(void)
{
	if (!ctx || !ctx->inv_cnt)
		return;
	if (app_splash)
		splash_draw_policy();
	if (runtime_config.transition)
		runtime_config.transition(busy && (scene == APP || scene == HOME), runtime_config.user);
	if (runtime_config.merge_transitions && busy && (scene == APP || scene == HOME))
		YMGUI_Ctx_MergeDirtyAreas(ctx);
	if (runtime_config.refresh)
		runtime_config.refresh(ctx, runtime_config.user);
	else
		YMGUI_Refresh(ctx);
}

static void show_power_dialog(void);
static void recent_layout(int animate);
static void home_select(int index, int animate);



static void status_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	(void)obj;
	GYcolor ink = obj->bg_color;
	for (int i = 0; i < 4; ++i)
	{
		GYrect bar = {(GYcoord)(area->x + i * 5), (GYcoord)(area->y + 13 - i * 3),
					  3, (GYcoord)(4 + i * 3)};
		YMGUI_Draw_Fill(surface, &bar, ink, GY_OPA_COVER);
	}
	GYrect rim = {(GYcoord)(area->x + 27), (GYcoord)(area->y + 3), 25, 13};
	PhoneUI_rounded(surface, &rim, ink, 4, GY_OPA_COVER);
	GYrect gap = {(GYcoord)(area->x + 29), (GYcoord)(area->y + 5), 19, 9};
	PhoneUI_rounded(surface, &gap, obj->parent == app_page ? PAPER : RGB(55, 67, 105), 2, GY_OPA_COVER);
	GYrect fill = {(GYcoord)(area->x + 30), (GYcoord)(area->y + 6), 16, 7};
	PhoneUI_rounded(surface, &fill, ink, 1, GY_OPA_COVER);
	GYrect tip = {(GYcoord)(area->x + 53), (GYcoord)(area->y + 7), 2, 5};
	YMGUI_Draw_Fill(surface, &tip, ink, GY_OPA_COVER);
}



static void status_bar(GYOBJ parent)
{
	GYcolor ink = parent == app_page ? INK : WHITE;
	PhoneUI_left_label(parent, 19, 4, 54, "9:41", ink, 2);
	GYOBJ icon = YMGUI_Creat_Obj_Creat(parent, 248, 3, 56, 18);
	icon->bg_color = ink;
	icon->draw_cb = status_draw;
}



static GYrect splash_tile_area(const GYrect* window)
{
	int size = window->w < 66 ? window->w : 66;
	if (size > window->h)
		size = window->h;
	return (GYrect){window->x + (window->w - size) / 2,
					window->y + (window->h - size) / 2, size, size};
}

static void splash_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	(void)area;
	/* ResizeTo 不会缩放子坐标。按当前窗口排版，不在全屏子视图里写死图标偏移。 */
	GYrect window;
	YMGUI_Obj_GetAbsArea(obj->parent, &window);
	GYrect tile = splash_tile_area(&window);
	int id = current_app >= 0 && current_app < APP_COUNT ? current_app : TASKS;
	YMGUI_Draw_Fill(surface, &window, PAPER, GY_OPA_COVER);
	PhoneUI_rounded(surface, &tile, PhoneApps_Get(id)->color, tile.w / 3, GY_OPA_COVER);
	PhoneUI_icon_draw(surface, PhoneApps_Get(id)->icon, tile.x + tile.w / 2, tile.y + tile.h / 2, PhoneApps_Get(id)->color);
}

/* The splash fills its clipped parent window before painting its icon. */
volatile uint32 phone_splash_occlusion_enabled = 1;
static void splash_draw_policy(void)
{
	if (phone_splash_occlusion_enabled)
		app_splash->state |= GY_STATE_OpaqueRect;
	else
		app_splash->state &= ~GY_STATE_OpaqueRect;
}



static void wallpaper_render(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	(void)obj;
	for (int yy = 0; yy < area->h; ++yy)
	{
		if (area->y + yy < surface->clip.y || area->y + yy >= surface->clip.y + surface->clip.h)
			continue;
		int r = theme_alt ? 47 + yy * 54 / H : 42 + yy * 150 / H;
		int g = theme_alt ? 79 + yy * 67 / H : 51 + yy * 61 / H;
		int b = theme_alt ? 90 + yy * 46 / H : 93 + yy * 67 / H;
#if YMGUI_COLOR_DEPTH == 16
		/* RGB565 的低位有序抖动：原生 1x 下避免大面积渐变出现色带。 */
		static const uint8 noise[4][4] = {{0, 4, 1, 5}, {6, 2, 7, 3}, {1, 5, 0, 4}, {7, 3, 6, 2}};
		for (int x = 0; x < area->w; ++x)
		{
			int d = noise[yy & 3][x & 3];
			GY_PutPx(surface, area->x + x, area->y + yy, GY_ColorToPx(RGB(r + d, g + d / 2, b + d)));
		}
#else
		GYrect line = {area->x, (GYcoord)(area->y + yy), area->w, 1};
		YMGUI_Draw_Fill(surface, &line, RGB(r, g, b), GY_OPA_COVER);
#endif
	}
	for (int x = 0; x < area->w; ++x)
	{
		int crest = 239 + (x - 90) * (x - 90) / 620;
		GYrect ribbon = {(GYcoord)(area->x + x), (GYcoord)(area->y + crest), 1, (GYcoord)(H - crest)};
		YMGUI_Draw_Fill(surface, &ribbon, theme_alt ? RGB(69, 135, 147) : RGB(241, 173, 164), 175);
		crest = 315 + (x - 340) * (x - 340) / 750;
		ribbon.y = area->y + crest;
		ribbon.h = H - crest;
		YMGUI_Draw_Fill(surface, &ribbon, theme_alt ? RGB(32, 81, 106) : RGB(86, 76, 133), 240);
		crest = 379 + x * x / 1300;
		ribbon.y = area->y + crest;
		ribbon.h = H - crest;
		YMGUI_Draw_Fill(surface, &ribbon, theme_alt ? RGB(29, 65, 85) : RGB(44, 48, 88), 220);
	}
}



/* Kept writable for an on-board comparison using the same firmware. */
volatile uint32 phone_wallpaper_cache_enabled = 1;
static GYpx* wallpaper_pixels;
static int wallpaper_theme = -1;
static void wallpaper(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	GYrect hit;
	if (!phone_wallpaper_cache_enabled || area->w != W || area->h != H)
	{
		wallpaper_render(obj, surface, area);
		return;
	}
	if (!wallpaper_pixels)
		wallpaper_pixels = (GYpx*)GY_malloc1(W * H * sizeof(GYpx));
	if (!wallpaper_pixels)
	{
		wallpaper_render(obj, surface, area);
		return;
	}
	if (wallpaper_theme != theme_alt)
	{
		GYrect full = {0, 0, W, H};
		GYsurface cache = {wallpaper_pixels, full, full, W};
		wallpaper_render(obj, &cache, &full);
		wallpaper_theme = theme_alt;
	}
	if (!GY_Rect_Intersect(&hit, area, &surface->clip) ||
		!GY_Rect_Intersect(&hit, &hit, &surface->buf_area))
		return;
	for (int y = hit.y; y < hit.y + hit.h; ++y)
		memcpy((GYpx*)surface->buf + (y - surface->buf_area.y) * surface->stride + hit.x - surface->buf_area.x,
			   wallpaper_pixels + (y - area->y) * W + hit.x - area->x, hit.w * sizeof(GYpx));
}

/* HOME and RECENTS share one stationary bar above their page animations. */
static void fixed_status_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	GYsurface clipped = *surface;
	GYrect full = {0, 0, W, H};
	if (!GY_Rect_Intersect(&clipped.clip, area, &surface->clip))
		return;
	wallpaper(obj, &clipped, &full);
}

static void logo_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	(void)obj;
	int cx = area->x + area->w / 2, cy = area->y + area->h / 2;
	YMGUI_Draw_CircleFill(surface, cx, cy, 37, RGB(66, 209, 180));
	YMGUI_Draw_CircleFill(surface, cx, cy, 27, RGB(14, 32, 55));
	YMGUI_Draw_CircleFill(surface, cx + 14, cy - 15, 8, RGB(66, 209, 180));
}

static void show_home(void)
{
	PhoneApps_Hide();
	scene = HOME;
	if (fixed_status)
		YMGUI_Obj_SetHidden(fixed_status, 0);
	busy = 0;
	clearing = 0;
	YMGUI_Obj_SetHidden(boot_page, 1);
	YMGUI_Obj_SetHidden(power_page, 1);
	YMGUI_Obj_SetHidden(app_page, 1);
	YMGUI_Obj_SetHidden(recent_page, 1);
	YMGUI_Obj_SetHidden(home_page, 0);
	PhoneUI_set_pos(home_page, 0, 0);
	if (navigation)
		YMGUI_Obj_SetHidden(navigation, 0);
}

static void begin_boot(void)
{
#if YMGUI_ANIM
	YMGUI_Anim_CancelAll(ctx);
#endif
	home_select(0, 0);
	PhoneApps_CloseAll();
	for (int i = 0; i < APP_COUNT; ++i)
	{
		running[i] = 0;
		snapshot_release(i);
	}
	if (navigation)
		YMGUI_Obj_SetHidden(navigation, 1);
	scene = BOOT;
	if (fixed_status)
		YMGUI_Obj_SetHidden(fixed_status, 1);
	phase_elapsed = 0;
	busy = 0;
	current_app = -1;
	YMGUI_Obj_SetHidden(home_page, 1);
	YMGUI_Obj_SetHidden(app_page, 1);
	YMGUI_Obj_SetHidden(recent_page, 1);
	YMGUI_Obj_SetHidden(power_page, 1);
	YMGUI_Obj_SetHidden(boot_page, 0);
	PhoneUI_set_pos(boot_logo, 0, 106);
	YMGUI_Bar_SetValue(boot_bar, 0);
#if YMGUI_ANIM
	PhoneUI_move(boot_logo, 0, 94, 680);
	PhoneUI_animate_bar(boot_bar, 100, 740);
#else
	show_home();
#endif
}

static void begin_shutdown(void)
{
	if (scene == OFF || scene == SHUTDOWN)
		return;
	PhoneIME_Hide();
	PhoneDesktop_Close();
	PhoneApps_CloseAll();
	scene = SHUTDOWN;
	YMGUI_Obj_SetHidden(fixed_status, 1);
	phase_elapsed = 0;
	busy = 0;
	YMGUI_Obj_SetHidden(power_dialog, 1);
	YMGUI_Obj_SetHidden(navigation, 1);
	YMGUI_Obj_SetHidden(app_page, 1);
	YMGUI_Obj_SetHidden(recent_page, 1);
	YMGUI_Obj_SetHidden(boot_page, 1);
	YMGUI_Obj_SetHidden(power_page, 0);
	YMGUI_Obj_SetHidden(restart_button, 1);
	YMGUI_Obj_SetHidden(power_bar, 0);
	PhoneUI_text_set(power_title, "正在关机");
	YMGUI_Bar_SetValue(power_bar, 0);
#if YMGUI_ANIM
	PhoneUI_animate_bar(power_bar, 100, 700);
#else
	scene = OFF;
	PhoneUI_text_set(power_title, "已关机");
	YMGUI_Obj_SetHidden(power_bar, 1);
	YMGUI_Obj_SetHidden(restart_button, 0);
#endif
}

static void update_running(void)
{
	PhoneApps_SyncRunning(running);
	char text[40];
	int n = 0;
	for (int i = 0; i < APP_COUNT; ++i)
		n += running[i];
	snprintf(text, sizeof(text), "%d 个应用已打开", n);
	PhoneUI_text_set(recent_count, text);
	YMGUI_Obj_SetHidden(recent_empty, n != 0);
	recent_total = 0;
	for (int i = 0; i < APP_COUNT; ++i)
	{
		YMGUI_Obj_SetHidden(recent_cards[i], !running[i]);
		if (running[i])
			recent_ids[recent_total++] = i;
	}
	recent_layout(scene == RECENTS && !clearing);
}


static void prepare_app(int id, int splash, const char* argument)
{
	snapshot_commit();
	current_app = id;
	running[id] = 1;
	if (!snapshots[id])
	{
		snapshots[id] = (GYpx*)GY_malloc1(PREVIEW_W * PREVIEW_H * sizeof(GYpx));
		if (snapshots[id])
			memset(snapshots[id], 0, PREVIEW_W * PREVIEW_H * sizeof(GYpx));
	}
	if (!snapshot_work)
		snapshot_work = (GYpx*)GY_malloc1(W * 448 * sizeof(GYpx));
	snapshot_owner = snapshot_work ? id : -1;
	snapshot_dirty = 0;
	if (snapshot_work)
		memset(snapshot_work, 0, W * 448 * sizeof(GYpx));
	update_running();
	PhoneApps_Show(id, argument);
	PhoneUI_text_set(app_title, PhoneApps_Get(id)->title);
	for (int i = 0; i < APP_COUNT; ++i)
		YMGUI_Obj_SetHidden(app_views[i], i != id || splash);
	YMGUI_Obj_SetHidden(app_splash, !splash);
	splash_elapsed = 0;
}

static int open_app_from(int id, const char* argument, GYOBJ source)
{
	if (busy || id < 0 || id >= APP_COUNT || scene == BOOT || scene == OFF || scene == SHUTDOWN)
		return 0;
	int desktop_index = PhoneDesktop_Index(id);
	if (desktop_index < 0)
		return 0;
	PhoneDesktop_Close();
	PhoneShade_Close(0);
	PhoneIME_Hide();
	PhoneLauncherSlot slot;
	PhoneLauncher_Place(desktop_index, APP_COUNT, &slot);
	home_select(slot.page, 0);
	prepare_app(id, 1, argument);
	YMGUI_Obj_SetHidden(recent_page, 1);
	YMGUI_Obj_SetHidden(app_page, 0);
	GYrect origin;
	YMGUI_Obj_GetAbsArea(source ? source : home_buttons[id], &origin);
	PhoneUI_set_pos(app_page, origin.x, origin.y);
	app_page->area.w = origin.w;
	app_page->area.h = origin.h;
	scene = APP;
	YMGUI_Obj_SetHidden(fixed_status, 1);
	busy = 1;
	PhoneUI_move(app_page, 0, 0, APP_OPEN_MS);
#if YMGUI_ANIM
	if (!YMGUI_Anim_ResizeTo(app_page, W, H, APP_OPEN_MS, GY_ANIM_EASE_OUT))
	{
		app_page->area.w = W;
		app_page->area.h = H;
	}
#else
	app_page->area.w = W;
	app_page->area.h = H;
#endif
#if !YMGUI_ANIM
	busy = 0;
	YMGUI_Obj_SetHidden(app_splash, 1);
	YMGUI_Obj_SetHidden(app_views[id], 0);
#endif
	return 1;
}

int PhoneHost_Open(int id, const char* argument)
{
	return open_app_from(id, argument, NULL);
}

static void open_app(int id)
{
	PhoneHost_Open(id, NULL);
}

static void open_recent(void)
{
	if (busy || scene == RECENTS || scene == BOOT || scene == OFF || scene == SHUTDOWN)
		return;
	PhoneIME_Hide();
	PhoneApps_Hide();
	if (scene == APP)
	{
		YMGUI_Obj_Invalidate(app_page);
		phone_refresh();
	}
	snapshot_commit();
	YMGUI_Obj_SetHidden(app_page, 1);
	PhoneUI_set_pos(app_page, 0, H);
	update_running();
	recent_scroll = 0;
	recent_layout(0);
	PhoneUI_set_pos(recent_page, 0, H);
	YMGUI_Obj_SetHidden(recent_page, 0);
	scene = RECENTS;
	YMGUI_Obj_SetHidden(fixed_status, 0);
	busy = 1;
	PhoneUI_move(recent_page, 0, 0, 280);
#if !YMGUI_ANIM
	busy = 0;
#endif
}

static void go_home(void)
{
	if (busy || scene == HOME || scene == BOOT || scene == OFF || scene == SHUTDOWN)
		return;
	PhoneIME_Hide();
	PhoneApps_Hide();
	if (scene == APP)
	{
		YMGUI_Obj_Invalidate(app_page);
		phone_refresh();
	}
	snapshot_commit();
	GYOBJ leaving = scene == APP ? app_page : recent_page;
	scene = HOME;
	YMGUI_Obj_SetHidden(fixed_status, 0);
	busy = 1;
	phase_elapsed = 0;
	if (leaving == app_page && current_app >= 0)
	{
		GYrect target;
		YMGUI_Obj_GetAbsArea(dock_buttons[current_app], &target);
		PhoneUI_move(leaving, target.x, target.y, 260);
#if YMGUI_ANIM
		YMGUI_Anim_ResizeTo(leaving, target.w, target.h, 260, GY_ANIM_EASE_OUT);
#endif
	}
	else
		PhoneUI_move(leaving, 0, H, 260);
#if !YMGUI_ANIM
	YMGUI_Obj_SetHidden(leaving, 1);
	busy = 0;
#endif
}

static void clear_recent(void)
{
	if (scene != RECENTS || busy)
		return;
	int any = 0;
	for (int slot = 0; slot < recent_total; ++slot)
	{
		int i = recent_ids[slot];
		if (running[i])
		{
			any = 1;
			PhoneUI_move(recent_cards[i], slot % 2 ? W + 8 : -144, recent_cards[i]->area.y, 280 + (slot / 2) * 65);
		}
	}
	if (!any)
		return;
#if YMGUI_ANIM
	busy = 1;
	clearing = 1;
#else
	for (int i = 0; i < APP_COUNT; ++i)
	{
		running[i] = 0;
		snapshot_release(i);
	}
	current_app = -1;
	update_running();
#endif
}

static void advance(uint32 elapsed)
{
	PhoneDesktop_Tick(elapsed);
	PhoneShade_Update((scene == HOME || scene == APP || scene == RECENTS) && !busy && !PhoneDesktop_Busy() &&
					  (power_dialog->state & GY_STATE_Hidden) && !YMGUI_MsgBox_IsShown(app_demo_dialog));
	PhoneIME_Update(scene == APP && !busy && !PhoneShade_Visible() && (power_dialog->state & GY_STATE_Hidden));
	if (elapsed > 100)
		elapsed = 100;
	PhoneApps_Tick(elapsed);
	if (scene == BOOT)
	{
		phase_elapsed += (int)elapsed;
		if (phase_elapsed >= 800)
		{
			show_home();
			PhoneUI_set_pos(home_page, 0, 26);
			PhoneUI_move(home_page, 0, 0, 380);
		}
	}
	else if (scene == SHUTDOWN)
	{
		phase_elapsed += (int)elapsed;
		if (phase_elapsed >= 780)
		{
			scene = OFF;
			PhoneUI_text_set(power_title, "已关机");
			YMGUI_Obj_SetHidden(power_bar, 1);
			YMGUI_Obj_SetHidden(restart_button, 0);
		}
	}
	else if (scene == APP && !(app_splash->state & GY_STATE_Hidden))
	{
		splash_elapsed += (int)elapsed;
		if (splash_elapsed >= APP_OPEN_MS)
		{
			YMGUI_Obj_SetHidden(app_splash, 1);
			YMGUI_Obj_SetHidden(app_views[current_app], 0);
			busy = 0;
		}
	}
#if YMGUI_ANIM
	if (busy && scene == HOME)
	{
		phase_elapsed += elapsed;
		if (phase_elapsed >= 280)
		{
			YMGUI_Obj_SetHidden(app_page, 1);
			YMGUI_Obj_SetHidden(recent_page, 1);
			app_page->area.w = W;
			app_page->area.h = H;
			PhoneUI_set_pos(app_page, 0, H);
			PhoneUI_set_pos(recent_page, 0, H);
			busy = 0;
		}
	}
	if (clearing && scene == RECENTS)
	{
		int moving = 0;
		for (int i = 0; i < APP_COUNT; ++i)
			if (running[i] && recent_cards[i]->area.x > -144 && recent_cards[i]->area.x < W + 8)
				moving = 1;
		if (!moving)
		{
			for (int i = 0; i < APP_COUNT; ++i)
			{
				running[i] = 0;
				snapshot_release(i);
			}
			current_app = -1;
			update_running();
			busy = 0;
			clearing = 0;
		}
	}
	else if (dismiss_id >= 0)
	{
		if (recent_cards[dismiss_id]->area.x <= -156 || recent_cards[dismiss_id]->area.x >= W)
		{
			running[dismiss_id] = 0;
			snapshot_release(dismiss_id);
			dismiss_id = -1;
			busy = 0;
			update_running();
			recent_layout(1);
		}
	}
	else if (busy && scene == RECENTS && recent_page->area.y == 0)
		busy = 0;
#endif
}

static void on_app_icon(GYOBJ btn)
{
	for (int i = 0; i < APP_COUNT; ++i)
		if (home_buttons[i] == btn || dock_buttons[i] == btn)
		{
			open_app_from(i, NULL, btn);
			return;
		}
}
static void on_home(GYOBJ btn)
{
	(void)btn;
	if (PhoneDesktop_Close())
		return;
	if (PhoneShade_Close(1))
		return;
	if (scene == HOME && !busy)
		home_select(0, 1);
	else
		go_home();
}
static void on_recent(GYOBJ btn)
{
	(void)btn;
	PhoneDesktop_Close();
	PhoneShade_Close(0);
	open_recent();
}
static void on_clear(GYOBJ btn)
{
	(void)btn;
	clear_recent();
}
static void on_restart(GYOBJ btn)
{
	(void)btn;
	begin_boot();
}

/* 应用层皮肤复用控件的模型、命中和捕获；不替换控件私有数据。 */



/* One shared active frame; per-app previews are committed before leaving. */
static void phone_flush(struct GYdisp* disp, const GYrect* a, const GYpx* pixels)
{
	if (capture_target)
		for (int y = 0; y < a->h; ++y)
			memcpy(capture_target + (a->y + y) * W + a->x, pixels + y * a->w, a->w * sizeof(GYpx));
	if (test_frame)
		for (int y = 0; y < a->h; ++y)
			memcpy(test_frame + (a->y + y) * W + a->x, pixels + y * a->w, a->w * sizeof(GYpx));
	if (scene == APP && !busy && !PhoneShade_Visible() && current_app >= 0 && snapshot_owner == current_app && snapshot_work)
	{
		snapshot_dirty = 1;
		for (int y = a->y; y < a->y + a->h && y < 448; ++y)
		{
			if (y < 0)
				continue;
			int left = a->x < 0 ? 0 : a->x;
			int right = a->x + a->w > W ? W : a->x + a->w;
			if (right > left)
				memcpy(snapshot_work + y * W + left,
					   pixels + (y - a->y) * a->w + left - a->x,
					   (right - left) * sizeof(GYpx));
		}
	}
	int brightness = display_brightness;
	if (dim_buffer && brightness < 100)
	{
		memcpy(dim_buffer, pixels, a->w * a->h * sizeof(GYpx));
		GYsurface surface = {dim_buffer, *a, *a, a->w};
		YMGUI_Draw_Fill(&surface, a, RGB(0, 0, 0), (100 - brightness) * 160 / 100);
		display_flush(disp, a, dim_buffer);
	}
	else
		display_flush(disp, a, pixels);
}

static void on_mini_music(GYOBJ obj)
{
	(void)obj;
	running[MUSIC] = 1;
	update_running();
	PhoneApps_Command(MUSIC, "toggle", NULL);
}

static void recent_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	int id = (int)(intptr_t)obj->user_data;
	/* Keep all card drawing within its invalidation rectangle. */
	GYsurface clipped = *s;
	int x0 = a->x > s->clip.x ? a->x : s->clip.x;
	int y0 = a->y > s->clip.y ? a->y : s->clip.y;
	int x1 = a->x + a->w < s->clip.x + s->clip.w ? a->x + a->w : s->clip.x + s->clip.w;
	int y1 = a->y + a->h < s->clip.y + s->clip.h ? a->y + a->h : s->clip.y + s->clip.h;
	if (x1 <= x0 || y1 <= y0)
		return;
	clipped.clip = (GYrect){x0, y0, x1 - x0, y1 - y0};
	s = &clipped;
	/* An outside shadow clipped to this rectangle leaves square lower corners.
	 * Leave the rounded corners transparent to the freshly drawn wallpaper. */
	PhoneUI_rounded(s, a, WHITE, 14, GY_OPA_COVER);
	YMGUI_Draw_CircleFill(s, a->x + 19, a->y + 15, 6, PhoneApps_Get(id)->color);
	PhoneUI_draw_text(s, PhoneApps_Get(id)->title, INK, 2, a->x + 33 + PhoneUI_text_width(PhoneApps_Get(id)->title, 2) / 2, a->y + 6);
	PhoneUI_draw_text(s, "x", MUTED, 0, a->x + a->w - 18, a->y + 2);
	if (snapshots[id] && snapshot_valid[id])
	{
		GYrect target = {a->x + 8, a->y + 30, 120, 168};
		/* Preview is scaled once when the app leaves; animation only copies pixels. */
		GYrect hit;
		if (GY_Rect_Intersect(&hit, &target, &s->clip) && GY_Rect_Intersect(&hit, &hit, &s->buf_area))
			for (int y = hit.y; y < hit.y + hit.h; ++y)
				memcpy(s->buf + (y - s->buf_area.y) * s->stride + hit.x - s->buf_area.x,
					   snapshots[id] + (y - target.y) * PREVIEW_W + hit.x - target.x,
					   hit.w * sizeof(GYpx));
	}
	else
	{
		GYrect target = {a->x + 8, a->y + 30, 120, 168};
		PhoneUI_rounded(s, &target, PAPER, 8, GY_OPA_COVER);
		GYrect tile = {a->x + 41, a->y + 87, 54, 54};
		PhoneUI_rounded(s, &tile, PhoneApps_Get(id)->color, 14, GY_OPA_COVER);
		PhoneUI_icon_draw(s, PhoneApps_Get(id)->icon, a->x + 68, a->y + 114, PhoneApps_Get(id)->color);
	}
}

static int recent_scroll_limit(void)
{
	int limit = ((recent_total + 1) / 2) * 222 - 312;
	return limit > 0 ? limit : 0;
}

static void recent_layout(int animate)
{
	if (recent_scroll < 0)
		recent_scroll = 0;
	if (recent_scroll > recent_scroll_limit())
		recent_scroll = recent_scroll_limit();
	for (int i = 0; i < recent_total; ++i)
	{
		GYOBJ card = recent_cards[recent_ids[i]];
		int x = 16 + (i % 2) * 152;
		int y = 4 + (i / 2) * 222 - recent_scroll;
		card_home_y[recent_ids[i]] = y;

		if (animate)
			PhoneUI_move(card, x, y, 240);
		else
			PhoneUI_set_pos(card, x, y);
	}
}

static void dismiss_recent(int id)
{
	if (busy || id < 0 || !running[id])
		return;
#if YMGUI_ANIM
	busy = 1;
	dismiss_id = id;
	PhoneUI_move(recent_cards[id], drag_dx < 0 ? -160 : W + 8, recent_cards[id]->area.y, 260);
#else
	running[id] = 0;
	snapshot_release(id);
	update_running();
#endif
}

static void recent_event(GYOBJ obj, GYEvent event)
{
	if (busy || scene != RECENTS)
		return;
	int id = (int)(intptr_t)obj->user_data;
	if (event == GY_EVENT_Pressed)
	{
		drag_x = ctx->point_x;
		drag_y = ctx->point_y;
		drag_dx = drag_dy = dragging = drag_axis = 0;
		drag_scroll = recent_scroll;
		for (int i = 0; i < APP_COUNT; ++i)
			drag_origins[i] = recent_cards[i]->area.x;
		drag_origin_y = obj->area.y;
#if YMGUI_ANIM
		for (int i = 0; i < APP_COUNT; ++i)
			YMGUI_Anim_CancelObject(recent_cards[i]);
#endif
	}
	else if (event == GY_EVENT_Pressing)
	{
		drag_dx = ctx->point_x - drag_x;
		drag_dy = ctx->point_y - drag_y;
		if (!drag_axis && abs(drag_dx) + abs(drag_dy) > 8)
		{
			dragging = 1;
			drag_axis = abs(drag_dx) > abs(drag_dy) && id >= 0 ? 1 : 2;
		}
		if (drag_axis == 1)
			PhoneUI_set_pos(obj, drag_origins[id] + drag_dx, drag_origin_y);
		else if (drag_axis == 2)
		{
			recent_scroll = drag_scroll - drag_dy;
			recent_layout(0);
		}
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
	{
		if (!ctx->pressed_obj)
		{
			dragging = 1;
			recent_scroll = drag_scroll;
			recent_layout(1);
			return;
		}
		if (drag_axis == 1 && abs(drag_dx) > 60)
			dismiss_recent(id);
		else
			recent_layout(1);
	}
	else if (event == GY_EVENT_Clicked && !dragging && id >= 0)
	{
		GYrect a;
		YMGUI_Obj_GetAbsArea(obj, &a);
		if (ctx->point_y < a.y + 30 && ctx->point_x > a.x + a.w - 30)
			dismiss_recent(id);
		else
			open_app(id);
	}
	else if (event == GY_EVENT_Wheel)
	{
		recent_scroll -= ctx->wheel_y * 64;
		recent_layout(1);
	}
}

static void cancel_power(GYOBJ obj)
{
	(void)obj;
	YMGUI_Obj_SetHidden(power_dialog, 1);
}
static void confirm_power(GYOBJ obj)
{
	(void)obj;
	begin_shutdown();
}
static void power_backdrop_event(GYOBJ obj, GYEvent event)
{
	if (event == GY_EVENT_Clicked)
		cancel_power(obj);
}
static void dim_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	YMGUI_Draw_Fill(s, a, RGB(8, 12, 25), 160);
}
static void show_power_dialog(void)
{
	if (busy || scene == BOOT || scene == OFF || scene == SHUTDOWN)
		return;
	PhoneShade_Close(0);
	PhoneDesktop_Close();
	YMGUI_Obj_SetHidden(power_dialog, 0);
	GYOBJ sheet = power_dialog->child_head;
	PhoneUI_set_pos(sheet, 18, H);
	PhoneUI_move(sheet, 18, 286, 240);
}
static void on_back(GYOBJ obj)
{
	(void)obj;
	if (PhoneDesktop_Close())
		return;
	if (PhoneShade_Close(1))
		return;
	if (PhoneIME_Hide())
		return;
	if (YMGUI_MsgBox_IsShown(app_demo_dialog))
	{
		YMGUI_MsgBox_Close(app_demo_dialog);
		return;
	}
	if (scene == APP && !busy && PhoneApps_Back())
		return;
	if (!(power_dialog->state & GY_STATE_Hidden))
		cancel_power(NULL);
	else
		go_home();
}
static void nav_event(GYOBJ obj, GYEvent event)
{
	if (event == GY_EVENT_ContextRequested)
		show_power_dialog();
	else
		PhoneUI_button_event(obj, event);
}
static void build_navigation(void)
{
	fixed_status = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, 0, W, 28);
	fixed_status->draw_cb = fixed_status_draw;
	fixed_status->state |= GY_STATE_ClipChildren | GY_STATE_OpaqueRect;
	status_bar(fixed_status);
	YMGUI_Obj_SetHidden(fixed_status, 1);
	navigation = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, 448, W, 32);
	YMGUI_Obj_SetBgColor(navigation, RGB(28, 32, 49));
	GYOBJ back = PhoneUI_button(navigation, 34, 0, 60, 32, "", on_back, 0);
	GYOBJ home = PhoneUI_button(navigation, 130, 0, 60, 32, "", on_home, 0);
	GYOBJ recent = PhoneUI_button(navigation, 226, 0, 60, 32, "", on_recent, 0);
	PhoneUI_button_icon(back, ICON_BACK);
	PhoneUI_button_icon(home, ICON_HOME);
	PhoneUI_button_icon(recent, ICON_RECENTS);
	home->event_cb = nav_event;
	power_dialog = YMGUI_Creat_Obj_Creat(ctx->top_layer, 0, 0, W, H);
	power_dialog->draw_cb = dim_draw;
	power_dialog->event_cb = power_backdrop_event;
	GYOBJ sheet = PhoneUI_panel(power_dialog, 18, 286, 284, 172, WHITE);
	PhoneUI_left_label(sheet, 20, 14, 244, "确认关机？", INK, 3);
	PhoneUI_left_label(sheet, 20, 55, 244, "关闭演示，稍后可以重新开机。", MUTED, 2);
	PhoneUI_button(sheet, 16, 96, 120, 44, "取消", cancel_power, RGB(127, 137, 158));
	PhoneUI_button(sheet, 148, 96, 120, 44, "关机", confirm_power, RGB(211, 83, 106));
	YMGUI_Obj_SetHidden(power_dialog, 1);
}

static void dots_draw(GYOBJ obj, GYSURFACE s, const GYrect* a)
{
	(void)obj;
	for (int i = 0; i < HOME_PAGES; ++i)
		YMGUI_Draw_CircleFill(s, a->x + 11 + i * 16, a->y + 7, i == home_index ? 3 : 2,
							  i == home_index ? WHITE : RGB(166, 159, 192));
}

static void home_select(int index, int animate)
{
	home_index = index < 0 ? 0 : index >= HOME_PAGES ? HOME_PAGES - 1
													 : index;
	if (animate)
		PhoneUI_move(home_strip, -home_index * W, 0, 260);
	else
	{
#if YMGUI_ANIM
		YMGUI_Anim_CancelObject(home_strip);
#endif
		PhoneUI_set_pos(home_strip, -home_index * W, 0);
	}
	YMGUI_Obj_SetHidden(home_dock, home_index != 0 || PhoneDesktop_Index(MUSIC) < 0);
	PhoneUI_set_pos(home_dots, home_dots->area.x, home_index == 0 ? 348 : 422);
	YMGUI_Obj_Invalidate(home_dots);
}

/* 透明手势层覆盖桌面内容：图标/标签/空白处都可以开始翻页，拖动不误开 App。 */
static void home_event(GYOBJ obj, GYEvent event)
{
	(void)obj;
	if (scene != HOME || busy)
		return;
	if (event == GY_EVENT_ContextRequested || event == GY_EVENT_ContextDragging ||
		event == GY_EVENT_ContextReleased || event == GY_EVENT_ContextCancelled)
	{
		home_dragged = 1;
		PhoneDesktop_Context(event);
		return;
	}
	if (PhoneDesktop_Busy())
		return;
	if (event == GY_EVENT_Pressed)
	{
#if YMGUI_ANIM
		YMGUI_Anim_CancelObject(home_strip);
#endif
		home_start_x = ctx->point_x;
		home_start_y = ctx->point_y;
		home_origin = home_strip->area.x;
		home_dragged = 0;
		home_pressed_app = -1;
		for (int i = 0; i < APP_COUNT; ++i)
		{
			if (PhoneDesktop_Index(i) < 0)
				continue;
			GYrect a;
			YMGUI_Obj_GetAbsArea(home_buttons[i], &a);
			if (ctx->point_x >= a.x - 5 && ctx->point_x < a.x + a.w + 5 &&
				ctx->point_y >= a.y && ctx->point_y < a.y + a.h + 22)
			{
				home_pressed_app = i;
				home_buttons[i]->state |= GY_STATE_Pressed;
				YMGUI_Obj_Invalidate(home_buttons[i]);
			}
		}
	}
	else if (event == GY_EVENT_Pressing)
	{
		int dx = ctx->point_x - home_start_x, dy = ctx->point_y - home_start_y;
		if (abs(dx) + abs(dy) > 8)
			home_dragged = 1;
		if (home_dragged)
		{
			int x = home_origin + dx;
			if (x > 0)
				x /= 4;
			int edge = -(HOME_PAGES - 1) * W;
			if (x < edge)
				x = edge + (x - edge) / 4;
			PhoneUI_set_pos(home_strip, x, 0);
			if (home_pressed_app >= 0)
			{
				home_buttons[home_pressed_app]->state &= (uint8)~GY_STATE_Pressed;
				YMGUI_Obj_Invalidate(home_buttons[home_pressed_app]);
			}
		}
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
	{
		if (home_pressed_app >= 0)
		{
			home_buttons[home_pressed_app]->state &= (uint8)~GY_STATE_Pressed;
			YMGUI_Obj_Invalidate(home_buttons[home_pressed_app]);
		}
		if (!ctx->pressed_obj)
		{
			home_dragged = 1;
			home_select(home_index, 1);
			return;
		}
		int dx = ctx->point_x - home_start_x;
		int dy = ctx->point_y - home_start_y;
		int target = home_index;
		if (abs(dx) > 48 && abs(dx) > abs(dy))
			target += dx < 0 ? 1 : -1;
		if (abs(home_origin + home_index * W) > 2)
			home_dragged = 1;
		home_select(target, 1);
	}
	else if (event == GY_EVENT_Clicked && !home_dragged && home_pressed_app >= 0)
	{
		open_app(home_pressed_app);
	}
	else if (event == GY_EVENT_Wheel)
	{
		int delta = ctx->wheel_x ? ctx->wheel_x : -ctx->wheel_y;
		home_select(home_index + (delta > 0 ? 1 : -1), 1);
	}
}

static void dots_event(GYOBJ obj, GYEvent event)
{
	if (event == GY_EVENT_Clicked && scene == HOME && !busy)
		home_select((ctx->point_x - obj->area.x - 3) / 16, 1);
}

static void desktop_removed(int id)
{
	running[id] = 0;
	snapshot_release(id);
	if (current_app == id)
		current_app = -1;
	update_running();
}
static void build_home(void)
{
	home_page = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, W, H);
	home_page->draw_cb = wallpaper;
	home_page->state |= GY_STATE_OpaqueRect;
	/* Status bar is shared with RECENTS in the stationary top layer. */
	GYOBJ viewport = YMGUI_Creat_Obj_Creat(home_page, 0, 28, W, 408);
	viewport->state |= GY_STATE_ClipChildren;
	viewport->draw_cb = NULL;
	home_strip = YMGUI_Creat_Obj_Creat(viewport, 0, 0, W * HOME_PAGES, 408);
	home_strip->draw_cb = NULL;
	GYOBJ pages[HOME_PAGES];
	for (int i = 0; i < HOME_PAGES; ++i)
	{
		pages[i] = YMGUI_Creat_Obj_Creat(home_strip, W * i, 0, W, 408);
		pages[i]->draw_cb = NULL;
	}
	PhoneUI_left_label(pages[0], 22, 9, 240, "09:41", WHITE, 1);
	PhoneUI_left_label(pages[0], 25, 75, 272, "9月29日  星期二", RGB(232, 230, 243), 0);
	GYOBJ widget = PhoneUI_panel(pages[0], 22, 123, 276, 79, RGB(68, 71, 110));
	widget->draw_cb = PhoneUI_glass_draw;
	PhoneUI_left_label(widget, 16, 11, 170, "给生活留一点空白", WHITE, 0);
	PhoneUI_left_label(widget, 16, 40, 178, "多云 / 22 C", RGB(224, 224, 240), 2);
	GYOBJ weather = YMGUI_Creat_Obj_Creat(widget, 210, 12, 52, 52);
	weather->draw_cb = PhoneUI_weather_draw;

	PhoneDesktop_Init(ctx, home_strip, home_buttons, &home_index, home_select, desktop_removed);
	for (int i = 0; i < APP_COUNT; ++i)
		dock_buttons[i] = home_buttons[i];
	GYOBJ touch = YMGUI_Creat_Obj_Creat(home_page, 0, 28, W, 408);
	touch->draw_cb = NULL;
	touch->event_cb = home_event;
	home_dots = YMGUI_Creat_Obj_Creat(home_page, (W - HOME_PAGES * 16 - 6) / 2, 348, HOME_PAGES * 16 + 6, 17);
	home_dots->draw_cb = dots_draw;
	home_dots->event_cb = dots_event;

	GYOBJ dock = home_dock = PhoneUI_panel(home_page, 15, 368, 290, 68, RGB(42, 44, 73));
	dock->draw_cb = PhoneUI_glass_draw;
	dock_buttons[MUSIC] = PhoneUI_button(dock, 12, 13, 42, 42, "", on_app_icon, PhoneApps_Get(MUSIC)->color);
	PhoneUI_button_icon(dock_buttons[MUSIC], ICON_MUSIC);
	PhoneUI_left_label(dock, 65, 12, 167, "晚间海浪", WHITE, 0);
	home_music_status = PhoneUI_left_label(dock, 65, 36, 170, "Yaomi 音乐 / 已暂停", RGB(206, 207, 227), 2);
	mini_music_button = PhoneUI_button(dock, 242, 14, 36, 40, "", on_mini_music, 0);
	PhoneUI_button_icon(mini_music_button, ICON_PLAY);
}



static void build_app_views(void)
{
	app_page = YMGUI_Creat_Obj_Creat(ctx->root, 0, H, W, H);
	app_page->state |= GY_STATE_ClipChildren;
	YMGUI_Obj_SetBgColor(app_page, PAPER);
	status_bar(app_page);
	app_title = PhoneUI_label(app_page, 80, 2, 160, "", INK);
	YMGUI_Obj_SetHidden(app_title, 1);
	for (int i = 0; i < APP_COUNT; ++i)
	{
		app_views[i] = YMGUI_Creat_Obj_Creat(app_page, 0, 36, W, 412);
		YMGUI_Obj_SetBgColor(app_views[i], PAPER);
		app_views[i]->state |= GY_STATE_ClipChildren;
		PhoneApps_Create(i, app_views[i]);
	}

	app_splash = YMGUI_Creat_Obj_Creat(app_page, 0, 0, W, H);
	app_splash->draw_cb = splash_draw;
	app_splash->state |= GY_STATE_OpaqueRect;
	YMGUI_Obj_SetHidden(app_page, 1);
}

static void build_recent(void)
{
	recent_page = YMGUI_Creat_Obj_Creat(ctx->root, 0, H, W, H);
	recent_page->draw_cb = wallpaper;
	recent_page->state |= GY_STATE_OpaqueRect;
	recent_page->state |= GY_STATE_ClipChildren;
	/* No moving status-bar copy inside this animated page. */
	PhoneUI_left_label(recent_page, 22, 31, 260, "最近应用", WHITE, 3);
	recent_count = PhoneUI_left_label(recent_page, 24, 66, 264, "", RGB(217, 221, 239), 2);
	recent_viewport = YMGUI_Creat_Obj_Creat(recent_page, 0, 86, W, 312);
	recent_viewport->state |= GY_STATE_ClipChildren;
	recent_viewport->draw_cb = NULL;
	recent_viewport->event_cb = recent_event;
	recent_viewport->user_data = (void*)(intptr_t)-1;
	for (int i = 0; i < APP_COUNT; ++i)
	{
		recent_cards[i] = YMGUI_Creat_Obj_Creat(recent_viewport, 16 + (i % 2) * 152, 4 + (i / 2) * 222, 136, 206);
		recent_cards[i]->draw_cb = recent_draw;
		recent_cards[i]->event_cb = recent_event;
		recent_cards[i]->user_data = (void*)(intptr_t)i;
	}
	recent_empty = PhoneUI_label(recent_page, 20, 220, 280, "已清理，轻装出发。", WHITE);
	PhoneUI_button(recent_page, 106, 408, 108, 32, "全部清理", on_clear, RGB(83, 83, 124));
	YMGUI_Obj_SetHidden(recent_page, 1);
}

static void build_boot_power(void)
{
	boot_page = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, W, H);
	YMGUI_Obj_SetBgColor(boot_page, RGB(10, 25, 44));
	boot_logo = YMGUI_Creat_Obj_Creat(boot_page, 0, 106, W, 86);
	boot_logo->draw_cb = logo_draw;
	PhoneUI_label(boot_page, 40, 225, 240, "Y A O M I", RGB(237, 248, 251));
	PhoneUI_label(boot_page, 40, 258, 240, "点亮你的小小世界", RGB(140, 184, 202));
	boot_bar = YMGUI_Creat_Bar_Creat(boot_page, 65, 343, 190, 7);
	YMGUI_Bar_SetRange(boot_bar, 0, 100);
	YMGUI_Bar_SetColors(boot_bar, RGB(35, 65, 81), RGB(81, 223, 186));
	PhoneUI_label(boot_page, 45, 365, 230, "正在启动", RGB(134, 172, 190));

	power_page = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, W, H);
	YMGUI_Obj_SetBgColor(power_page, RGB(8, 18, 32));
	GYOBJ mark = YMGUI_Creat_Obj_Creat(power_page, 0, 100, W, 86);
	mark->draw_cb = logo_draw;
	power_title = PhoneUI_label(power_page, 42, 221, 236, "正在关机", RGB(234, 248, 250));
	power_bar = YMGUI_Creat_Bar_Creat(power_page, 65, 280, 190, 7);
	YMGUI_Bar_SetRange(power_bar, 0, 100);
	YMGUI_Bar_SetColors(power_bar, RGB(35, 56, 73), RGB(81, 223, 186));
	restart_button = PhoneUI_button(power_page, 75, 336, 170, 46, "重新开机", on_restart, RGB(44, 114, 113));
	YMGUI_Obj_SetHidden(power_page, 1);
}

static int build_ui(void)
{
	YMGUI_Obj_SetBgColor(ctx->root, RGB(9, 23, 40));
	build_home();
	build_app_views();
	build_recent();
	build_boot_power();
	build_navigation();
	if (!PhoneIME_Init(ctx))
		return 0;
	PhoneShade_Init(ctx);
	PhoneDesktop_BuildOverlay();
	/* 模态最后创建，盖住导航与键盘；不能让底层按钮穿透。 */
	app_demo_dialog = YMGUI_Creat_MsgBox_Creat(ctx);
	YMGUI_MsgBox_SetColors(app_demo_dialog, GY_ARGB(140, 15, 20, 35), WHITE, RGB(215, 219, 231), INK, MUTED);
	update_running();
	begin_boot();
	return 1;
}


void PhoneHost_SetTheme(int alternate)
{
	theme_alt = !!alternate;
	YMGUI_Obj_Invalidate(home_page);
	if (fixed_status)
		YMGUI_Obj_Invalidate(fixed_status);
}
void PhoneHost_SetBrightness(int value)
{
	display_brightness = value < 0 ? 0 : value > 100 ? 100
													 : value;
	YMGUI_Obj_Invalidate(ctx->root);
	PhoneShade_Sync();
	PhoneApps_Command(SETTINGS, "sync", NULL);
}
int PhoneHost_GetBrightness(void)
{
	return display_brightness;
}
void PhoneHost_SetWifi(int on)
{
	PhoneShade_SetWifi(on);
	PhoneApps_Command(SETTINGS, "sync", NULL);
}
int PhoneHost_GetWifi(void)
{
	return PhoneShade_GetWifi();
}
void PhoneHost_Capture(GYpx* pixels)
{
	capture_target = pixels;
	YMGUI_Obj_Invalidate(ctx->root);
	YMGUI_Obj_Invalidate(ctx->top_layer);
	phone_refresh();
	capture_target = NULL;
}
void PhoneHost_MusicChanged(int playing)
{
	if (mini_music_button)
		PhoneUI_button_icon(mini_music_button, playing ? ICON_PAUSE : ICON_PLAY);
	if (home_music_status)
		PhoneUI_text_set(home_music_status, playing ? "Yaomi 音乐 / 播放中" : "Yaomi 音乐 / 已暂停");
}
void PhoneHost_Power(void)
{
	show_power_dialog();
}
void PhoneHost_Close(void)
{
	if (busy || scene != APP)
		return;
	int id = current_app;
	go_home();
	running[id] = 0;
	snapshot_release(id);
	current_app = -1;
	update_running();
}
static void notice_result(GYOBJ dialog, int index)
{
	YMGUI_MsgBox_Close(dialog);
	if (index == 1)
		PhoneHost_Close();
}
void PhoneHost_Notice(const char* title, const char* message)
{
	if (busy || scene != APP || !title || !message)
		return;
	PhoneIME_Hide();
	YMGUI_MsgBox_SetTitle(app_demo_dialog, title);
	YMGUI_MsgBox_SetText(app_demo_dialog, message);
	YMGUI_MsgBox_ClearButtons(app_demo_dialog);
	YMGUI_MsgBox_AddButton(app_demo_dialog, "关闭弹窗", notice_result);
	YMGUI_MsgBox_AddButton(app_demo_dialog, "返回桌面", notice_result);
	YMGUI_MsgBox_Show(app_demo_dialog);
}

/* Board key navigation: short KEY1 selects, short KEY2 clicks;
   long KEY1 is Back, long KEY2 is Home. */
static int key_app = -1, key_control = -1, key_recent = -1, focus_count;


static GYpoint focus_points[96];
static GYOBJ focus_objects[96], key_focus;
static void collect_controls(GYOBJ obj, int x, int y)
{
	if (!obj || (obj->state & GY_STATE_Hidden))
		return;
	x += obj->area.x;
	y += obj->area.y;
	if (obj->event_cb && (!PhoneIME_Visible() || PhoneIME_IsKey(obj)) && obj->area.w > 0 && obj->area.h > 0 && focus_count < 96)
	{
		int cx = x + obj->area.w / 2, cy = y + obj->area.h / 2;
		if (cx >= 0 && cx < W && cy >= 28 && cy < H)
		{
			focus_objects[focus_count] = obj;
			focus_points[focus_count].x = cx;
			focus_points[focus_count++].y = cy;
		}
	}
	for (GYOBJ child = obj->child_head; child; child = child->sibling)
		collect_controls(child, x - obj->scroll_x, y - obj->scroll_y);
}
static void phone_cursor(int x, int y)
{
	if (runtime_config.cursor)
		runtime_config.cursor(x, y, runtime_config.user);
}
static void board_cursor_refresh(void)
{
	YMGUI_Obj_Invalidate(ctx->root);
	YMGUI_Obj_Invalidate(ctx->top_layer);
}
void PhoneShell_Key(int key, int long_press)
{
	if (!ctx || busy || scene == BOOT || (key != 1 && key != 2))
		return;
	if (long_press)
	{
		if (key == 1)
			on_back(NULL);
		else
			on_home(NULL);
		key_control = -1;
		key_focus = NULL;
		phone_cursor(-1, -1);
		board_cursor_refresh();
		return;
	}
	if (scene == RECENTS && !PhoneShade_Visible())
	{
		if (key == 1 || key_recent < 0)
			key_recent = (key_recent + 1) % (recent_total + 1);
		if (key_recent >= recent_total)
		{
			phone_cursor(160, 424);
			board_cursor_refresh();
			if (key == 2)
				on_clear(NULL);
		}
		else
		{
			recent_scroll = (key_recent / 2) * 222;
			recent_layout(0);
			GYrect a;
			YMGUI_Obj_GetAbsArea(recent_cards[recent_ids[key_recent]], &a);
			phone_cursor(a.x + a.w / 2, a.y + a.h / 2);
			board_cursor_refresh();
			if (key == 2)
			{
				phone_cursor(-1, -1);
				PhoneHost_Open(recent_ids[key_recent], NULL);
			}
		}
		return;
	}
	if (scene == HOME && !PhoneShade_Visible())
	{
		if (key == 1)
		{
			key_app = (key_app + 1) % (APP_COUNT + 1);
			if (key_app == APP_COUNT)
				phone_cursor(256, 464);
			else
			{
				PhoneLauncherSlot slot;
				PhoneLauncher_Place(PhoneDesktop_Index(key_app), APP_COUNT, &slot);
				home_select(slot.page, 0);
				phone_cursor(slot.x + 24, slot.y + 28 + 24);
			}
			board_cursor_refresh();
		}
		else
		{
			if (key_app < 0)
				key_app = 0;
			phone_cursor(-1, -1);
			key_control = -1;
			key_focus = NULL;
			if (key_app == APP_COUNT)
				on_recent(NULL);
			else
				PhoneHost_Open(key_app, NULL);
		}
	}
	else
	{
		focus_count = 0;
		if (PhoneIME_Visible())
			collect_controls(PhoneIME_Keyboard(), 0, 0);
		else if (YMGUI_MsgBox_IsShown(app_demo_dialog) || !(power_dialog->state & GY_STATE_Hidden))
			collect_controls(ctx->top_layer, 0, 0);
		else
		{
			collect_controls(ctx->root, 0, 0);
			collect_controls(ctx->top_layer, 0, 0);
		}
		if (!focus_count)
			return;
		key_control = -1;
		for (int i = 0; i < focus_count; ++i)
			if (focus_objects[i] == key_focus)
			{
				key_control = i;
				break;
			}
		if (key == 1 || key_control < 0)
			key_control = (key_control + 1) % focus_count;
		if (key_control >= focus_count)
			key_control = 0;
		key_focus = focus_objects[key_control];
		int x = focus_points[key_control].x, y = focus_points[key_control].y;
		phone_cursor(x, y);
		board_cursor_refresh();
		if (key == 2)
		{
			YMGUI_Inject_Pointer(x, y, 1);
			YMGUI_Inject_Pointer(x, y, 0);
		}
	}
}
static void phone_touch_frame(const GYtouchpoint* points, uint8 count, void* user)
{
	(void)user;
	if (scene == APP && !busy && !PhoneShade_Visible() && !PhoneIME_Visible() &&
		(power_dialog->state & GY_STATE_Hidden) && !YMGUI_MsgBox_IsShown(app_demo_dialog))
		PhoneApps_Touch(points, count);
	else
		PhoneApps_Touch(NULL, 0);
}
int PhoneShell_Init(GYDISP display, const PhoneRuntimeConfig* config)
{
	if (ctx || !display || !display->buf1 || !display->flush_cb ||
		display->hor_res != W || display->ver_res != H || display->buf_px_cnt < W)
		return 0;
	memset(&runtime_config, 0, sizeof(runtime_config));
	if (config)
		runtime_config = *config;
	runtime_display = display;
	dim_buffer = GY_malloc1(display->buf_px_cnt * sizeof(GYpx));
	if (!dim_buffer)
	{
		runtime_display = NULL;
		return 0;
	}
	display_flush = display->flush_cb;
	display->flush_cb = phone_flush;
	ctx = YMGUI_Creat_Ctx_Creat(display, W, H);
	if (!ctx || !PhoneApps_Validate())
	{
		PhoneShell_Shutdown();
		return 0;
	}
	key_app = key_control = key_recent = -1;
	key_focus = NULL;
	focus_count = 0;
	scene = BOOT;
	current_app = -1;
	busy = clearing = splash_elapsed = phase_elapsed = 0;
	display_brightness = 100;
	theme_alt = 0;
	recent_scroll = recent_total = home_index = 0;
	home_dragged = dragging = drag_axis = 0;
	dismiss_id = home_pressed_app = -1;
	memset(running, 0, sizeof(running));
	PhoneLocale_Init();
	YMGUI_Inject_SetCtx(ctx);
	if (!PhoneQuick_RegisterDefaults())
	{
		PhoneShell_Shutdown();
		return 0;
	}
	if (!build_ui())
	{
		PhoneShell_Shutdown();
		return 0;
	}
	YMGUI_Inject_SetTouchListener(phone_touch_frame, NULL);
	return 1;
}
void PhoneShell_Step(uint32 elapsed_ms)
{
	if (!ctx)
		return;
	ctx->tick_elapsed = elapsed_ms;
	YMGUI_Inject_Tick(elapsed_ms);
	PhoneShell_Pump();
}
void PhoneShell_Pump(void)
{
	if (!ctx)
		return;
	advance(ctx->tick_elapsed);
	ctx->tick_elapsed = 0;
	YMGUI_TextActions_Update(ctx, PhoneIME_Visible() ? 226 : 448, scene == APP && !busy && !PhoneShade_Visible() && (power_dialog->state & GY_STATE_Hidden) && !YMGUI_MsgBox_IsShown(app_demo_dialog));
	phone_refresh();
}
GYCTX PhoneShell_Context(void)
{
	return ctx;
}
void PhoneShell_Shutdown(void)
{
	if (ctx)
	{
		YMGUI_Inject_SetTouchListener(NULL, NULL);
		YMGUI_Inject_SetCtx(NULL);
		PhoneShade_Shutdown();
		PhoneApps_Destroy();
		PhoneIME_Shutdown();
		PhoneLocale_Shutdown();
		YMGUI_Free_CtxFree(ctx);
		ctx = NULL;
	}
	for (int i = 0; i < APP_COUNT; ++i)
		snapshot_release(i);
	GY_free1(wallpaper_pixels);
	wallpaper_pixels = NULL;
	wallpaper_theme = -1;
	GY_free1(dim_buffer);
	dim_buffer = NULL;
	if (runtime_display)
		runtime_display->flush_cb = display_flush;
	runtime_display = NULL;
	display_flush = NULL;
	capture_target = NULL;
	navigation = fixed_status = power_dialog = app_demo_dialog = mini_music_button = home_music_status = NULL;
}


#ifndef PHONE_PORTABLE_BUILD
#include "phone_selftest.inc"

static void select_preview(const char* arg)
{
	if (!strcmp(arg, "--home-menu") || !strcmp(arg, "--home-drag"))
	{
		show_home();
		step_time(400);
		YMGUI_Inject_ContextBegin(48, 289);
		if (!strcmp(arg, "--home-drag"))
			YMGUI_Inject_ContextMove(121, 292);
		return;
	}
	if (!strcmp(arg, "--shade") || !strcmp(arg, "--screenshot") || !strncmp(arg, "--shade-clear-", 14))
	{
		show_home();
		step_time(400);
		PhoneShade_Open();
		step_time(260);
		if (!strcmp(arg, "--screenshot"))
		{
			click_at(200, 185);
			step_time(260);
		}
		if (!strncmp(arg, "--shade-clear-", 14))
		{
			click_at(270, 282);
			step_time(!strcmp(arg, "--shade-clear-mid") ? 160 : 400);
#if YMGUI_ANIM
			if (!strcmp(arg, "--shade-clear-mid"))
			{
				YMGUI_Anim_CancelObject((GYOBJ)PhoneShade_Inspect("card0"));
				YMGUI_Anim_CancelObject((GYOBJ)PhoneShade_Inspect("card1"));
			}
#endif
		}
		return;
	}
	if (!strcmp(arg, "--boot"))
		return;
	/* 冻结启动过渡，供首帧/中途截图检查，不修改动画采样算法。 */
	if (!strncmp(arg, "--launch-start-", 15) || !strncmp(arg, "--launch-mid-", 13))
	{
		int first = !strncmp(arg, "--launch-start-", 15);
		int id = PhoneApps_Find(arg + (first ? 15 : 13));
		if (id < 0)
			return;
		show_home();
		open_app(id);
		if (!first)
			step_time(120);
#if YMGUI_ANIM
		YMGUI_Anim_CancelObject(app_page);
#endif
		return;
	}
	if (!strcmp(arg, "--camera-dialog") || !strcmp(arg, "--browser-dialog"))
	{
		select_preview(!strcmp(arg, "--camera-dialog") ? "--camera" : "--browser");
		PhoneApps_Command(current_app, "notice", NULL);
		return;
	}
	if (!strcmp(arg, "--ime") || !strcmp(arg, "--ime-idle") || !strcmp(arg, "--symbols"))
	{
		select_preview("--notes");
		PhoneApps_Command(NOTES, "set-body", "你好，Yaomi。\n记录今天的小想法。");
		click_at(75, 188);
		step_time(220);
		if (!strcmp(arg, "--symbols"))
		{
			click_at(25, 421);
			return;
		}
		if (!strcmp(arg, "--ime-idle"))
			return;
		const char* py = "nihao";
		for (const char* p = py; *p; ++p)
			YMGUI_Inject_Key(*p, 1);
		return;
	}
	if (!strcmp(arg, "--clear-mid") || !strcmp(arg, "--clear-done"))
	{
		select_preview("--recents");
		clear_recent();
		step_time(!strcmp(arg, "--clear-mid") ? 100 : 1000);
#if YMGUI_ANIM
		if (!strcmp(arg, "--clear-mid"))
			for (int i = 0; i < APP_COUNT; ++i)
				YMGUI_Anim_CancelObject(recent_cards[i]);
#endif
		return;
	}
	show_home();
	int app_id = -1;
	for (int i = 0; i < APP_COUNT; ++i)
		if (!strncmp(arg, "--", 2) && !strcmp(arg + 2, PhoneApps_Get(i)->key))
			app_id = i;
	if (!strncmp(arg, "--home", 6) && arg[6] >= '2' && arg[6] <= '0' + HOME_PAGES)
	{
		home_select(arg[6] - '1', 0);
		return;
	}
	if (app_id >= 0)
	{
		prepare_app(app_id, 0, NULL);
		YMGUI_Obj_SetHidden(app_page, 0);
		PhoneUI_set_pos(app_page, 0, 0);
		scene = APP;
	}
	else if (!strcmp(arg, "--recents"))
	{
		for (int i = 0; i < APP_COUNT; ++i)
		{
			prepare_app(i, 0, NULL);
			app_page->area.w = W;
			app_page->area.h = H;
			YMGUI_Obj_SetHidden(app_page, 0);
			PhoneUI_set_pos(app_page, 0, 0);
			scene = APP;
			YMGUI_Obj_Invalidate(ctx->root);
			phone_refresh();
		}
		PhoneApps_Hide();
		YMGUI_Obj_SetHidden(app_page, 1);
		recent_scroll = 0;
		update_running();
		YMGUI_Obj_SetHidden(recent_page, 0);
		PhoneUI_set_pos(recent_page, 0, 0);
		scene = RECENTS;
	}
	else if (!strcmp(arg, "--dialog"))
	{
		show_power_dialog();
	}
	else if (!strcmp(arg, "--power"))
	{
		begin_shutdown();
		scene = OFF;
		PhoneUI_text_set(power_title, "已关机");
		YMGUI_Obj_SetHidden(power_bar, 1);
		YMGUI_Obj_SetHidden(restart_button, 0);
	}
}

int main(int argc, char** argv)
{
	int max_frames = -1, frame = 0, test = 0;
	const char* preview = NULL;
	if (argc > 1)
	{
		if (!strcmp(argv[1], "--selftest"))
		{
			test = 1;
			max_frames = 4;
		}
		else if (argv[1][0] == '-')
		{
			preview = argv[1];
			max_frames = 40;
		}
		else
			max_frames = atoi(argv[1]);
	}
	GYdisp disp = {0};
	disp.hor_res = W;
	disp.ver_res = H;
	disp.buf_px_cnt = W * 48;
	disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
	if (test)
	{
		PhoneQuick_RegisterDefaults();
		if (!PhoneQuick_Register(&quick_probe))
			++failures;
	}
	if (!disp.buf1 || SDL_LCD_Init(&disp, 1) != 0 || !PhoneShell_Init(&disp, NULL))
	{
		failures = 1;
		goto done;
	}
	SDL_LCD_SetTitle("Yaomi Phone - YMGUI");
	if (test)
		selftest();
	else if (preview)
		select_preview(preview);
	else if (getenv("YMGUI_SHOT"))
		select_preview("--home");
	while (SDL_LCD_PumpEvents())
	{
		if (!test && !preview && !getenv("YMGUI_SHOT"))
			advance(ctx->tick_elapsed);
		phone_refresh();
		SDL_LCD_Delay(16);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}
done:
	PhoneShell_Shutdown();
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	return failures ? 1 : 0;
}

#endif
