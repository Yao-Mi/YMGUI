#ifndef YMGUI_PLUGIN_INTERNAL_H
#define YMGUI_PLUGIN_INTERNAL_H

#include "YMGUI_Plugin.h"

typedef void (*YMGUI_PluginCloseFn)(void* handle);

int YMGUI_Plugin_Validate(const YMGUI_Plugin* plugin, YMGUI_PluginInfo* info);

int YMGUI_PluginRegistry_AddDynamic(const YMGUI_Plugin* plugin, void* handle,
	YMGUI_PluginCloseFn close_fn, const YMGUI_PluginHost* host);

#endif
