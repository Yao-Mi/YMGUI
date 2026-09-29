#include "phone_app.h"
#include <string.h>

#define PHONE_APP(id, file) extern const PhoneApp PhoneApp_##file;
#include PHONE_APP_CATALOG
#undef PHONE_APP

static const PhoneApp* const apps[APP_COUNT] = {
#define PHONE_APP(id, file) &PhoneApp_##file,
#include PHONE_APP_CATALOG
#undef PHONE_APP
};
static GYOBJ views[APP_COUNT];
static uint8 active[APP_COUNT];
static int foreground = -1;

const PhoneApp* PhoneApps_Get(int id)
{
	return id >= 0 && id < APP_COUNT ? apps[id] : NULL;
}

int PhoneApps_Find(const char* key)
{
	if (key)
		for (int i = 0; i < APP_COUNT; ++i)
			if (!strcmp(apps[i]->key, key))
				return i;
	return -1;
}

int PhoneApps_Validate(void)
{
	for (int i = 0; i < APP_COUNT; ++i)
	{
		const PhoneApp* app = apps[i];
		if (!app || !app->key || !app->key[0] || !app->title || !app->title[0] || !app->create)
			return 0;
		for (int j = 0; j < i; ++j)
			if (!strcmp(app->key, apps[j]->key))
				return 0;
	}
	return 1;
}

void PhoneApps_Create(int id, GYOBJ view)
{
	const PhoneApp* app = PhoneApps_Get(id);
	if (!app || !view || views[id])
		return;
	views[id] = view;
	app->create(view);
}

void PhoneApps_Hide(void)
{
	int previous = foreground;
	foreground = -1;
	if (previous >= 0 && apps[previous]->hide)
		apps[previous]->hide();
}

void PhoneApps_SyncRunning(const int* running)
{
	if (!running)
		return;
	for (int i = 0; i < APP_COUNT; ++i)
	{
		if (active[i] && !running[i])
		{
			active[i] = 0;
			if (foreground == i)
				PhoneApps_Hide();
			if (apps[i]->close)
				apps[i]->close();
		}
		active[i] = views[i] && running[i];
	}
}

void PhoneApps_Show(int id, const char* argument)
{
	const PhoneApp* app = PhoneApps_Get(id);
	if (!app || !views[id] || !active[id])
		return;
	if (foreground != id)
		PhoneApps_Hide();
	foreground = id;
	if (app->show)
		app->show(argument);
}

void PhoneApps_Tick(uint32 elapsed)
{
	if (elapsed > 100)
		elapsed = 100;
	for (int i = 0; i < APP_COUNT; ++i)
		if (active[i] && apps[i]->tick)
			apps[i]->tick(elapsed);
}

int PhoneApps_Back(void)
{
	return foreground >= 0 && apps[foreground]->back ? apps[foreground]->back() : 0;
}

void PhoneApps_CloseAll(void)
{
	PhoneApps_Hide();
	for (int i = 0; i < APP_COUNT; ++i)
	{
		int was_active = active[i];
		active[i] = 0;
		if (was_active && apps[i]->close)
			apps[i]->close();
	}
}

void PhoneApps_Destroy(void)
{
	PhoneApps_CloseAll();
	for (int i = APP_COUNT - 1; i >= 0; --i)
	{
		if (!views[i])
			continue;
		if (apps[i]->destroy)
			apps[i]->destroy();
		YMGUI_Free_ObjFree(views[i]);
		views[i] = NULL;
	}
}

int PhoneApps_Command(int id, const char* name, const char* argument)
{
	const PhoneApp* app = PhoneApps_Get(id);
	return app && views[id] && name && app->command ? app->command(name, argument) : 0;
}

int PhoneApps_Query(int id, const char* name)
{
	const PhoneApp* app = PhoneApps_Get(id);
	return app && views[id] && name && app->query ? app->query(name) : 0;
}

intptr_t PhoneApps_Inspect(int id, const char* name, int index)
{
	const PhoneApp* app = PhoneApps_Get(id);
	return app && views[id] && name && app->inspect ? app->inspect(name, index) : 0;
}
