#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ music_button, music_status, music_seek;
	int music_playing, music_elapsed;
} AppState;
static AppState state;

static void app_tick(uint32 elapsed);
static void app_close(void);
static int app_command(const char* name, const char* argument);
static void album_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area);
static void update_music(void);
static void on_music(GYOBJ btn);


static void app_tick(uint32 elapsed)
{
	if (state.music_playing)
	{
		state.music_elapsed += elapsed;
		if (state.music_elapsed >= 1000)
		{
			state.music_elapsed -= 1000;
			YMGUI_Slider_SetValue(state.music_seek, (YMGUI_Slider_GetValue(state.music_seek) + 1) % 101);
		}
	}
}
static void app_close(void)
{
	state.music_playing = 0;
	update_music();
}
static int app_command(const char* name, const char* argument)
{
	(void)argument;
	if (strcmp(name, "toggle"))
		return 0;
	on_music(state.music_button);
	return 1;
}

static void album_draw(GYOBJ obj, GYSURFACE surface, const GYrect* area)
{
	(void)obj;
	PhoneUI_rounded(surface, area, RGB(83, 70, 135), 22, GY_OPA_COVER);
	YMGUI_Draw_CircleFill(surface, area->x + 90, area->y + 80, 70, RGB(168, 117, 170));
	YMGUI_Draw_CircleFill(surface, area->x + 90, area->y + 80, 56, RGB(44, 50, 99));
	YMGUI_Draw_CircleFill(surface, area->x + 90, area->y + 80, 37, RGB(113, 96, 167));
	YMGUI_Draw_CircleFill(surface, area->x + 90, area->y + 80, 9, RGB(31, 42, 74));
}

static void update_music(void)
{
	PhoneUI_button_icon(state.music_button, state.music_playing ? ICON_PAUSE : ICON_PLAY);
	PhoneHost_MusicChanged(state.music_playing);
	PhoneUI_text_set(state.music_status, state.music_playing ? "Yaomi 音乐 / 播放中" : "Yaomi 音乐 / 已暂停");
}

static void on_music(GYOBJ btn)
{
	state.music_playing = !state.music_playing;
	update_music();
#if YMGUI_ANIM
	if (YMGUI_Anim_BgColorTo(btn, state.music_playing ? RGB(71, 190, 166) : RGB(123, 107, 194),
							 250, GY_ANIM_SMOOTH))
		return;
#endif
	YMGUI_Obj_SetBgColor(btn, state.music_playing ? RGB(71, 190, 166) : RGB(123, 107, 194));
}
static void app_create(GYOBJ view)
{
	state = (AppState){0};
	PhoneUI_app_header(view, "音乐", "今天也有好心情");
	GYOBJ album = YMGUI_Creat_Obj_Creat(view, 68, 62, 184, 160);
	album->draw_cb = album_draw;
	PhoneUI_label_ex(view, 20, 225, 280, "晚间海浪", INK, 3);
	state.music_status = PhoneUI_label_ex(view, 20, 264, 280, "Yaomi 音乐 / 已暂停", MUTED, 2);
	state.music_seek = YMGUI_Creat_Slider_Creat(view, 30, 289, 260, 22);
	YMGUI_Slider_SetValue(state.music_seek, 32);
	state.music_seek->draw_cb = PhoneUI_slider_draw;
	PhoneUI_left_label(view, 30, 313, 100, "1:24", MUTED, 2);
	PhoneUI_label_ex(view, 252, 313, 40, "4:20", MUTED, 2);
	state.music_button = PhoneUI_button(view, 134, 344, 52, 52, "", on_music, PhoneApps_Get(MUSIC)->color);
	PhoneUI_button_icon(state.music_button, ICON_PLAY);
	/* 前后曲是视觉标记；本演示只有一首曲目的播放状态。 */
	GYOBJ prev = PhoneUI_panel(view, 68, 351, 38, 38, RGB(156, 161, 181));
	prev->draw_cb = PhoneUI_icon_tile_draw;
	prev->user_data = (void*)(intptr_t)ICON_PREVIOUS;
	GYOBJ next = PhoneUI_panel(view, 214, 351, 38, 38, RGB(156, 161, 181));
	next->draw_cb = PhoneUI_icon_tile_draw;
	next->user_data = (void*)(intptr_t)ICON_NEXT;
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "music_button"))
		return (intptr_t)state.music_button;
	if (!strcmp(name, "music_status"))
		return (intptr_t)state.music_status;
	if (!strcmp(name, "music_seek"))
		return (intptr_t)state.music_seek;
	if (!strcmp(name, "music_playing"))
		return (intptr_t)state.music_playing;
	if (!strcmp(name, "music_elapsed"))
		return (intptr_t)state.music_elapsed;
	return 0;
}

const PhoneApp PhoneApp_music = {
	.key = "music", .title = "音乐", .color = RGB(228, 91, 130), .icon = ICON_MUSIC, .create = app_create, .inspect = app_inspect, .tick = app_tick, .close = app_close, .command = app_command, .destroy = app_destroy};
