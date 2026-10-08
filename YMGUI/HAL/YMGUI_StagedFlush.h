#ifndef YMGUI_STAGED_FLUSH_H
#define YMGUI_STAGED_FLUSH_H
#include "YMGUI_Hal.h"
#include "YMGUI_Bus.h"
/* Callback sends pixels from a backend-accessible buffer. StagedFlush uses
 * caller-provided staging; Auto may pass the original source directly. It must wait for completion or
 * abort DMA before returning, including timeout/error. The area is a chunk;
 * first is set only once so ports can keep the controller's write window. */
typedef int (*GYstaged_send_cb)(const GYrect* area, const GYpx* staged_pixels, int first, void* user);
int YMGUI_StagedFlush(GYbus* bus, const GYrect* area, const GYpx* pixels,
					  GYpx* staging, size_t capacity, GYstaged_send_cb send, void* user);
/* Backend decides whether it can synchronously send this source directly.
 * Called under GUI ownership, never in an ISR. No hardware address assumptions.
 * If accepted, send receives the whole compact area without staging; its
 * completion/abort contract and source lifetime are the same as above. */
typedef int (*GYstaged_access_cb)(const GYrect* area, const GYpx* pixels, void* user);
int YMGUI_StagedFlushAuto(GYbus* bus, const GYrect* area, const GYpx* pixels,
						  GYpx* staging, size_t capacity, GYstaged_access_cb accessible,
						  GYstaged_send_cb send, void* user);
#endif
