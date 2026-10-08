#ifndef YMGUI_ARENA_H
#define YMGUI_ARENA_H
#include <stddef.h>
/* Caller-owned storage; no global state. External memory must be accessible
 * for every call. Serialize calls and never use the heap from an ISR. */
typedef struct
{
	void* head;
	size_t used, peak;
} GYarena;
int YMGUI_Arena_Init(GYarena* arena, void* storage, size_t size);
void* YMGUI_Arena_Alloc(GYarena* arena, size_t size);
void YMGUI_Arena_Free(GYarena* arena, void* ptr);
void* YMGUI_Arena_Realloc(GYarena* arena, void* ptr, size_t size);
size_t YMGUI_Arena_LargestFree(const GYarena* arena);
#endif
