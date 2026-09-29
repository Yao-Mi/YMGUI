#include "phone_app.h"
#include "phone_quick.h"
#include "YMGUI_Hal.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(condition)                                                  \
	do                                                                    \
	{                                                                     \
		if (!(condition))                                                 \
		{                                                                 \
			printf("registry failure at %d: %s\n", __LINE__, #condition); \
			++failures;                                                   \
		}                                                                 \
	} while (0)

typedef struct
{
	int create, show, hide, tick, close, destroy, freed, back, value;
	char argument[32];
} Probe;
static Probe alpha, beta;
static int quick_hits;
static void quick_activate(void)
{
	++quick_hits;
}
static void test_quick_registry(void)
{
	static PhoneQuick features[33];
	static char keys[33][16];
	CHECK(!PhoneQuick_Register(NULL));
	features[0] = (PhoneQuick){"invalid", "", NULL, quick_activate, NULL};
	CHECK(!PhoneQuick_Register(&features[0]));
	for (int i = 0; i < 32; ++i)
	{
		snprintf(keys[i], sizeof(keys[i]), "feature-%02d", 31 - i);
		features[i] = (PhoneQuick){keys[i], "测试", NULL, quick_activate, NULL};
		CHECK(PhoneQuick_Register(&features[i]));
		CHECK(PhoneQuick_Get(i) == &features[i]); /* 注册顺序，不按 key 排序。 */
		CHECK(!PhoneQuick_Register(&features[i]));
		GYrect slot;
		CHECK(PhoneQuick_Place(i, &slot) == i / 8);
		CHECK(slot.x == 14 + i % 4 * 76 && slot.y == 90 + i % 8 / 4 * 69);
		CHECK(slot.x + slot.w <= 320 && slot.y + slot.h <= 221);
	}
	CHECK(PhoneQuick_Find("feature-31") == &features[0]);
	PhoneQuick_Get(3)->activate();
	CHECK(quick_hits == 1 && !PhoneQuick_Get(-1) && !PhoneQuick_Get(32));
	CHECK(!PhoneQuick_Find(NULL) && !PhoneQuick_Find("missing"));
	features[32] = (PhoneQuick){"overflow", "测试", NULL, quick_activate, NULL};
	CHECK(!PhoneQuick_Register(&features[32]));
	PhoneQuick_Lock();
	CHECK(!PhoneQuick_Register(&features[32]));
	GYrect slot;
	CHECK(PhoneQuick_Place(-1, &slot) == -1 && PhoneQuick_Place(32, &slot) == -1 && PhoneQuick_Place(0, NULL) == -1);
}

#define PROBE(name)                                                           \
	static void name##_free(GYOBJ view)                                       \
	{                                                                         \
		(void)view;                                                           \
		++name.freed;                                                         \
	}                                                                         \
	static void name##_create(GYOBJ view)                                     \
	{                                                                         \
		++name.create;                                                        \
		view->free_cb = name##_free;                                          \
	}                                                                         \
	static void name##_show(const char* arg)                                  \
	{                                                                         \
		++name.show;                                                          \
		snprintf(name.argument, sizeof(name.argument), "%s", arg ? arg : ""); \
	}                                                                         \
	static void name##_hide(void)                                             \
	{                                                                         \
		++name.hide;                                                          \
	}                                                                         \
	static void name##_tick(uint32 elapsed)                                   \
	{                                                                         \
		name.tick += (int)elapsed;                                            \
	}                                                                         \
	static int name##_back(void)                                              \
	{                                                                         \
		++name.back;                                                          \
		return 1;                                                             \
	}                                                                         \
	static void name##_close(void)                                            \
	{                                                                         \
		++name.close;                                                         \
	}                                                                         \
	static void name##_destroy(void)                                          \
	{                                                                         \
		CHECK(name.freed == 0);                                               \
		++name.destroy;                                                       \
	}                                                                         \
	static int name##_command(const char* command, const char* arg)           \
	{                                                                         \
		(void)arg;                                                            \
		if (strcmp(command, "increment"))                                     \
			return 0;                                                         \
		++name.value;                                                         \
		return 1;                                                             \
	}                                                                         \
	static int name##_query(const char* query)                                \
	{                                                                         \
		return !strcmp(query, "value") ? name.value : 0;                      \
	}                                                                         \
	const PhoneApp PhoneApp_##name = {                                        \
		.key = #name, .title = #name, .create = name##_create, .show = name##_show, .hide = name##_hide, .tick = name##_tick, .back = name##_back, .close = name##_close, .destroy = name##_destroy, .command = name##_command, .query = name##_query};

PROBE(alpha)
PROBE(beta)

int main(void)
{
	test_quick_registry();
	GYdisp disp = {0};
	GYCTX ctx = YMGUI_Creat_Ctx_Creat(&disp, 320, 480);
	if (!ctx)
		return 1;
	CHECK(PhoneApps_Validate());
	CHECK(PhoneApps_Find("alpha") == ALPHA && PhoneApps_Find("beta") == BETA);
	CHECK(PhoneApps_Find(NULL) == -1 && PhoneApps_Find("missing") == -1);
	CHECK(!PhoneApps_Get(-1) && !PhoneApps_Get(APP_COUNT));
	CHECK(!PhoneApps_Back() && !PhoneApps_Query(ALPHA, "value"));
	CHECK(!PhoneApps_Command(ALPHA, "increment", NULL));
	PhoneApps_Show(ALPHA, "not created");
	CHECK(alpha.show == 0);
	GYOBJ a = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, 320, 400);
	GYOBJ b = YMGUI_Creat_Obj_Creat(ctx->root, 0, 0, 320, 400);
	PhoneApps_Create(ALPHA, a);
	PhoneApps_Create(ALPHA, a);
	PhoneApps_Create(BETA, b);
	CHECK(alpha.create == 1 && beta.create == 1);
	PhoneApps_Show(ALPHA, "not running");
	CHECK(alpha.show == 0);
	int running[APP_COUNT] = {1, 0};
	PhoneApps_SyncRunning(running);
	PhoneApps_Show(ALPHA, "payload");
	CHECK(alpha.show == 1 && !strcmp(alpha.argument, "payload"));
	PhoneApps_Tick(30);
	CHECK(alpha.tick == 30 && beta.tick == 0);
	PhoneApps_Hide();
	PhoneApps_Hide();
	CHECK(alpha.hide == 1 && !PhoneApps_Back());
	PhoneApps_Tick(30);
	CHECK(alpha.tick == 60); /* 回桌面仍运行。 */
	PhoneApps_Show(ALPHA, NULL);
	running[BETA] = 1;
	PhoneApps_SyncRunning(running);
	PhoneApps_Show(BETA, "message");
	CHECK(alpha.hide == 2 && beta.show == 1 && PhoneApps_Back() && beta.back == 1);
	CHECK(PhoneApps_Command(ALPHA, "increment", NULL));
	CHECK(PhoneApps_Query(ALPHA, "value") == 1);
	CHECK(!PhoneApps_Command(ALPHA, "unknown", NULL) && !PhoneApps_Command(ALPHA, NULL, NULL));
	CHECK(!PhoneApps_Command(-1, "increment", NULL) && !PhoneApps_Inspect(ALPHA, "none", 0));
	PhoneApps_Tick(999);
	CHECK(alpha.tick == 160 && beta.tick == 100); /* 大时间片钳制一致。 */
	running[BETA] = 0;
	PhoneApps_SyncRunning(running);
	PhoneApps_SyncRunning(running);
	CHECK(beta.hide == 1 && beta.close == 1 && !PhoneApps_Back());
	PhoneApps_CloseAll();
	PhoneApps_CloseAll();
	PhoneApps_Tick(100);
	CHECK(alpha.close == 1 && beta.close == 1 && alpha.tick == 160 && beta.tick == 100);
	CHECK(PhoneApps_Query(ALPHA, "value") == 1); /* 关闭不销毁会话内容。 */
	PhoneApps_SyncRunning(running);
	PhoneApps_Show(ALPHA, "reopen");
	CHECK(alpha.create == 1 && alpha.show == 3 && !strcmp(alpha.argument, "reopen"));
	PhoneApps_Destroy();
	PhoneApps_Destroy();
	CHECK(alpha.hide == 3 && alpha.close == 2 && beta.close == 1);
	CHECK(alpha.destroy == 1 && beta.destroy == 1 && alpha.freed == 1 && beta.freed == 1);
	CHECK(!PhoneApps_Query(ALPHA, "value") && !PhoneApps_Command(ALPHA, "increment", NULL));
	CHECK(!ctx->root->child_head);
	YMGUI_Free_CtxFree(ctx);
	printf("phone app registry: %s\n", failures ? "FAIL" : "OK");
	return failures ? 1 : 0;
}
