#include "phone_ui.h"
#include "phone_host.h"

typedef struct
{
	GYOBJ gallery_panel, gallery_caption;
	int gallery_index;
} AppState;
static AppState state;

static void on_gallery(GYOBJ btn);


static void on_gallery(GYOBJ btn)
{
	(void)btn;
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
	PhoneUI_app_header(view, "相册", "收藏安静的风景");
	state.gallery_panel = PhoneUI_panel(view, 18, 65, 284, 230, RGB(196, 105, 113));
	state.gallery_panel->draw_cb = PhoneUI_gallery_draw;
	state.gallery_panel->event_cb = NULL;
	state.gallery_caption = PhoneUI_left_label(view, 22, 307, 276, "01 / 珊瑚", INK, 3);
	PhoneUI_left_label(view, 23, 345, 275, "示例风景 / Yaomi 相册", MUTED, 2);
	PhoneUI_button(view, 18, 373, 284, 34, "下一张风景   >", on_gallery, RGB(147, 112, 136));
}

static void app_destroy(void)
{
	memset(&state, 0, sizeof(state));
}
static intptr_t app_inspect(const char* name, int index)
{
	(void)index;
	if (!strcmp(name, "gallery_panel"))
		return (intptr_t)state.gallery_panel;
	if (!strcmp(name, "gallery_caption"))
		return (intptr_t)state.gallery_caption;
	if (!strcmp(name, "gallery_index"))
		return (intptr_t)state.gallery_index;
	return 0;
}

const PhoneApp PhoneApp_gallery = {
	.key = "gallery", .title = "相册", .color = RGB(239, 165, 72), .icon = ICON_GALLERY, .create = app_create, .inspect = app_inspect, .destroy = app_destroy};
