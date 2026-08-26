#include "YMGUI_Plugin.h"
#include "YMGUI_PluginInternal.h"

typedef const YMGUI_Plugin* (*PluginEntryFn)(void);

#if YMGUI_PLUGIN_DYNAMIC && defined(_WIN32)
#include <windows.h>

static void closeLibrary(void* handle)
{
	FreeLibrary((HMODULE)handle);
}

int YMGUI_Plugin_InspectDynamic(const char* path, YMGUI_PluginInfo* info)
{
	HMODULE handle;
	PluginEntryFn entry;
	int rc;
	if (path == NULL || info == NULL)
		return -1;
	handle = LoadLibraryA(path);
	if (handle == NULL)
		return -2;
	entry = (PluginEntryFn)(void*)GetProcAddress(handle, "YMGUI_Plugin_Get");
	if (entry == NULL)
	{
		FreeLibrary(handle);
		return -3;
	}
	rc = YMGUI_Plugin_Validate(entry(), info);
	FreeLibrary(handle);
	return rc == 0 ? 0 : -4;
}

int YMGUI_Plugin_LoadDynamic(const char* path, const YMGUI_PluginHost* host)
{
	HMODULE handle;
	PluginEntryFn entry;
	int rc;
	if (path == NULL || host == NULL)
		return -1;
	handle = LoadLibraryA(path);
	if (handle == NULL)
		return -2;
	entry = (PluginEntryFn)(void*)GetProcAddress(handle, "YMGUI_Plugin_Get");
	if (entry == NULL)
	{
		FreeLibrary(handle);
		return -3;
	}
	rc = YMGUI_PluginRegistry_AddDynamic(entry(), (void*)handle, closeLibrary, host);
	if (rc != 0)
		FreeLibrary(handle);
	return rc;
}

#elif YMGUI_PLUGIN_DYNAMIC && (defined(__unix__) || defined(__APPLE__) || defined(__ANDROID__))
#include <dlfcn.h>

static void closeLibrary(void* handle)
{
	dlclose(handle);
}

int YMGUI_Plugin_InspectDynamic(const char* path, YMGUI_PluginInfo* info)
{
	void* handle;
	PluginEntryFn entry;
	int rc;
	if (path == NULL || info == NULL)
		return -1;
	handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (handle == NULL)
		return -2;
	entry = (PluginEntryFn)dlsym(handle, "YMGUI_Plugin_Get");
	if (entry == NULL)
	{
		dlclose(handle);
		return -3;
	}
	rc = YMGUI_Plugin_Validate(entry(), info);
	dlclose(handle);
	return rc == 0 ? 0 : -4;
}

int YMGUI_Plugin_LoadDynamic(const char* path, const YMGUI_PluginHost* host)
{
	void* handle;
	PluginEntryFn entry;
	int rc;
	if (path == NULL || host == NULL)
		return -1;
	handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (handle == NULL)
		return -2;
	entry = (PluginEntryFn)dlsym(handle, "YMGUI_Plugin_Get");
	if (entry == NULL)
	{
		dlclose(handle);
		return -3;
	}
	rc = YMGUI_PluginRegistry_AddDynamic(entry(), handle, closeLibrary, host);
	if (rc != 0)
		dlclose(handle);
	return rc;
}

#else
int YMGUI_Plugin_InspectDynamic(const char* path, YMGUI_PluginInfo* info)
{
	(void)path;
	(void)info;
	return -1;
}

int YMGUI_Plugin_LoadDynamic(const char* path, const YMGUI_PluginHost* host)
{
	(void)path;
	(void)host;
	return -1;
}
#endif
