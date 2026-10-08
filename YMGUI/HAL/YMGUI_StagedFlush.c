#include "YMGUI_StagedFlush.h"
#include <string.h>
int YMGUI_StagedFlush(GYbus* bus, const GYrect* a, const GYpx* pixels, GYpx* staging, size_t capacity, GYstaged_send_cb send, void* user)
{
	if (!bus || bus->owner != GY_BUS_GUI || !a || a->w <= 0 || a->h <= 0 || !pixels || !staging || !send || capacity < (size_t)a->w)
		return 0;
	if (bus->io.in_isr && bus->io.in_isr(bus->io.user))
		return 0;
	size_t rows = capacity / (size_t)a->w;
	for (int y = 0; y < a->h;)
	{
		size_t n = (size_t)(a->h - y);
		if (n > rows)
			n = rows;
		memcpy(staging, pixels + (size_t)y * a->w, n * a->w * sizeof(GYpx));
		GYrect chunk = {a->x, a->y + y, a->w, (GYcoord)n};
		if (!YMGUI_Bus_BeginLCD(bus))
			return 0;
		int ok = send(&chunk, staging, y == 0, user);
		if (!YMGUI_Bus_EndLCD(bus) || !ok)
			return 0;
		y += (int)n;
	}
	return 1;
}
