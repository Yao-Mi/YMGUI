#include "phone_quick.h"
#include <stdio.h>
#define PHONE_QUICK(name, source) extern const PhoneQuick PhoneQuick_##name;
#include "phone_quick_catalog.def"
#undef PHONE_QUICK
int PhoneQuick_RegisterDefaults(void)
{
	static int initialized;
	static int valid = 1;
	if (initialized)
		return valid;
	initialized = 1;
#define PHONE_QUICK(name, source)                              \
	if (!PhoneQuick_Register(&PhoneQuick_##name))              \
	{                                                          \
		fprintf(stderr, "Invalid quick setting: %s\n", #name); \
		valid = 0;                                             \
	}
#include "phone_quick_catalog.def"
#undef PHONE_QUICK
	return valid;
}
