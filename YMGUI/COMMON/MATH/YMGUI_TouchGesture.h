#ifndef YMGUI_TOUCH_GESTURE_H
#define YMGUI_TOUCH_GESTURE_H
#include "YMGUI_Hal.h"
static inline uint32 GY_TouchISqrt(uint32 n)
{
	uint32 value = 0, bit = 1u << 30;
	while (bit > n)
		bit >>= 2;
	while (bit)
	{
		if (n >= value + bit)
		{
			n -= value + bit;
			value = (value >> 1) + bit;
		}
		else
			value >>= 1;
		bit >>= 2;
	}
	return value;
}
/* Screen coordinates must have differences within signed 16-bit range. */
static inline uint32 GY_TouchDistance(const GYtouchpoint* a, const GYtouchpoint* b)
{
	int32 dx = (int32)a->x - b->x, dy = (int32)a->y - b->y;
	return GY_TouchISqrt((uint32)(dx * dx) + (uint32)(dy * dy));
}
#endif
