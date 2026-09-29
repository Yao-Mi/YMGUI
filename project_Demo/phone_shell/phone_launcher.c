#include "phone_launcher.h"

void PhoneLauncher_Reset(PhoneLauncherModel* model, int* storage, int capacity)
{
	if (!model || !storage || capacity < 0)
		return;
	*model = (PhoneLauncherModel){storage, capacity, capacity};
	for (int i = 0; i < capacity; ++i)
		storage[i] = i;
}
int PhoneLauncher_Index(const PhoneLauncherModel* model, int app_id)
{
	if (model)
		for (int i = 0; i < model->count; ++i)
			if (model->order[i] == app_id)
				return i;
	return -1;
}
int PhoneLauncher_Move(PhoneLauncherModel* model, int app_id, int index)
{
	int from = PhoneLauncher_Index(model, app_id);
	if (from < 0 || index < 0 || index >= model->count)
		return 0;
	for (int i = from; i < index; ++i)
		model->order[i] = model->order[i + 1];
	for (int i = from; i > index; --i)
		model->order[i] = model->order[i - 1];
	model->order[index] = app_id;
	return 1;
}
int PhoneLauncher_Remove(PhoneLauncherModel* model, int app_id)
{
	int index = PhoneLauncher_Index(model, app_id);
	if (index < 0)
		return 0;
	for (int i = index; i < model->count - 1; ++i)
		model->order[i] = model->order[i + 1];
	model->order[--model->count] = -1;
	return 1;
}

int PhoneLauncher_Place(int index, int count, PhoneLauncherSlot* slot)
{
	if (!slot || index < 0 || index >= count)
		return 0;
	int local = index;
	slot->page = 0;
	if (index >= PHONE_LAUNCHER_FIRST_CAPACITY)
	{
		local = index - PHONE_LAUNCHER_FIRST_CAPACITY;
		slot->page = 1 + local / PHONE_LAUNCHER_PAGE_CAPACITY;
		local %= PHONE_LAUNCHER_PAGE_CAPACITY;
	}
	slot->x = 23 + (local % 4) * 75;
	slot->y = slot->page == 0 ? 238 : 28 + (local / 4) * 110;
	return 1;
}
