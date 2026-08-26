#include "YMGUI_Plugin.h"
#include "YMGUI_PluginInternal.h"
#include <string.h>

typedef enum
{
	PLUGIN_REGISTERED = 0,
	PLUGIN_ACTIVE,
	PLUGIN_FAILED
} PluginState;

typedef struct
{
	const YMGUI_Plugin* plugin;
	void* handle;
	YMGUI_PluginCloseFn close_fn;
	PluginState state;
} PluginSlot;

static PluginSlot s_slots[YMGUI_PLUGIN_MAX];
static uint32 s_count;

static size_t boundedLength(const char* text, size_t limit)
{
	size_t length = 0;
	while (length < limit && text[length] != '\0')
		length++;
	return length;
}

int YMGUI_Plugin_Validate(const YMGUI_Plugin* plugin, YMGUI_PluginInfo* info)
{
	size_t id_len, name_len;
	if (plugin == NULL || plugin->struct_size < YMGUI_PLUGIN_V1_SIZE ||
		plugin->api_version != YMGUI_PLUGIN_API_VERSION || plugin->id == NULL ||
		plugin->name == NULL || plugin->init == NULL || plugin->deinit == NULL)
		return -1;
	id_len = boundedLength(plugin->id, YMGUI_PLUGIN_ID_MAX + 1);
	name_len = boundedLength(plugin->name, YMGUI_PLUGIN_NAME_MAX + 1);
	if (id_len == 0 || id_len > YMGUI_PLUGIN_ID_MAX ||
		name_len == 0 || name_len > YMGUI_PLUGIN_NAME_MAX)
		return -1;
	for (size_t i = 0; i < id_len; ++i)
	{
		char c = plugin->id[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
			return -1;
	}
	if (info != NULL)
	{
		memcpy(info->id, plugin->id, id_len + 1);
		memcpy(info->name, plugin->name, name_len + 1);
		info->api_version = plugin->api_version;
	}
	return 0;
}

static void removeSlot(uint32 index)
{
	if (index + 1 < s_count)
		memmove(&s_slots[index], &s_slots[index + 1], (s_count - index - 1) * sizeof(PluginSlot));
	s_count--;
	memset(&s_slots[s_count], 0, sizeof(PluginSlot));
}

void YMGUI_PluginRegistry_Reset(void)
{
	YMGUI_PluginRegistry_UnloadAll();
	memset(s_slots, 0, sizeof(s_slots));
	s_count = 0;
}

int YMGUI_PluginRegistry_Add(const YMGUI_Plugin* plugin)
{
	uint32 i;
	if (YMGUI_Plugin_Validate(plugin, NULL) != 0)
		return -1;
	for (i = 0; i < s_count; ++i) if (strcmp(s_slots[i].plugin->id, plugin->id) == 0) return -2;
	if (s_count >= YMGUI_PLUGIN_MAX) return -3;
	s_slots[s_count++].plugin = plugin;
	return 0;
}

int YMGUI_PluginRegistry_LoadAll(const YMGUI_PluginHost* host)
{
	uint32 i;
	int loaded = 0;
	if (host == NULL || host->struct_size < YMGUI_PLUGIN_HOST_V1_SIZE ||
		host->api_version != YMGUI_PLUGIN_API_VERSION)
		return -1;
	for (i = 0; i < s_count; ++i)
	{
		PluginSlot* slot = &s_slots[i];
		if (slot->state != PLUGIN_REGISTERED)
			continue;
		if (slot->plugin->init(host) == 0)
		{
			slot->state = PLUGIN_ACTIVE;
			loaded++;
		}
		else
		{
			/* init may have acquired partial resources; deinit is its rollback hook. */
			if (slot->plugin->deinit != NULL)
				slot->plugin->deinit();
			slot->state = PLUGIN_FAILED;
		}
	}
	return loaded;
}

void YMGUI_PluginRegistry_Tick(uint32 elapsed_ms)
{
	uint32 i;
	for (i = 0; i < s_count; ++i)
		if (s_slots[i].state == PLUGIN_ACTIVE && s_slots[i].plugin->tick != NULL)
			s_slots[i].plugin->tick(elapsed_ms);
}

void YMGUI_PluginRegistry_UnloadAll(void)
{
	uint32 i;
	for (i = s_count; i > 0; --i)
	{
		PluginSlot* slot = &s_slots[i - 1];
		if (slot->state == PLUGIN_ACTIVE && slot->plugin->deinit != NULL)
			slot->plugin->deinit();
		slot->state = PLUGIN_REGISTERED;
		if (slot->handle != NULL && slot->close_fn != NULL)
		{
			void* handle = slot->handle;
			YMGUI_PluginCloseFn close_fn = slot->close_fn;
			removeSlot(i - 1);
			close_fn(handle);
		}
	}
}

int YMGUI_PluginRegistry_AddDynamic(const YMGUI_Plugin* plugin, void* handle,
	YMGUI_PluginCloseFn close_fn, const YMGUI_PluginHost* host)
{
	int rc;
	if (handle == NULL || close_fn == NULL || host == NULL ||
		host->struct_size < YMGUI_PLUGIN_HOST_V1_SIZE ||
		host->api_version != YMGUI_PLUGIN_API_VERSION)
		return -1;
	rc = YMGUI_PluginRegistry_Add(plugin);
	if (rc != 0)
		return -5;
	s_slots[s_count - 1].handle = handle;
	s_slots[s_count - 1].close_fn = close_fn;
	if (plugin->init(host) != 0)
	{
		if (plugin->deinit != NULL)
			plugin->deinit();
		removeSlot(s_count - 1);
		return -6;
	}
	s_slots[s_count - 1].state = PLUGIN_ACTIVE;
	return 0;
}
