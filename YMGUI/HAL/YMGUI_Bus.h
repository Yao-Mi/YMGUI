#ifndef YMGUI_BUS_H
#define YMGUI_BUS_H
#include "YMGUI_PubType.h"
typedef enum
{
	GY_BUS_IDLE,
	GY_BUS_GUI,
	GY_BUS_WORKER,
	GY_BUS_LCD
} GYbus_owner;
typedef struct
{
	void* user;
	uint32 (*enter)(void* user);
	void (*leave)(uint32 saved, void* user);
	int (*in_isr)(void* user);
} GYbus_io;
typedef struct
{
	GYbus_io io;
	GYbus_owner owner;
	uint32 acquisitions, deferred, violations, transfers;
} GYbus;
/* No callbacks is suitable only for cooperative single-thread use. */
int YMGUI_Bus_Init(GYbus* bus, const GYbus_io* io);
int YMGUI_Bus_Try(GYbus* bus, GYbus_owner owner);
int YMGUI_Bus_Release(GYbus* bus, GYbus_owner owner);
int YMGUI_Bus_BeginLCD(GYbus* bus);
/* May be called after DMA completion from an ISR. Restores GUI ownership. */
int YMGUI_Bus_EndLCD(GYbus* bus);
#endif
