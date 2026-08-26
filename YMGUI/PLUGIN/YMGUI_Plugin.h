#ifndef YMGUI_PLUGIN_H
#define YMGUI_PLUGIN_H

#include "YMGUI_Obj.h"

#define YMGUI_PLUGIN_API_VERSION 1u
#define YMGUI_PLUGIN_MAX 16
#define YMGUI_PLUGIN_ID_MAX 31
#define YMGUI_PLUGIN_NAME_MAX 63

#ifndef YMGUI_PLUGIN_DYNAMIC
#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__) || defined(__ANDROID__)
#define YMGUI_PLUGIN_DYNAMIC 1
#else
#define YMGUI_PLUGIN_DYNAMIC 0
#endif
#endif

#if defined(_WIN32)
#define YMGUI_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define YMGUI_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define YMGUI_PLUGIN_EXPORT
#endif

typedef struct YMGUI_PluginHost YMGUI_PluginHost;
typedef struct YMGUI_Plugin YMGUI_Plugin;

typedef struct
{
	char id[YMGUI_PLUGIN_ID_MAX + 1];
	char name[YMGUI_PLUGIN_NAME_MAX + 1];
	uint32 api_version;
} YMGUI_PluginInfo;

typedef void (*YMGUI_PluginClickedFn)(GYOBJ button);
typedef GYOBJ (*YMGUI_PluginCreateObjectFn)(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
typedef GYOBJ (*YMGUI_PluginCreateLabelFn)(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
typedef GYOBJ (*YMGUI_PluginCreateButtonFn)(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h);
typedef void (*YMGUI_PluginDestroyObjectFn)(GYOBJ object);
typedef void (*YMGUI_PluginObjectSetBgFn)(GYOBJ object, GYcolor color);
typedef void (*YMGUI_PluginObjectSetHiddenFn)(GYOBJ object, uint8 hidden);
typedef void (*YMGUI_PluginLabelSetTextFn)(GYOBJ label, const char* text);
typedef void (*YMGUI_PluginLabelSetColorFn)(GYOBJ label, GYcolor color);
typedef void (*YMGUI_PluginLabelSetBgFn)(GYOBJ label, uint8 enable);
typedef void (*YMGUI_PluginButtonSetTextFn)(GYOBJ button, const char* text);
typedef void (*YMGUI_PluginButtonSetColorsFn)(GYOBJ button, GYcolor normal, GYcolor pressed);
typedef void (*YMGUI_PluginButtonSetClickedFn)(GYOBJ button, YMGUI_PluginClickedFn clicked);
typedef void (*YMGUI_PluginLogFn)(const char* text);

struct YMGUI_PluginHost {
	uint32 struct_size;
	uint32 api_version;
	GYCTX ctx;
	GYOBJ root;
	YMGUI_PluginCreateLabelFn create_label;
	YMGUI_PluginDestroyObjectFn destroy_object;
	YMGUI_PluginLabelSetTextFn label_set_text;
	YMGUI_PluginLabelSetColorFn label_set_color;
	YMGUI_PluginLogFn log;
	YMGUI_PluginCreateObjectFn create_object;
	YMGUI_PluginCreateButtonFn create_button;
	YMGUI_PluginObjectSetBgFn object_set_bg;
	YMGUI_PluginObjectSetHiddenFn object_set_hidden;
	YMGUI_PluginLabelSetBgFn label_set_bg;
	YMGUI_PluginButtonSetTextFn button_set_text;
	YMGUI_PluginButtonSetColorsFn button_set_colors;
	YMGUI_PluginButtonSetClickedFn button_set_clicked;
};

struct YMGUI_Plugin {
	uint32 struct_size;
	const char* id;
	const char* name;
	uint32 api_version;
	int  (*init)(const YMGUI_PluginHost* host);
	void (*tick)(uint32 elapsed_ms);
	void (*deinit)(void);
};

#define YMGUI_PLUGIN_HOST_V1_SIZE ((uint32)(offsetof(YMGUI_PluginHost, log) + sizeof(((YMGUI_PluginHost*)0)->log)))
#define YMGUI_PLUGIN_HOST_UI_SIZE ((uint32)(offsetof(YMGUI_PluginHost, button_set_clicked) + sizeof(((YMGUI_PluginHost*)0)->button_set_clicked)))
#define YMGUI_PLUGIN_V1_SIZE ((uint32)(offsetof(YMGUI_Plugin, deinit) + sizeof(((YMGUI_Plugin*)0)->deinit)))

void YMGUI_PluginRegistry_Reset(void);
int  YMGUI_PluginRegistry_Add(const YMGUI_Plugin* plugin);
int  YMGUI_PluginRegistry_LoadAll(const YMGUI_PluginHost* host);
void YMGUI_PluginRegistry_Tick(uint32 elapsed_ms);
void YMGUI_PluginRegistry_UnloadAll(void);

/* Optional OS loader: .so on Linux/Android, .dll on Windows, .dylib on macOS. */
int YMGUI_Plugin_InspectDynamic(const char* path, YMGUI_PluginInfo* info);
int YMGUI_Plugin_LoadDynamic(const char* path, const YMGUI_PluginHost* host);

#endif
