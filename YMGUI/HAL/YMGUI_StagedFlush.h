#ifndef YMGUI_STAGED_FLUSH_H
#define YMGUI_STAGED_FLUSH_H
#include "YMGUI_Hal.h"
#include "YMGUI_Bus.h"
/* Callback sends only caller-provided staging, accessible to its backend. It must wait for completion or
 * abort DMA before returning, including timeout/error. The area is a chunk;
 * first is set only once so ports can keep the controller's write window. */
typedef int (*GYstaged_send_cb)(const GYrect* area, const GYpx* staged_pixels, int first, void* user);
int YMGUI_StagedFlush(GYbus* bus, const GYrect* area, const GYpx* pixels,
					  GYpx* staging, size_t capacity, GYstaged_send_cb send, void* user);
#endif
