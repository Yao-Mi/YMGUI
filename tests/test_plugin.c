#include "YMGUI_Plugin.h"
#include <stdio.h>

static int fails;
static int init_calls;
static int tick_calls;
static int deinit_calls;
static int fail_init;

#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static int testInit(const YMGUI_PluginHost* host)
{
	(void)host;
	init_calls++;
	return fail_init ? -1 : 0;
}

static void testTick(uint32 elapsed_ms)
{
	(void)elapsed_ms;
	tick_calls++;
}

static void testDeinit(void)
{
	deinit_calls++;
}

static const YMGUI_Plugin plugin_a = {
	sizeof(YMGUI_Plugin),
	"plugin-a", "Plugin A", YMGUI_PLUGIN_API_VERSION, testInit, testTick, testDeinit
};

static const YMGUI_Plugin plugin_a_duplicate = {
	sizeof(YMGUI_Plugin),
	"plugin-a", "Duplicate A", YMGUI_PLUGIN_API_VERSION, testInit, testTick, testDeinit
};

static const YMGUI_Plugin bad_version = {
	sizeof(YMGUI_Plugin),
	"bad-version", "Bad Version", YMGUI_PLUGIN_API_VERSION + 1, testInit, testTick, testDeinit
};

static const YMGUI_Plugin bad_size = {
	0,
	"bad-size", "Bad Size", YMGUI_PLUGIN_API_VERSION, testInit, testTick, testDeinit
};

static const YMGUI_Plugin bad_id = {
	sizeof(YMGUI_Plugin),
	"bad/id", "Bad ID", YMGUI_PLUGIN_API_VERSION, testInit, testTick, testDeinit
};

static const YMGUI_Plugin bad_name = {
	sizeof(YMGUI_Plugin),
	"bad-name", "", YMGUI_PLUGIN_API_VERSION, testInit, testTick, testDeinit
};

static const YMGUI_Plugin bad_deinit = {
	sizeof(YMGUI_Plugin),
	"bad-deinit", "Bad Deinit", YMGUI_PLUGIN_API_VERSION, testInit, testTick, NULL
};

static void clearCounters(void)
{
	init_calls = 0;
	tick_calls = 0;
	deinit_calls = 0;
	fail_init = 0;
}

int main(void)
{
	YMGUI_PluginHost host = {0};
	YMGUI_Plugin many[YMGUI_PLUGIN_MAX];
	char ids[YMGUI_PLUGIN_MAX][16];
	uint32 i;
	host.struct_size = sizeof(YMGUI_PluginHost);
	host.api_version = YMGUI_PLUGIN_API_VERSION;

	YMGUI_PluginRegistry_Reset();
	clearCounters();
	CHECK(YMGUI_PluginRegistry_Add(NULL) < 0, "reject NULL plugin");
	CHECK(YMGUI_PluginRegistry_Add(&bad_size) < 0, "reject descriptor size mismatch");
	CHECK(YMGUI_PluginRegistry_Add(&bad_version) < 0, "reject API version mismatch");
	CHECK(YMGUI_PluginRegistry_Add(&bad_id) < 0, "reject unsafe plugin id");
	CHECK(YMGUI_PluginRegistry_Add(&bad_name) < 0, "reject empty plugin name");
	CHECK(YMGUI_PluginRegistry_Add(&bad_deinit) < 0, "reject missing deinit callback");
	CHECK(YMGUI_PluginRegistry_Add(&plugin_a) == 0, "register valid plugin");
	CHECK(YMGUI_PluginRegistry_Add(&plugin_a_duplicate) < 0, "reject duplicate id");
	host.struct_size = 0;
	CHECK(YMGUI_PluginRegistry_LoadAll(&host) < 0, "reject host size mismatch");
	CHECK(init_calls == 0, "invalid host does not initialize plugins");
	host.struct_size = sizeof(YMGUI_PluginHost);
	CHECK(YMGUI_PluginRegistry_LoadAll(&host) == 1, "load registered plugin");
	CHECK(init_calls == 1, "init called once");
	CHECK(YMGUI_PluginRegistry_LoadAll(&host) == 0, "second LoadAll skips active plugin");
	CHECK(init_calls == 1, "active plugin is not initialized twice");
	YMGUI_PluginRegistry_Tick(10);
	CHECK(tick_calls == 1, "tick reaches active plugin");

	YMGUI_PluginRegistry_UnloadAll();
	CHECK(deinit_calls == 1, "unload deinitializes active plugin");
	YMGUI_PluginRegistry_Tick(10);
	CHECK(tick_calls == 1, "tick skips unloaded plugin");
	CHECK(YMGUI_PluginRegistry_LoadAll(&host) == 1, "static plugin can reload after unload");
	CHECK(init_calls == 2, "reload initializes plugin again");
	YMGUI_PluginRegistry_Reset();
	CHECK(deinit_calls == 2, "reset safely unloads active plugin");
	CHECK(YMGUI_PluginRegistry_Add(&plugin_a) == 0, "reset clears registrations");

	YMGUI_PluginRegistry_Reset();
	clearCounters();
	fail_init = 1;
	CHECK(YMGUI_PluginRegistry_Add(&plugin_a) == 0, "register plugin for failure test");
	CHECK(YMGUI_PluginRegistry_LoadAll(&host) == 0, "failed init is not counted as loaded");
	CHECK(init_calls == 1, "failed init called once");
	CHECK(deinit_calls == 1, "failed init invokes rollback deinit");
	YMGUI_PluginRegistry_Tick(10);
	CHECK(tick_calls == 0, "failed plugin never ticks");
	CHECK(YMGUI_PluginRegistry_LoadAll(&host) == 0 && init_calls == 1, "failed plugin is not retried implicitly");
	YMGUI_PluginRegistry_UnloadAll();
	fail_init = 0;
	CHECK(YMGUI_PluginRegistry_LoadAll(&host) == 1, "explicit unload allows retry after failure");
	YMGUI_PluginRegistry_Reset();

	clearCounters();
	for (i = 0; i < YMGUI_PLUGIN_MAX; ++i)
	{
		sprintf(ids[i], "capacity-%lu", (unsigned long)i);
		many[i] = plugin_a;
		many[i].id = ids[i];
		CHECK(YMGUI_PluginRegistry_Add(&many[i]) == 0, "register plugin within capacity");
	}
	CHECK(YMGUI_PluginRegistry_Add(&plugin_a) < 0, "reject plugin beyond capacity");
	YMGUI_PluginRegistry_Reset();

	if (fails == 0)
		printf("test_plugin: all passed\n");
	return fails == 0 ? 0 : 1;
}
