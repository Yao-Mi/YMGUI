#include "phone_quick.h"
#include <string.h>
static const PhoneQuick* entries[PHONE_QUICK_CAPACITY];
static int count, locked;
int PhoneQuick_Register(const PhoneQuick* feature)
{
	if (locked || count == PHONE_QUICK_CAPACITY || !feature || !feature->key || !feature->key[0] ||
		!feature->title || !feature->title[0] || !feature->activate || PhoneQuick_Find(feature->key))
		return 0;
	entries[count++] = feature;
	return 1;
}
int PhoneQuick_Count(void)
{
	return count;
}
const PhoneQuick* PhoneQuick_Get(int index)
{
	return index >= 0 && index < count ? entries[index] : NULL;
}
const PhoneQuick* PhoneQuick_Find(const char* key)
{
	if (key)
		for (int i = 0; i < count; ++i)
			if (!strcmp(key, entries[i]->key))
				return entries[i];
	return NULL;
}
void PhoneQuick_Lock(void)
{
	locked = 1;
}
int PhoneQuick_Place(int index, GYrect* slot)
{
	if (index < 0 || index >= count || !slot)
		return -1;
	*slot = (GYrect){14 + index % 4 * 76, 90 + index % 8 / 4 * 69, 66, 62};
	return index / 8;
}
