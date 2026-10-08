#ifndef PHONE_RUNTIME_H
#define PHONE_RUNTIME_H
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
typedef struct
{
	void* user;
	void (*refresh)(GYCTX ctx, void* user);
	void (*transition)(int active, void* user);
	unsigned merge_transitions;
	void (*cursor)(int x, int y, void* user);
} PhoneRuntimeConfig;
/* Display and its buffers remain caller-owned until Shutdown. Singleton. */
int PhoneShell_Init(GYDISP display, const PhoneRuntimeConfig* config);
void PhoneShell_Step(uint32 elapsed_ms);
/* For ports whose input poll already injects Tick: consume ctx->tick_elapsed. */
void PhoneShell_Pump(void);
/* key 1: select/long Back; key 2: click/long Home. */
void PhoneShell_Key(int key, int long_press);
void PhoneShell_Shutdown(void);
GYCTX PhoneShell_Context(void);
#endif
