#include "YMGUI_Button.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Plugin.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_TextView.h"
#include "SDL_LCD.h"
#include <SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

#define SCR_W 800
#define SCR_H 480
#define BAND_H 60
#define SIDEBAR_W 220
#define HEADER_H 52
#define LOG_LINES 5
#define LOG_LINE_MAX 72
#define DISCOVERED_MAX 5
#define PLUGIN_PATH_MAX 512

typedef struct
{
	YMGUI_PluginInfo info;
	char path[PLUGIN_PATH_MAX];
	GYOBJ row;
	GYOBJ badge;
} DiscoveredPlugin;

static GYCTX g_ctx;
static GYOBJ g_plugin_mount;
static GYOBJ g_placeholder;
static GYOBJ g_summary;
static GYOBJ g_load_btn;
static GYOBJ g_unload_btn;
static GYOBJ g_log_view;
static YMGUI_PluginHost g_host;
static DiscoveredPlugin g_plugins[DISCOVERED_MAX];
static uint8 g_plugin_count;
static int g_selected = -1;
static int g_active = -1;
static uint8 g_loaded;
static char g_log_lines[LOG_LINES][LOG_LINE_MAX];
static uint8 g_log_count;
static uint16 g_log_serial;
static int g_selftest_fail;

static int hasPluginExtension(const char* name)
{
	const char* ext = strrchr(name, '.');
	if (ext == NULL)
		return 0;
#if defined(_WIN32)
	return _stricmp(ext, ".dll") == 0;
#elif defined(__APPLE__)
	return strcmp(ext, ".dylib") == 0 || strcmp(ext, ".so") == 0;
#else
	return strcmp(ext, ".so") == 0;
#endif
}

static int pluginCompare(const void* a, const void* b)
{
	const DiscoveredPlugin* pa = (const DiscoveredPlugin*)a;
	const DiscoveredPlugin* pb = (const DiscoveredPlugin*)b;
	return strcmp(pa->info.name, pb->info.name);
}

static void inspectCandidate(const char* directory, const char* name)
{
	DiscoveredPlugin candidate;
	uint8 i;
	int n;
	if (!hasPluginExtension(name) || g_plugin_count >= DISCOVERED_MAX)
		return;
	memset(&candidate, 0, sizeof(candidate));
	n = snprintf(candidate.path, sizeof(candidate.path), "%s%c%s", directory,
#if defined(_WIN32)
		'\\',
#else
		'/',
#endif
		name);
	if (n < 0 || (size_t)n >= sizeof(candidate.path) ||
		YMGUI_Plugin_InspectDynamic(candidate.path, &candidate.info) != 0)
		return;
	for (i = 0; i < g_plugin_count; ++i)
		if (strcmp(g_plugins[i].info.id, candidate.info.id) == 0)
			return;
	g_plugins[g_plugin_count++] = candidate;
}

static void scanPlugins(const char* directory)
{
	memset(g_plugins, 0, sizeof(g_plugins));
	g_plugin_count = 0;
#if defined(_WIN32)
	WIN32_FIND_DATAA data;
	HANDLE find;
	char pattern[PLUGIN_PATH_MAX];
	if (snprintf(pattern, sizeof(pattern), "%s\\*", directory) < 0)
		return;
	find = FindFirstFileA(pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			inspectCandidate(directory, data.cFileName);
	} while (FindNextFileA(find, &data));
	FindClose(find);
#else
	DIR* dir = opendir(directory);
	struct dirent* entry;
	if (dir == NULL)
		return;
	while ((entry = readdir(dir)) != NULL)
		inspectCandidate(directory, entry->d_name);
	closedir(dir);
#endif
	qsort(g_plugins, g_plugin_count, sizeof(g_plugins[0]), pluginCompare);
	g_selected = g_plugin_count > 0 ? 0 : -1;
}

static void executableDirectory(const char* argv0, char* out, size_t out_size)
{
	const char* slash;
	const char* backslash;
	size_t len;
	if (argv0 == NULL)
	{
		snprintf(out, out_size, ".");
		return;
	}
	slash = strrchr(argv0, '/');
	backslash = strrchr(argv0, '\\');
	if (backslash != NULL && (slash == NULL || backslash > slash))
		slash = backslash;
	if (slash == NULL)
	{
		snprintf(out, out_size, ".");
		return;
	}
	len = (size_t)(slash - argv0);
	if (len == 0)
		len = 1;
	if (len >= out_size)
		len = out_size - 1;
	memcpy(out, argv0, len);
	out[len] = '\0';
}

static void rebuildLog(void)
{
	char text[LOG_LINES * LOG_LINE_MAX];
	size_t off = 0;
	uint8 i;
	text[0] = '\0';
	for (i = 0; i < g_log_count; i++)
	{
		int n = snprintf(text + off, sizeof(text) - off, "%s%s", i ? "\n" : "", g_log_lines[i]);
		if (n < 0 || (size_t)n >= sizeof(text) - off)
			break;
		off += (size_t)n;
	}
	YMGUI_TextView_SetText(g_log_view, text);
}

static void hostLog(const char* text)
{
	if (text == NULL)
		text = "";
	printf("[plugin-manager] %s\n", text);
	if (g_log_view == NULL)
		return;
	if (g_log_count == LOG_LINES)
	{
		uint8 i;
		for (i = 1; i < LOG_LINES; i++)
			memcpy(g_log_lines[i - 1], g_log_lines[i], LOG_LINE_MAX);
		g_log_count--;
	}
	g_log_serial++;
	snprintf(g_log_lines[g_log_count++], LOG_LINE_MAX, "%02u  %s", g_log_serial, text);
	rebuildLog();
}

static GYOBJ hostCreateObject(GYOBJ p, GYcoord x, GYcoord y, GYcoord w, GYcoord h) { return YMGUI_Creat_Obj_Creat(p, x, y, w, h); }
static GYOBJ hostCreateLabel(GYOBJ p, GYcoord x, GYcoord y, GYcoord w, GYcoord h) { return YMGUI_Creat_Label_Creat(p, x, y, w, h); }
static GYOBJ hostCreateButton(GYOBJ p, GYcoord x, GYcoord y, GYcoord w, GYcoord h) { return YMGUI_Creat_Button_Creat(p, x, y, w, h); }
static void hostDestroyObject(GYOBJ o) { YMGUI_Free_ObjFree(o); }
static void hostObjectSetBg(GYOBJ o, GYcolor c) { YMGUI_Obj_SetBgColor(o, c); }
static void hostObjectSetHidden(GYOBJ o, uint8 hidden) { YMGUI_Obj_SetHidden(o, hidden); }
static void hostLabelSetText(GYOBJ o, const char* t) { YMGUI_Label_SetText(o, t); }
static void hostLabelSetColor(GYOBJ o, GYcolor c) { YMGUI_Label_SetTextColor(o, c); }
static void hostLabelSetBg(GYOBJ o, uint8 on) { YMGUI_Label_SetBgEnable(o, on); }
static void hostButtonSetText(GYOBJ o, const char* t) { YMGUI_Button_SetText(o, t); }
static void hostButtonSetColors(GYOBJ o, GYcolor a, GYcolor b) { YMGUI_Button_SetColors(o, a, b); }
static void hostButtonSetClicked(GYOBJ o, YMGUI_PluginClickedFn cb) { YMGUI_Button_SetClicked(o, cb); }

static void updateManagerState(void)
{
	uint8 i;
	char summary[96];
	for (i = 0; i < g_plugin_count; ++i)
	{
		YMGUI_Label_SetText(g_plugins[i].badge, g_loaded && g_active == i ? "ACTIVE" : "INACTIVE");
		YMGUI_Obj_SetBgColor(g_plugins[i].badge, g_loaded && g_active == i ?
			GY_ARGB(0xFF, 0x1E, 0x72, 0x4D) : GY_ARGB(0xFF, 0x55, 0x5C, 0x65));
		YMGUI_Button_SetColors(g_plugins[i].row, i == g_selected ?
			GY_ARGB(0xFF, 0x31, 0x3A, 0x42) : GY_ARGB(0xFF, 0x25, 0x2B, 0x31),
			GY_ARGB(0xFF, 0x1D, 0x23, 0x29));
	}
	snprintf(summary, sizeof(summary), "%u active  |  %u found  |  API v1",
		g_loaded ? 1u : 0u, (unsigned)g_plugin_count);
	YMGUI_Label_SetText(g_summary, summary);
	if (g_loaded)
	{
		YMGUI_Button_SetColors(g_load_btn, GY_ARGB(0xFF, 0x35, 0x3B, 0x43), GY_ARGB(0xFF, 0x2A, 0x2F, 0x36));
		YMGUI_Button_SetColors(g_unload_btn, GY_ARGB(0xFF, 0xB8, 0x45, 0x3E), GY_ARGB(0xFF, 0x82, 0x2F, 0x2A));
	}
	else
	{
		YMGUI_Button_SetColors(g_load_btn, g_selected >= 0 ? GY_ARGB(0xFF, 0x1E, 0x72, 0x4D) : GY_ARGB(0xFF, 0x35, 0x3B, 0x43),
			g_selected >= 0 ? GY_ARGB(0xFF, 0x15, 0x50, 0x36) : GY_ARGB(0xFF, 0x2A, 0x2F, 0x36));
		YMGUI_Button_SetColors(g_unload_btn, GY_ARGB(0xFF, 0x35, 0x3B, 0x43), GY_ARGB(0xFF, 0x2A, 0x2F, 0x36));
	}
	YMGUI_Obj_SetHidden(g_placeholder, g_loaded);
	if (!g_loaded)
		YMGUI_Label_SetText(g_placeholder, g_plugin_count ? "Select Load to activate the plugin" : "No valid plugins found in this directory");
}

static int loadPlugin(void)
{
	int rc;
	char msg[80];
	if (g_loaded)
	{
		hostLog("a plugin is already active");
		return 0;
	}
	if (g_selected < 0 || g_selected >= g_plugin_count)
	{
		hostLog("no valid plugin selected");
		return -1;
	}
	rc = YMGUI_Plugin_LoadDynamic(g_plugins[g_selected].path, &g_host);
	if (rc != 0)
	{
		snprintf(msg, sizeof(msg), "load failed (error %d)", rc);
		hostLog(msg);
		return rc;
	}
	g_loaded = 1;
	g_active = g_selected;
	updateManagerState();
	snprintf(msg, sizeof(msg), "%s activated", g_plugins[g_active].info.id);
	hostLog(msg);
	return 0;
}

static void onLoad(GYOBJ button) { (void)button; loadPlugin(); }

static void onUnload(GYOBJ button)
{
	(void)button;
	if (!g_loaded)
	{
		hostLog("no active plugin to unload");
		return;
	}
	YMGUI_PluginRegistry_UnloadAll();
	g_loaded = 0;
	g_active = -1;
	updateManagerState();
	hostLog("plugin unloaded safely");
}

static void onSelect(GYOBJ button)
{
	uint8 i;
	if (g_loaded)
	{
		hostLog("unload the active plugin before changing selection");
		return;
	}
	for (i = 0; i < g_plugin_count; ++i)
		if (g_plugins[i].row == button)
		{
			g_selected = i;
			updateManagerState();
			return;
		}
}

static void clickAt(GYcoord x, GYcoord y)
{
	YMGUI_Inject_Pointer(x, y, 1);
	YMGUI_Inject_Pointer(x, y, 0);
}

static void runSelftest(void)
{
	uint16 serial;
	if (g_plugin_count != 1)
		g_selftest_fail = 1;
	clickAt(728, 78); /* Manager Unload */
	if (g_loaded)
		g_selftest_fail = 1;
	clickAt(622, 78); /* Manager Load */
	if (!g_loaded)
		g_selftest_fail = 1;
	serial = g_log_serial;
	clickAt(332, 304); /* Plugin-owned Refresh snapshot */
	if (g_log_serial == serial)
		g_selftest_fail = 1;
	hostLog(g_selftest_fail ? "selftest failed" : "selftest passed: unload/load/plugin action");
}

static void buildHostApi(void)
{
	memset(&g_host, 0, sizeof(g_host));
	g_host.struct_size = sizeof(g_host);
	g_host.api_version = YMGUI_PLUGIN_API_VERSION;
	g_host.ctx = g_ctx;
	g_host.root = g_plugin_mount;
	g_host.create_label = hostCreateLabel;
	g_host.destroy_object = hostDestroyObject;
	g_host.label_set_text = hostLabelSetText;
	g_host.label_set_color = hostLabelSetColor;
	g_host.log = hostLog;
	g_host.create_object = hostCreateObject;
	g_host.create_button = hostCreateButton;
	g_host.object_set_bg = hostObjectSetBg;
	g_host.object_set_hidden = hostObjectSetHidden;
	g_host.label_set_bg = hostLabelSetBg;
	g_host.button_set_text = hostButtonSetText;
	g_host.button_set_colors = hostButtonSetColors;
	g_host.button_set_clicked = hostButtonSetClicked;
}

static void buildUi(void)
{
	GYOBJ root = g_ctx->root;
	GYOBJ header = YMGUI_Creat_Obj_Creat(root, 0, 0, SCR_W, HEADER_H);
	GYOBJ sidebar;
	GYOBJ content;
	GYOBJ label;
	uint8 i;
	YMGUI_Obj_SetBgColor(root, GY_ARGB(0xFF, 0x0D, 0x11, 0x15));
	YMGUI_Obj_SetBgColor(header, GY_ARGB(0xFF, 0x18, 0x1D, 0x22));
	label = YMGUI_Creat_Label_Creat(header, 16, 8, 190, 34);
	YMGUI_Label_SetText(label, "YMGUI Plugins");
	YMGUI_Label_SetTextColor(label, GY_ARGB(0xFF, 0xF1, 0xC7, 0x58));
	g_summary = YMGUI_Creat_Label_Creat(header, 520, 12, 260, 28);
	YMGUI_Label_SetTextColor(g_summary, GY_ARGB(0xFF, 0xA8, 0xB0, 0xB8));

	sidebar = YMGUI_Creat_Obj_Creat(root, 0, HEADER_H, SIDEBAR_W, SCR_H - HEADER_H);
	YMGUI_Obj_SetBgColor(sidebar, GY_ARGB(0xFF, 0x14, 0x19, 0x1E));
	label = YMGUI_Creat_Label_Creat(sidebar, 14, 16, 192, 22);
	YMGUI_Label_SetText(label, "INSTALLED PLUGINS");
	YMGUI_Label_SetTextColor(label, GY_ARGB(0xFF, 0x7F, 0x8A, 0x94));
	if (g_plugin_count == 0)
	{
		label = YMGUI_Creat_Label_Creat(sidebar, 14, 58, 192, 48);
		YMGUI_Label_SetText(label, "No valid plugins found");
		YMGUI_Label_SetTextColor(label, GY_ARGB(0xFF, 0x7F, 0x8A, 0x94));
	}
	for (i = 0; i < g_plugin_count; ++i)
	{
		GYcoord y = 52 + i * 56;
		g_plugins[i].row = YMGUI_Creat_Button_Creat(sidebar, 10, y, 200, 34);
		YMGUI_Button_SetText(g_plugins[i].row, g_plugins[i].info.name);
		YMGUI_Button_SetClicked(g_plugins[i].row, onSelect);
		label = YMGUI_Creat_Label_Creat(sidebar, 14, y + 37, 110, 18);
		YMGUI_Label_SetText(label, g_plugins[i].info.id);
		YMGUI_Label_SetTextColor(label, GY_ARGB(0xFF, 0x63, 0xC7, 0xD1));
		g_plugins[i].badge = YMGUI_Creat_Label_Creat(sidebar, 132, y + 37, 70, 18);
		YMGUI_Label_SetBgEnable(g_plugins[i].badge, 1);
	}
	label = YMGUI_Creat_Label_Creat(sidebar, 18, 360, 184, 20);
	YMGUI_Label_SetText(label, "ABI-validated plugins");
	YMGUI_Label_SetTextColor(label, GY_ARGB(0xFF, 0x7F, 0x8A, 0x94));
	label = YMGUI_Creat_Label_Creat(sidebar, 18, 386, 184, 20);
	YMGUI_Label_SetText(label, "ABI: host function table");
	YMGUI_Label_SetTextColor(label, GY_ARGB(0xFF, 0x7F, 0x8A, 0x94));

	content = YMGUI_Creat_Obj_Creat(root, SIDEBAR_W, HEADER_H, SCR_W - SIDEBAR_W, SCR_H - HEADER_H);
	YMGUI_Obj_SetBgColor(content, GY_ARGB(0xFF, 0x0F, 0x13, 0x17));
	label = YMGUI_Creat_Label_Creat(content, 20, 12, 236, 30);
	YMGUI_Label_SetText(label, "Plugin Manager");
	g_load_btn = YMGUI_Creat_Button_Creat(content, 354, 10, 96, 32);
	YMGUI_Button_SetText(g_load_btn, "Load");
	YMGUI_Button_SetClicked(g_load_btn, onLoad);
	g_unload_btn = YMGUI_Creat_Button_Creat(content, 460, 10, 96, 32);
	YMGUI_Button_SetText(g_unload_btn, "Unload");
	YMGUI_Button_SetClicked(g_unload_btn, onUnload);
	g_plugin_mount = YMGUI_Creat_Obj_Creat(content, 20, 54, 536, 236);
	YMGUI_Obj_SetBgColor(g_plugin_mount, GY_ARGB(0xFF, 0x19, 0x1E, 0x23));
	g_placeholder = YMGUI_Creat_Label_Creat(g_plugin_mount, 70, 96, 396, 32);
	YMGUI_Label_SetText(g_placeholder, "Select Load to activate the plugin");
	YMGUI_Label_SetTextColor(g_placeholder, GY_ARGB(0xFF, 0x7F, 0x8A, 0x94));
	label = YMGUI_Creat_Label_Creat(content, 20, 298, 180, 24);
	YMGUI_Label_SetText(label, "ACTIVITY LOG");
	YMGUI_Label_SetTextColor(label, GY_ARGB(0xFF, 0x7F, 0x8A, 0x94));
	g_log_view = YMGUI_Creat_TextView_Creat(content, 20, 326, 536, 82);
	YMGUI_Obj_SetBgColor(g_log_view, GY_ARGB(0xFF, 0x12, 0x16, 0x1A));
	YMGUI_TextView_SetTextColor(g_log_view, GY_ARGB(0xFF, 0xB8, 0xC0, 0xC7));
}

int main(int argc, char** argv)
{
	GYdisp disp = {0};
	char executable_dir[PLUGIN_PATH_MAX];
	const char* plugin_dir;
	int max_frames = argc > 1 ? atoi(argv[1]) : -1;
	int frame = 0;
	int selftest = argc > 3 && strcmp(argv[3], "selftest") == 0;
	int emptytest = argc > 3 && strcmp(argv[3], "emptytest") == 0;
	executableDirectory(argc > 0 ? argv[0] : NULL, executable_dir, sizeof(executable_dir));
	plugin_dir = argc > 2 ? argv[2] : executable_dir;
	scanPlugins(plugin_dir);
	disp.hor_res = SCR_W;
	disp.ver_res = SCR_H;
	disp.buf_px_cnt = SCR_W * BAND_H;
	disp.buf1 = (GYpx*)GY_malloc1(disp.buf_px_cnt * sizeof(GYpx));
	if (disp.buf1 == NULL || SDL_LCD_Init(&disp, 1) != 0)
		return 1;
	g_ctx = YMGUI_Creat_Ctx_Creat(&disp, SCR_W, SCR_H);
	if (g_ctx == NULL)
		return 1;
	buildUi();
	buildHostApi();
	YMGUI_PluginRegistry_Reset();
	YMGUI_Inject_SetCtx(g_ctx);
	updateManagerState();
	hostLog("plugin manager ready");
	if (g_plugin_count > 0)
		loadPlugin();
	if (selftest)
		runSelftest();
	if (emptytest && g_plugin_count != 0)
		g_selftest_fail = 1;
	while (SDL_LCD_PumpEvents())
	{
		YMGUI_PluginRegistry_Tick(33);
		YMGUI_Refresh(g_ctx);
		SDL_LCD_Delay(33);
		if (max_frames > 0 && ++frame >= max_frames)
			break;
	}
	YMGUI_PluginRegistry_UnloadAll();
	g_loaded = 0;
	g_active = -1;
	YMGUI_Inject_SetCtx(NULL);
	YMGUI_Free_CtxFree(g_ctx);
	SDL_LCD_Destroy();
	GY_free1(disp.buf1);
	return g_selftest_fail ? 1 : 0;
}
