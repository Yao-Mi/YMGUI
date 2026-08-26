#include "YMGUI_Plugin.h"
#include "YMGUI_PubDefine.h"
#include <stdio.h>

static const YMGUI_PluginHost* s_host;
static GYOBJ s_panel;
static GYOBJ s_uptime;
static GYOBJ s_refresh_count;
static uint32 s_elapsed_ms;
static uint32 s_refreshes;

static void setLabel(GYOBJ label, const char* text, GYcolor color)
{
	s_host->label_set_text(label, text);
	s_host->label_set_color(label, color);
}

static GYOBJ makeMetric(GYcoord x, const char* title, const char* value, GYcolor color)
{
	GYOBJ card = s_host->create_object(s_panel, x, 66, 154, 92);
	GYOBJ label;
	s_host->object_set_bg(card, GY_ARGB(0xFF, 0x22, 0x28, 0x2E));
	label = s_host->create_label(card, 8, 10, 138, 20);
	setLabel(label, title, GY_ARGB(0xFF, 0x91, 0x9B, 0xA4));
	label = s_host->create_label(card, 8, 42, 138, 32);
	setLabel(label, value, color);
	return label;
}

static void onRefresh(GYOBJ button)
{
	char text[32];
	(void)button;
	s_refreshes++;
	snprintf(text, sizeof(text), "%lu snapshots", (unsigned long)s_refreshes);
	s_host->label_set_text(s_refresh_count, text);
	s_host->log("plugin snapshot refreshed");
}

static int pluginInit(const YMGUI_PluginHost* host)
{
	GYOBJ label;
	GYOBJ button;
	#if defined(_WIN32)
	const char* platform = "Windows DLL";
	#elif defined(__ANDROID__)
	const char* platform = "Android SO";
	#elif defined(__APPLE__)
	const char* platform = "macOS DYLIB";
	#else
	const char* platform = "Linux SO";
	#endif
	if (host == NULL || host->struct_size < YMGUI_PLUGIN_HOST_UI_SIZE || host->root == NULL ||
		host->create_object == NULL || host->create_label == NULL || host->create_button == NULL)
		return -1;
	s_host = host;
	s_panel = host->create_object(host->root, 0, 0, host->root->area.w, host->root->area.h);
	if (s_panel == NULL)
		return -1;
	host->object_set_bg(s_panel, GY_ARGB(0xFF, 0x19, 0x1E, 0x23));
	label = host->create_label(s_panel, 16, 10, 250, 28);
	setLabel(label, "System Information", GY_ARGB(0xFF, 0xF0, 0xF2, 0xF4));
	label = host->create_label(s_panel, 350, 12, 164, 24);
	host->label_set_bg(label, 1);
	host->object_set_bg(label, GY_ARGB(0xFF, 0x1E, 0x72, 0x4D));
	setLabel(label, "RUNNING", GY_ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	makeMetric(16, "PLATFORM", platform, GY_ARGB(0xFF, 0x63, 0xC7, 0xD1));
	makeMetric(190, "PLUGIN API", "Version 1", GY_ARGB(0xFF, 0xF1, 0xC7, 0x58));
	s_uptime = makeMetric(364, "SESSION UPTIME", "0 seconds", GY_ARGB(0xFF, 0x75, 0xD5, 0x9A));
	button = host->create_button(s_panel, 16, 180, 152, 36);
	host->button_set_text(button, "Refresh snapshot");
	host->button_set_colors(button, GY_ARGB(0xFF, 0x2C, 0x75, 0x7D), GY_ARGB(0xFF, 0x1E, 0x52, 0x58));
	host->button_set_clicked(button, onRefresh);
	s_refresh_count = host->create_label(s_panel, 184, 184, 180, 28);
	setLabel(s_refresh_count, "0 snapshots", GY_ARGB(0xFF, 0xA8, 0xB0, 0xB8));
	host->log("system-info UI mounted");
	return 0;
}

static void pluginTick(uint32 elapsed_ms)
{
	char text[32];
	uint32 old_seconds = s_elapsed_ms / 1000u;
	s_elapsed_ms += elapsed_ms;
	if (s_uptime != NULL && s_elapsed_ms / 1000u != old_seconds)
	{
		snprintf(text, sizeof(text), "%lu seconds", (unsigned long)(s_elapsed_ms / 1000u));
		s_host->label_set_text(s_uptime, text);
	}
}

static void pluginDeinit(void)
{
	if (s_host != NULL && s_host->log != NULL)
		s_host->log("system-info UI unmounted");
	if (s_panel != NULL && s_host != NULL && s_host->destroy_object != NULL)
		s_host->destroy_object(s_panel);
	s_panel = NULL;
	s_uptime = NULL;
	s_refresh_count = NULL;
	s_elapsed_ms = 0;
	s_refreshes = 0;
	s_host = NULL;
}

YMGUI_PLUGIN_EXPORT const YMGUI_Plugin* YMGUI_Plugin_Get(void)
{
	static const YMGUI_Plugin plugin = {
		sizeof(YMGUI_Plugin),
		"system-info",
		"System Information",
		YMGUI_PLUGIN_API_VERSION,
		pluginInit,
		pluginTick,
		pluginDeinit
	};
	return &plugin;
}
