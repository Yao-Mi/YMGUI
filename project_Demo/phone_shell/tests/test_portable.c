#ifdef NDEBUG
#undef NDEBUG
#endif
#include "phone_runtime.h"
#include "phone_host.h"
#include "phone_app.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_TextActions.h"
#include "YMGUI_Event.h"
#include "YMGUI_Invalidate.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static GYpx frame[320 * 480], reference[320 * 480];
static unsigned flushes;
static void flush(GYdisp* d, const GYrect* a, const GYpx* pixels)
{
	(void)d;
	assert(a->x >= 0 && a->y >= 0 && a->x + a->w <= 320 && a->y + a->h <= 480);
	for (int y = 0; y < a->h; ++y)
		memcpy(frame + (a->y + y) * 320 + a->x, pixels + y * a->w, a->w * sizeof(GYpx));
	++flushes;
}
static void step(unsigned n)
{
	while (n--)
		PhoneShell_Step(16);
}
int main(void)
{
	GYdisp display = {0};
	display.hor_res = 320;
	display.ver_res = 480;
	display.buf_px_cnt = 320 * 48;
	display.buf1 = calloc(display.buf_px_cnt, sizeof(GYpx));
	display.flush_cb = flush;
	assert(display.buf1);
	PhoneRuntimeConfig config = {0};
	config.merge_transitions = 1;
	for (int cycle = 0; cycle < 3; ++cycle)
	{
		assert(PhoneShell_Init(&display, &config));
		assert(!PhoneShell_Init(&display, &config));
		step(100);
		assert(flushes);
		assert(PhoneHost_Open(GALLERY, NULL));
		step(100);
		GYtouchpoint contacts[2] = {{80, 180, 2}, {180, 180, 7}};
		YMGUI_Inject_Touch(contacts, 2);
		contacts[1].x = 240;
		YMGUI_Inject_Touch(contacts, 2);
		assert(PhoneApps_Inspect(GALLERY, "gallery_zoom", 0) > 100);
		YMGUI_Inject_Touch(NULL, 0);
		step(1);
		memcpy(reference, frame, sizeof(frame));
		YMGUI_Obj_Invalidate(PhoneShell_Context()->root);
		PhoneShell_Step(0);
		assert(!memcmp(frame, reference, sizeof(frame)));
		GYCTX c = PhoneShell_Context();
		GYOBJ edit = YMGUI_Creat_TextInput_Creat(c->top_layer, 0, 100, 180, 32, 128);
		assert(edit);
		YMGUI_TextInput_SetText(edit, "hello");
		YMGUI_SetFocus(c, edit);
		edit->state |= GY_STATE_Editing;
		YMGUI_TextActions_Update(c, 448, 1);
		YMGUI_TextInput_SelectAll(edit);
		YMGUI_TextActions_Event(edit, GY_EVENT_ContextRequested, 0);
		GYOBJ menu = YMGUI_TextActions_GetMenu(c);
		assert(menu && !(menu->state & GY_STATE_Hidden));
		YMGUI_Free_ObjFree(edit);
		assert(menu->state & GY_STATE_Hidden);
		PhoneShell_Shutdown();
		assert(!PhoneShell_Context());
		assert(display.flush_cb == flush);
		PhoneShell_Shutdown();
	}
	free(display.buf1);
	puts("phone_portable: ALL PASS");
	return 0;
}
