#include "YMGUI_PubDefine.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Label.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Plugin.h"
#include "SDL_LCD.h"
#include <stdio.h>
#include <stdlib.h>

static GYOBJ s_plugin_label;
static uint32 s_plugin_elapsed;
static const YMGUI_PluginHost* s_plugin_host;

static int systemInfoInit(const YMGUI_PluginHost* host)
{
	s_plugin_label = host->create_label(host->root, 20, 82, 280, 28);
	s_plugin_host = host;
	if (s_plugin_label == NULL)
		return -1;
	YMGUI_Label_SetTextColor(s_plugin_label, GY_ARGB(0xFF, 0x70, 0xD0, 0xFF));
	YMGUI_Label_SetText(s_plugin_label, "Plugin loaded: system-info");
	if (host->log != NULL)
		host->log("system-info plugin initialized");
	return 0;
}

static void systemInfoTick(uint32 elapsed_ms)
{
	char text[64];
	s_plugin_elapsed += elapsed_ms;
	if (s_plugin_label != NULL && (s_plugin_elapsed % 500u) < elapsed_ms)
	{
		sprintf(text, "system-info plugin | uptime %lus", (unsigned long)(s_plugin_elapsed / 1000u));
		YMGUI_Label_SetText(s_plugin_label, text);
	}
}

static void systemInfoDeinit(void)
{
	if (s_plugin_label != NULL && s_plugin_host != NULL && s_plugin_host->destroy_object != NULL)
		s_plugin_host->destroy_object(s_plugin_label);
	s_plugin_label = NULL;
	s_plugin_host = NULL;
	s_plugin_elapsed = 0;
}

static const YMGUI_Plugin s_system_info_plugin = {
	sizeof(YMGUI_Plugin),
	"system-info",
	"System Information",
	YMGUI_PLUGIN_API_VERSION,
	systemInfoInit,
	systemInfoTick,
	systemInfoDeinit
};

static GYOBJ hostCreateLabel(GYOBJ p, GYcoord x, GYcoord y, GYcoord w, GYcoord h) { return YMGUI_Creat_Label_Creat(p, x, y, w, h); }
static void hostDestroyObject(GYOBJ object) { YMGUI_Free_ObjFree(object); }
static void hostLabelSetText(GYOBJ label, const char* text) { YMGUI_Label_SetText(label, text); }
static void hostLabelSetColor(GYOBJ label, GYcolor color) { YMGUI_Label_SetTextColor(label, color); }
static void hostLog(const char* text) { fprintf(stderr, "[plugin] %s\n", text ? text : ""); }

int main(int argc, char** argv)
{
	GYdisp disp = {0};
	GYCTX ctx;
	YMGUI_PluginHost host = {0};
	int frames = argc > 1 ? atoi(argv[1]) : -1;
	int frame = 0;
	disp.hor_res = 320; disp.ver_res = 160; disp.buf_px_cnt = 320 * 40;
	disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
	if (SDL_LCD_Init(&disp, 3) != 0) return 1;
	ctx = YMGUI_Creat_Ctx_Creat(&disp, disp.hor_res, disp.ver_res);
	YMGUI_Obj_SetBgColor(ctx->root, GY_ARGB(0xFF, 0x12, 0x18, 0x20));
	GYOBJ title = YMGUI_Creat_Label_Creat(ctx->root, 20, 24, 280, 28);
	YMGUI_Label_SetText(title, "YMGUI Plugin Host");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF, 0xF0, 0xC0, 0x40));
	host.struct_size = sizeof(YMGUI_PluginHost);
	host.api_version = YMGUI_PLUGIN_API_VERSION;
	host.ctx = ctx;
	host.root = ctx->root;
	host.create_label = hostCreateLabel;
	host.destroy_object = hostDestroyObject;
	host.label_set_text = hostLabelSetText;
	host.label_set_color = hostLabelSetColor;
	host.log = hostLog;
	YMGUI_PluginRegistry_Reset();
	YMGUI_PluginRegistry_Add(&s_system_info_plugin);
	printf("registered: %s\n", s_system_info_plugin.name);
	YMGUI_PluginRegistry_LoadAll(&host);
	YMGUI_Inject_SetCtx(ctx);
	while (SDL_LCD_PumpEvents())
	{
		YMGUI_PluginRegistry_Tick(33);
		YMGUI_Refresh(ctx);
		SDL_LCD_Delay(33);
		if (frames > 0 && ++frame >= frames) break;
	}
	YMGUI_PluginRegistry_UnloadAll();
	YMGUI_Free_CtxFree(ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	return 0;
}
