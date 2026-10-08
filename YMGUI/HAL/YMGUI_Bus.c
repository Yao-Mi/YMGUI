#include "YMGUI_Bus.h"
#include <string.h>
int YMGUI_Bus_Init(GYbus* b, const GYbus_io* io)
{
	if (!b || (io && (!!io->enter != !!io->leave)))
		return 0;
	memset(b, 0, sizeof(*b));
	if (io)
		b->io = *io;
	return 1;
}
static int operation(GYbus* b, GYbus_owner owner, int op)
{
	if (!b)
		return 0;
	uint32 saved = b->io.enter ? b->io.enter(b->io.user) : 0;
	int isr = b->io.in_isr ? b->io.in_isr(b->io.user) : 0, ok = 0;
	if (op == 0)
	{
		if (owner != GY_BUS_GUI && owner != GY_BUS_WORKER)
			++b->violations;
		else if (isr || b->owner != GY_BUS_IDLE)
			++b->deferred;
		else
		{
			b->owner = owner;
			++b->acquisitions;
			ok = 1;
		}
	}
	else if (op == 1)
	{
		if (!isr && (owner == GY_BUS_GUI || owner == GY_BUS_WORKER) && b->owner == owner)
		{
			b->owner = GY_BUS_IDLE;
			ok = 1;
		}
		else
			++b->violations;
	}
	else if (op == 2)
	{
		if (!isr && b->owner == GY_BUS_GUI)
		{
			b->owner = GY_BUS_LCD;
			++b->transfers;
			ok = 1;
		}
		else
			++b->violations;
	}
	else
	{
		if (b->owner == GY_BUS_LCD)
		{
			b->owner = GY_BUS_GUI;
			ok = 1;
		}
		else
			++b->violations;
	}
	if (b->io.leave)
		b->io.leave(saved, b->io.user);
	return ok;
}
int YMGUI_Bus_Try(GYbus* b, GYbus_owner o)
{
	return operation(b, o, 0);
}
int YMGUI_Bus_Release(GYbus* b, GYbus_owner o)
{
	return operation(b, o, 1);
}
int YMGUI_Bus_BeginLCD(GYbus* b)
{
	return operation(b, GY_BUS_LCD, 2);
}
int YMGUI_Bus_EndLCD(GYbus* b)
{
	return operation(b, GY_BUS_LCD, 3);
}
